/*
 * Copyright (c) 2016-2018 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <zubax_chibios/os.hpp>
#include <zubax_chibios/util/helpers.hpp>
#include <cstdint>
#include <utility>
#include <array>
#include <cassert>
#include <optional>
#include "util.hpp"


namespace os
{
namespace bootloader
{
/**
 * Bootloader states. Some of the states are designed as commands to the outer logic, e.g. @ref ReadyToBoot
 * means that the application should be started.
 */
enum class State
{
    NoAppToBoot,
    BootDelay,
    BootCancelled,
    AppUpgradeInProgress,
    ReadyToBoot
};

static inline const char* stateToString(State state)
{
    switch (state)
    {
    case State::NoAppToBoot:            return "NoAppToBoot";
    case State::BootDelay:              return "BootDelay";
    case State::BootCancelled:          return "BootCancelled";
    case State::AppUpgradeInProgress:   return "AppUpgradeInProgress";
    case State::ReadyToBoot:            return "ReadyToBoot";
    default: return "INVALID_STATE";
    }
}

/**
 * These fields are defined by the Brickproof Bootloader specification.
 */
struct __attribute__((packed)) AppInfo
{
    std::uint64_t image_crc = 0;
    std::uint32_t image_size = 0;
    std::uint32_t vcs_commit = 0;
    std::uint8_t major_version = 0;
    std::uint8_t minor_version = 0;
};

/**
 * This interface abstracts the target-specific ROM routines.
 * Upgrade scenario:
 *  1. beginUpgrade()
 *  2. write() repeated until finished.
 *  3. endUpgrade(success or not)
 *
 * Please note that the performance of the ROM reading routine is critical.
 * Slow access may lead to watchdog timeouts (assuming that the watchdog is used),
 * disruption of communications, and premature expiration of the boot timeout.
 */
class IAppStorageBackend
{
public:
    virtual ~IAppStorageBackend() { }

    /**
     * @return 0 on success, negative on error
     */
    virtual int beginUpgrade() = 0;

    /**
     * @return number of bytes written; negative on error
     */
    virtual int write(std::size_t offset, const void* data, std::size_t size) = 0;

    /**
     * @return 0 on success, negative on error
     */
    virtual int endUpgrade(bool success) = 0;

    /**
     * @return number of bytes read; negative on error
     */
    virtual int read(std::size_t offset, void* data, std::size_t size) const = 0;
};

/**
 * This interface proxies data received by the downloader into the bootloader.
 */
class IDownloadStreamSink
{
public:
    virtual ~IDownloadStreamSink() { }

    /**
     * @return Negative on error, non-negative on success.
     */
    virtual int handleNextDataChunk(const void* data, std::size_t size) = 0;
};

/**
 * Inherit this class to implement firmware loading protocol, from remote to the local storage.
 */
class IDownloader
{
public:
    virtual ~IDownloader() { }

    /**
     * Performs the download operation synchronously.
     * Every received data chunk is fed into the sink using the corresponding method (refer to the interface
     * definition). If the sink returns error, downloading will be aborted.
     * @return Negative on error, 0 on success.
     */
    virtual int download(IDownloadStreamSink& sink) = 0;
};

/**
 * Main bootloader controller.
 * Beware that this class has a large buffer field used to cache ROM reads. Do not allocate it on the stack.
 */
class Bootloader
{
    /**
     * A proxy that streams the data from the downloader into the application storage.
     * Note that every access to the storage backend is protected with the mutex!
     */
    class Sink : public IDownloadStreamSink
    {
        IAppStorageBackend& backend_;
        chibios_rt::Mutex& mutex_;
        const std::size_t max_image_size_;
        std::size_t offset_ = 0;

        int handleNextDataChunk(const void* data, std::size_t size) override
        {
            os::MutexLocker mlock(mutex_);

            if ((offset_ + size) <= max_image_size_)
            {
                int res = backend_.write(offset_, data, size);
                if ((res >= 0) && (res != int(size)))
                {
                    return -ErrAppStorageWriteFailure;
                }

                offset_ += size;
                return res;
            }
            else
            {
                return -ErrAppImageTooLarge;
            }
        }

    public:
        Sink(IAppStorageBackend& back,
             chibios_rt::Mutex& mutex,
             std::size_t max_image_size) :
            backend_(back),
            mutex_(mutex),
            max_image_size_(max_image_size)
        { }
    };

    State state_;
    IAppStorageBackend& backend_;

    const std::uint32_t max_application_image_size_;
    const unsigned boot_delay_msec_;
    ::systime_t boot_delay_started_at_st_;

    std::uint8_t rom_buffer_[1024];             ///< Larger buffer enables faster CRC verification, which is important

    chibios_rt::Mutex mutex_;

    /// Caching is needed because app check can sometimes take a very long time (several seconds)
    std::optional<AppInfo> cached_app_info_;

