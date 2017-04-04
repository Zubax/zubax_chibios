/*
 * Copyright (c) 2016 Zubax, zubax.com
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
    static constexpr unsigned DefaultBootDelayMSec = 5000;

    State state_;
    IAppStorageBackend& backend_;

    const std::uint32_t max_application_image_size_;
    const unsigned boot_delay_msec_;
    ::systime_t boot_delay_started_at_st_;

    std::uint8_t rom_buffer_[1024];             ///< Larger buffer enables faster CRC verification, which is important

    chibios_rt::Mutex mutex_;

    /// Caching is needed because app check can sometimes take a very long time (several seconds)
    os::helpers::LazyConstructor<AppInfo> cached_app_info_;

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

    std::pair<AppDescriptor, bool> locateAppDescriptor();

    void verifyAppAndUpdateState(const State state_on_success);

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
     */
    Bootloader(IAppStorageBackend& backend,
               std::uint32_t max_application_image_size = 0xFFFFFFFFU,
               unsigned boot_delay_msec = DefaultBootDelayMSec);

    /**
     * @ref State.
     */
    State getState();

    /**
     * Returns info about the application, if any.
     * @return First component is the application, second component is the status:
     *         true means that the info is valid, false means that there is no application to work with.
     */
    std::pair<AppInfo, bool> getAppInfo();

    /**
     * Switches the state to @ref BootCancelled, if allowed.
     */
    void cancelBoot();

    /**
     * Switches the state to @ref ReadyToBoot, if allowed.
     */
    void requestBoot();

    /**
     * Template method that implements all of the high-level steps of the application update procedure.
     */
    int upgradeApp(IDownloader& downloader);
};

}
}
