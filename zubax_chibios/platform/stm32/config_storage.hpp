/*
 * Copyright (c) 2015 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include "flash_writer.hpp"
#include <zubax_chibios/config/config.hpp>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cerrno>


namespace os
{
namespace stm32
{
/**
 * See os::config::IStorageBackend.
 */
class ConfigStorageBackend : public os::config::IStorageBackend
{
    const std::size_t address_;
    const std::size_t size_;

public:
    ConfigStorageBackend(void* storage_address,
                         std::size_t storage_size) :
        address_(reinterpret_cast<std::size_t>(storage_address)),
        size_(storage_size)
    {
        assert(address_ % 256 == 0);
        assert(size_    % 256 == 0);
        assert(address_ > 0);
        assert(size_    > 0);
    }

    int read(std::size_t offset, void* data, std::size_t len) override
    {
        if ((data == nullptr) ||
            (offset + len) > size_)
        {
            assert(false);
            return -EINVAL;
        }

        std::memcpy(data, reinterpret_cast<void*>(address_ + offset), len);
        return 0;
    }

    int write(std::size_t offset, const void* data, std::size_t len) override
    {
        if ((data == nullptr) ||
            (offset + len) > size_)
        {
            assert(false);
            return -EINVAL;
        }

        return FlashWriter().write(reinterpret_cast<void*>(address_ + offset), data, len) ? 0 : -EIO;
    }

    int erase() override
    {
        return FlashWriter().erase(reinterpret_cast<void*>(address_), size_) ? 0 : -EIO;
    }
};

}
}