    /**
     * Refer to the Brickproof Bootloader specs.
     * Note that the structure must be aligned at 8 bytes boundary, and the image must be padded to 8 bytes!
     */
    struct __attribute__((packed)) AppDescriptor
    {
        static constexpr unsigned ImagePaddingBytes = 8;

        std::array<std::uint8_t, 8> signature;
        AppInfo app_info;
        std::array<std::uint8_t, 6> reserved;

        static constexpr std::array<std::uint8_t, 8> getSignatureValue()
        {
            return {'A','P','D','e','s','c','0','0'};
        }

        bool isValid(const std::uint32_t max_application_image_size) const
        {
            const auto sgn = getSignatureValue();
            return std::equal(std::begin(signature), std::end(signature), std::begin(sgn)) &&
                   (app_info.image_size > 0) &&
                   (app_info.image_size <= max_application_image_size) &&
                   ((app_info.image_size % ImagePaddingBytes) == 0);
        }
    };
    static_assert(sizeof(AppDescriptor) == 32, "Invalid packing");

    std::pair<AppDescriptor, bool> locateAppDescriptor()
    {
        constexpr auto Step = 8;

        for (std::size_t offset = 0;; offset += Step)
        {
            // Reading the storage in 8 bytes increments until we've found the signature
            {
                std::uint8_t signature[Step] = {};
                int res = backend_.read(offset, signature, sizeof(signature));
                if (res != sizeof(signature))
                {
                    break;
                }
                const auto reference = AppDescriptor::getSignatureValue();
                if (!std::equal(std::begin(signature), std::end(signature), std::begin(reference)))
                {
                    continue;
                }
            }

            // Reading the entire descriptor
            AppDescriptor desc;
            {
                int res = backend_.read(offset, &desc, sizeof(desc));
                if (res != sizeof(desc))
                {
                    break;
                }
                if (!desc.isValid(max_application_image_size_))
                {
                    continue;
                }
            }

            // Checking firmware CRC.
            // This block is very computationally intensive, so it has been carefully optimized for speed.
            {
                const auto crc_offset = offset + offsetof(AppDescriptor, app_info.image_crc);
                CRC64WE crc;

                // Read large chunks until the CRC field is reached (in most cases it will fit in just one chunk)
                for (std::size_t i = 0; i < crc_offset;)
                {
                    const int res = backend_.read(i, rom_buffer_,
                                                  std::min<std::size_t>(sizeof(rom_buffer_), crc_offset - i));
                    if LIKELY(res > 0)
                    {
                        i += res;
                        crc.add(rom_buffer_, res);
                    }
                    else
                    {
                        break;
                    }
                }

                // Fill CRC with zero
                {
                    static const std::uint8_t dummy[8]{0};
                    crc.add(&dummy[0], sizeof(dummy));
                }

                // Read the rest of the image in large chunks
                for (std::size_t i = crc_offset + 8; i < desc.app_info.image_size;)
                {
                    const int res = backend_.read(i, rom_buffer_,
                                                  std::min<std::size_t>(sizeof(rom_buffer_), desc.app_info.image_size - i));
                    if LIKELY(res > 0)
                    {
                        i += res;
                        crc.add(rom_buffer_, res);
                    }
                    else
                    {
                        break;
                    }
                }

                if (crc.get() != desc.app_info.image_crc)
                {
                    DEBUG_LOG("App descriptor found, but CRC is invalid\n");
                    continue;       // Look further...
                }
            }

            // Returning if the descriptor is correct
            DEBUG_LOG("App descriptor located at offset %x\n", unsigned(offset));
            return {desc, true};
        }

        return {AppDescriptor(), false};
    }

    void verifyAppAndUpdateState(const State state_on_success)
    {
        const auto appdesc_result = locateAppDescriptor();

        if (appdesc_result.second)
        {
            cached_app_info_ = appdesc_result.first.app_info;
            state_ = state_on_success;

            boot_delay_started_at_st_ = chVTGetSystemTime();        // This only makes sense if the new state is BootDelay

            DEBUG_LOG("App found; version %d.%d.%x, %d bytes\n",
                      appdesc_result.first.app_info.major_version,
                      appdesc_result.first.app_info.minor_version,
                      unsigned(appdesc_result.first.app_info.vcs_commit),
                      unsigned(appdesc_result.first.app_info.image_size));
        }
        else
        {
            cached_app_info_.reset();
            state_ = State::NoAppToBoot;

            DEBUG_LOG("App not found\n");
        }
    }

public:
    /**
     * Time since boot will be measured starting from the moment when the object was constructed.
     *
     * The max application image size parameter is very important for performance reasons;
     * without it, the bootloader may encounter an unrelated data structure in the ROM that looks like a
     * valid app descriptor (by virtue of having the same signature, which is only 64 bit long),
     * and it may spend a considerable amount of time trying to check the CRC that is certainly invalid.
     * Having an upper size limit for the application image allows the bootloader to weed out too large
     * values early, greatly improving robustness.
     *
     * By default, the boot delay is set to zero; i.e. if the application is valid it will be launched immediately.
     */
    Bootloader(IAppStorageBackend& backend,
               std::uint32_t max_application_image_size = 0xFFFFFFFFU,
               unsigned boot_delay_msec = 0) :
        backend_(backend),
        max_application_image_size_(max_application_image_size),
        boot_delay_msec_(boot_delay_msec)
    {
        os::MutexLocker mlock(mutex_);
        verifyAppAndUpdateState(State::BootDelay);
    }

