/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <zubax_chibios/os.hpp>
#include <cstdint>
#include <cassert>


namespace os
{
namespace bootloader
{
/**
 * Error codes.
 * These are returned from functions in negated form, i.e. -10000 means error code 10000.
 */
static constexpr std::int16_t ErrOK                     = 0;
static constexpr std::int16_t ErrInvalidState           = 10001;
static constexpr std::int16_t ErrAppImageTooLarge       = 10002;
static constexpr std::int16_t ErrAppStorageWriteFailure = 10003;

/**
 * This is used to verify integrity of the application and other data.
 * Note that firmware CRC verification is a very computationally intensive process that needs to be completed
 * in a limited time interval, which should be minimized. Therefore, this class has been carefully manually
 * optimized to achieve the optimal balance between speed and ROM footprint.
 *
 * CRC-64-WE
 * Description: http://reveng.sourceforge.net/crc-catalogue/17plus.htm#crc.cat-bits.64
 * Initial value: 0xFFFFFFFFFFFFFFFF
 * Poly: 0x42F0E1EBA9EA3693
 * Reverse: no
 * Output xor: 0xFFFFFFFFFFFFFFFF
 * Check: 0x62EC59E3F1A4F00A
 */
class CRC64WE
{
    static constexpr std::uint64_t Poly = 0x42F0E1EBA9EA3693;
    static constexpr std::uint64_t Mask = std::uint64_t(1) << 63;

    std::uint64_t crc_ = 0xFFFFFFFFFFFFFFFFULL;

public:
    void add(const void* data, unsigned len)
    {
        auto bytes = static_cast<const std::uint8_t*>(data);
        assert(bytes != nullptr);
        while (len --> 0)
        {
            crc_ ^= std::uint64_t(*bytes++) << 56;

            // Do not fold this into loop! The difference in performance can be drastic.
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
            crc_ = (crc_ & Mask) ? (crc_ << 1) ^ Poly : crc_ << 1;
        }
    }

    std::uint64_t get() const { return crc_ ^ 0xFFFFFFFFFFFFFFFFULL; }
};

}
}
