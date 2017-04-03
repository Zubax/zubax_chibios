/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "bootloader.hpp"
#include <ch.hpp>
#include <zubax_chibios/util/heapless.hpp>


namespace os
{
namespace bootloader
{
namespace
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

}

std::pair<Bootloader::AppDescriptor, bool> Bootloader::locateAppDescriptor()
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
                DEBUG_LOG("App descriptor found, but CRC is invalid (%s != %s)\n",
                          os::heapless::intToString(crc.get()).c_str(),
                          os::heapless::intToString(desc.app_info.image_crc).c_str());
                continue;       // Look further...
            }
        }

        // Returning if the descriptor is correct
        DEBUG_LOG("App descriptor located at offset %x\n", unsigned(offset));
        return {desc, true};
    }

    return {AppDescriptor(), false};
}

void Bootloader::verifyAppAndUpdateState(const State state_on_success)
{
    const auto appdesc_result = locateAppDescriptor();

    if (appdesc_result.second)
    {
        cached_app_info_.construct(appdesc_result.first.app_info);
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
        cached_app_info_.destroy();
        state_ = State::NoAppToBoot;

        DEBUG_LOG("App not found\n");
    }
}

Bootloader::Bootloader(IAppStorageBackend& backend,
                       std::uint32_t max_application_image_size,
                       unsigned boot_delay_msec) :
    backend_(backend),
    max_application_image_size_(max_application_image_size),
    boot_delay_msec_(boot_delay_msec)
{
    os::MutexLocker mlock(mutex_);
    verifyAppAndUpdateState(State::BootDelay);
}

State Bootloader::getState()
{
    os::MutexLocker mlock(mutex_);

    if ((state_ == State::BootDelay) &&
        (chVTTimeElapsedSinceX(boot_delay_started_at_st_) >= MS2ST(boot_delay_msec_)))
    {
        DEBUG_LOG("Boot delay expired\n");
        state_ = State::ReadyToBoot;
    }

    return state_;
}

std::pair<AppInfo, bool> Bootloader::getAppInfo()
{
    os::MutexLocker mlock(mutex_);

    if (cached_app_info_.isConstructed())
    {
        return {*cached_app_info_, true};
    }
    else
    {
        return {AppInfo(), false};
    }
}

void Bootloader::cancelBoot()
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

void Bootloader::requestBoot()
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

int Bootloader::upgradeApp(IDownloader& downloader)
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
        cached_app_info_.destroy();                             // Invalidate now, as we're going to modify the storage

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

}
}