    /**
     * @ref State.
     */
    State getState()
    {
        os::MutexLocker mlock(mutex_);

        if ((state_ == State::BootDelay) &&
            (chVTTimeElapsedSinceX(boot_delay_started_at_st_) >= TIME_MS2I(boot_delay_msec_)))
        {
            DEBUG_LOG("Boot delay expired\n");
            state_ = State::ReadyToBoot;
        }

        return state_;
    }

    /**
     * Returns info about the application, if any.
     * @return First component is the application, second component is the status:
     *         true means that the info is valid, false means that there is no application to work with.
     */
    std::pair<AppInfo, bool> getAppInfo()
    {
        os::MutexLocker mlock(mutex_);

        if (cached_app_info_)
        {
            return {*cached_app_info_, true};
        }
        else
        {
            return {AppInfo(), false};
        }
    }

    /**
     * Switches the state to @ref BootCancelled, if allowed.
     */
    void cancelBoot()
    {
        os::MutexLocker mlock(mutex_);

        switch (state_)
        {
        case State::BootDelay:
        case State::ReadyToBoot:
        {
            state_ = State::BootCancelled;
            DEBUG_LOG("Boot cancelled\n");
            break;
        }
        case State::NoAppToBoot:
        case State::BootCancelled:
        case State::AppUpgradeInProgress:
        {
            break;
        }
        }
    }

    /**
     * Switches the state to @ref ReadyToBoot, if allowed.
     */
    void requestBoot()
    {
        os::MutexLocker mlock(mutex_);

        switch (state_)
        {
        case State::BootDelay:
        case State::BootCancelled:
        {
            state_ = State::ReadyToBoot;
            DEBUG_LOG("Boot requested\n");
            break;
        }
        case State::NoAppToBoot:
        case State::AppUpgradeInProgress:
        case State::ReadyToBoot:
        {
            break;
        }
        }
    }

    /**
     * Template method that implements all of the high-level steps of the application update procedure.
     */
    int upgradeApp(IDownloader& downloader)
    {
        /*
         * Preparation stage.
         * Note that access to the backend and all members is always protected with the mutex, this is important.
         */
        {
            os::MutexLocker mlock(mutex_);

            switch (state_)
            {
            case State::BootDelay:
            case State::BootCancelled:
            case State::NoAppToBoot:
            {
                break;      // OK, continuing below
            }
            case State::ReadyToBoot:
            case State::AppUpgradeInProgress:
            {
                return -ErrInvalidState;
            }
            }

            state_ = State::AppUpgradeInProgress;
            cached_app_info_.reset();                               // Invalidate now, as we're going to modify the storage

            int res = backend_.beginUpgrade();
            if (res < 0)
            {
                verifyAppAndUpdateState(State::BootCancelled);      // The backend could have modified the storage
                return res;
            }
        }

        DEBUG_LOG("Starting app upgrade...\n");

        /*
         * Downloading stage.
         * New application is downloaded into the storage backend via the Sink proxy class.
         * Every write() via the Sink is mutex-protected.
         */
        Sink sink(backend_, mutex_, max_application_image_size_);

        int res = downloader.download(sink);
        DEBUG_LOG("App download finished with status %d\n", res);

        /*
         * Finalization stage.
         * Checking if the downloader has succeeded, checking if the backend is able to finalize successfully.
         * Notice the mutex.
         */
        os::MutexLocker mlock(mutex_);

        assert(state_ == State::AppUpgradeInProgress);
        state_ = State::NoAppToBoot;                // Default state until proven otherwise

        if (res < 0)                                // Download failed
        {
            (void)backend_.endUpgrade(false);       // Making sure the backend is finalized; error is irrelevant
            verifyAppAndUpdateState(State::BootCancelled);
            return res;
        }

        res = backend_.endUpgrade(true);
        if (res < 0)                                // Finalization failed
        {
            DEBUG_LOG("App storage backend finalization failed (%d)\n", res);
            verifyAppAndUpdateState(State::BootCancelled);
            return res;
        }

        /*
         * Everything went well, checking if the application is valid and updating the state accordingly.
         * This method will report success even if the application image it just downloaded is not valid,
         * since that would be out of the scope of its responsibility.
         */
        verifyAppAndUpdateState(State::BootDelay);

        return ErrOK;
    }
};

}
}
