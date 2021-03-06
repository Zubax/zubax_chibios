/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * This file was originally written in pure C99, but later it had to be migrated to C++.
 * In its current state it's kinda written in C99 with C++ features.
 *
 *                      This is not proper C++.
 */

#include <ch.hpp>
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <limits>
#include <zubax_chibios/os.hpp>
#include "config.hpp"
#include "config.h"

#ifndef CONFIG_PARAMS_MAX
#  define CONFIG_PARAMS_MAX     40
#endif

#ifndef CONFIG_PARAM_MAX_NAME_LENGTH
#  define CONFIG_PARAM_MAX_NAME_LENGTH     92    // UAVCAN compliant
#endif

#define OFFSET_LAYOUT_HASH      0
#define OFFSET_CRC              4
#define OFFSET_VALUES           8

using namespace os::config;


static constexpr int InitCodeRestored       = 1;
static constexpr int InitCodeLayoutMismatch = 2;
static constexpr int InitCodeCRCMismatch    = 3;

static constexpr int MaxRetries = 3;


static const ConfigParam* _descr_pool[CONFIG_PARAMS_MAX];
static float _value_pool[CONFIG_PARAMS_MAX];

static int _num_params = 0;
static std::uint32_t _layout_hash = 0;
static bool _frozen = false;

static chibios_rt::Mutex _mutex;

static unsigned _modification_cnt = 0;

static IStorageBackend* g_storage = nullptr;


static std::uint32_t crc32_step(std::uint32_t crc, std::uint8_t new_byte)
{
    crc = crc ^ (std::uint32_t)new_byte;
    for (int j = 7; j >= 0; j--)
    {
        crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return crc;
}

static std::uint32_t crc32(const void* data, int len)
{
    assert(data && len >= 0);
    if (!data)
    {
        return 0;
    }

    std::uint32_t crc = 0;
    for (int i = 0; i < len; i++)
    {
        crc = crc32_step(crc, *(((const std::uint8_t*)data) + i));
    }
    return crc;
}

static bool isValid(const ConfigParam* descr, float value)
{
    assert(descr);

    if (!std::isfinite(value))
    {
        return false;
    }

    if (std::strlen(descr->name) > CONFIG_PARAM_MAX_NAME_LENGTH)
    {
        return false;
    }

    switch (descr->type)
    {
    case CONFIG_TYPE_BOOL:
    {
        if ((value < 0.0F) || (value > 1.0F))
        {
            return false;
        }
        break;
    }
    case CONFIG_TYPE_INT:
    {
        if (std::abs(value) >= 16777216.0F)      // 2**32
        {
            return false;
        }
    }
    [[fallthrough]];
    /* no break */
    case CONFIG_TYPE_FLOAT:
    {
        if (value > descr->max || value < descr->min)
        {
            return false;
        }
        break;
    }
    default:
    {
        assert(0);
        return false;
    }
    }
    return true;
}

static int indexByName(const char* name)
{
    assert(name);
    if (!name)
    {
        return -1;
    }
    for (int i = 0; i < _num_params; i++)
    {
        if (!std::strcmp(_descr_pool[i]->name, name))
        {
            return i;
        }
    }
    return -1;
}

void configRegisterParam_(const ConfigParam* param)
{
    // This function can not be executed after the startup initialization is finished
    assert(!_frozen);
    if (_frozen)
    {
        return;
    }

    ASSERT_ALWAYS(param && param->name);
    ASSERT_ALWAYS(_num_params < CONFIG_PARAMS_MAX);  // If fails here, increase CONFIG_PARAMS_MAX
    ASSERT_ALWAYS(isValid(param, param->default_)); // If fails here, param descriptor is invalid
    ASSERT_ALWAYS(indexByName(param->name) < 0);   // If fails here, param name is not unique

    // Register this param
    const int index = _num_params++;
    ASSERT_ALWAYS(_descr_pool[index] == NULL);
    _descr_pool[index] = param;
    _value_pool[index] = param->default_;

    // Update the layout identification hash
    for (const char* c = param->name; *c; c++)
    {
        _layout_hash = crc32_step(_layout_hash, *c);
    }
}

static void reinitializeDefaults()
{
    for (int i = 0; i < _num_params; i++)
    {
        _value_pool[i] = _descr_pool[i]->default_;
    }
}

int configSave(void)
{
    ASSERT_ALWAYS(_frozen);
    os::MutexLocker locker(_mutex);

    int flash_res = 0;
    for (int attempt = 0; attempt < MaxRetries; attempt++)
    {
        DEBUG_LOG("Save attempt %d\n", attempt);

        // Erase
        flash_res = g_storage->erase();
        if (flash_res)
        {
            DEBUG_LOG("Erase error %d\n", flash_res);
            continue;
        }

        // Write Layout
        flash_res = g_storage->write(OFFSET_LAYOUT_HASH, &_layout_hash, 4);
        if (flash_res)
        {
            DEBUG_LOG("Hash write error %d\n", flash_res);
            continue;
        }

        {
            // Write CRC
            const int pool_len = _num_params * sizeof(_value_pool[0]);
            const std::uint32_t true_crc = crc32(_value_pool, pool_len);
            flash_res = g_storage->write(OFFSET_CRC, &true_crc, 4);
            if (flash_res)
            {
                DEBUG_LOG("CRC write error %d\n", flash_res);
                continue;
            }

            // Write Values
            flash_res = g_storage->write(OFFSET_VALUES, _value_pool, pool_len);
            if (flash_res)
            {
                DEBUG_LOG("Data write error %d\n", flash_res);
                continue;
            }
        }

        DEBUG_LOG("Saved successfully\n");
        return 0;
    }

    assert(flash_res);
    return flash_res;
}

int configErase(void)
{
    ASSERT_ALWAYS(_frozen);
    os::MutexLocker locker(_mutex);
    int res = g_storage->erase();
    if (res >= 0)
    {
        reinitializeDefaults();
        _modification_cnt += 1;
    }
    return res;
}

const char* configNameByIndex(int index)
{
    ASSERT_ALWAYS(_frozen);
    assert(index >= 0);
    if (index < 0 || index >= _num_params)
    {
        return NULL;
    }
    return _descr_pool[index]->name;
}

int configSet(const char* name, float value)
{
    int retval = 0;
    ASSERT_ALWAYS(_frozen);
    os::MutexLocker locker(_mutex);

    const int index = indexByName(name);
    if (index < 0)
    {
        retval = -ENOENT;
        goto leave;
    }

    if (!isValid(_descr_pool[index], value))
    {
        retval = -EINVAL;
        goto leave;
    }

    _modification_cnt += 1;
    _value_pool[index] = value;

    leave:
    return retval;
}

int configGetDescr(const char* name, ConfigParam* out)
{
    ASSERT_ALWAYS(_frozen);
    assert(out);
    if (!out)
    {
        return -EINVAL;
    }

    int retval = 0;
    os::MutexLocker locker(_mutex);

    const int index = indexByName(name);
    if (index < 0)
    {
        retval = -ENOENT;
        goto leave;
    }

    *out = *_descr_pool[index];

    leave:
    return retval;
}

float configGet(const char* name)
{
    ASSERT_ALWAYS(_frozen);
    os::MutexLocker locker(_mutex);
    const int index = indexByName(name);
    assert(index >= 0);
    const float val = (index < 0) ? nanf("") : _value_pool[index];
    assert(std::isfinite(val));
    return val;
}

namespace os
{
namespace config
{

int init(IStorageBackend* storage)
{
    ASSERT_ALWAYS(_num_params <= CONFIG_PARAMS_MAX);  // being paranoid
    ASSERT_ALWAYS(!_frozen);

    if (storage == nullptr)
    {
        return -EINVAL;
    }

    g_storage = storage;

    _frozen = true;

    reinitializeDefaults();             // Init defaults by default

    {
        std::uint32_t stored_layout_hash = 0xdeadbeef;

        // Read the layout hash
        for (int attempt = 0; attempt < MaxRetries; attempt++)
        {
            int flash_res = g_storage->read(OFFSET_LAYOUT_HASH, &stored_layout_hash, 4);
            if (flash_res == 0)
            {
                if (stored_layout_hash == _layout_hash)
                {
                    break;
                }
            }
        }

        if (stored_layout_hash != _layout_hash)
        {
            return InitCodeLayoutMismatch;
        }
    }

    // If the layout hash has not changed, we can restore the values safely
    for (int attempt = 0; attempt < MaxRetries; attempt++)
    {
        const int pool_len = _num_params * sizeof(_value_pool[0]);

        // Read the data
        int flash_res = g_storage->read(OFFSET_VALUES, _value_pool, pool_len);
        if (flash_res)
        {
            continue;
        }

        // Check CRC
        const std::uint32_t true_crc = crc32(_value_pool, pool_len);
        std::uint32_t stored_crc = 0;
        flash_res = g_storage->read(OFFSET_CRC, &stored_crc, 4);
        if (flash_res || (true_crc != stored_crc))
        {
            continue;
        }

        // Reinitialize defaults if restored values are not valid
        for (int i = 0; i < _num_params; i++)
        {
            if (!isValid(_descr_pool[i], _value_pool[i]))
            {
                _value_pool[i] = _descr_pool[i]->default_;
            }
        }

        return InitCodeRestored;
    }

    reinitializeDefaults();

    return InitCodeCRCMismatch;
}

std::uint16_t getParamCount()
{
    return std::uint16_t(_num_params);
}

unsigned getModificationCounter()
{
    return _modification_cnt;           // Atomic access
}


template <typename T>
static inline ParamMetadataPointer constructParamPointer(const int index)
{
    return ParamMetadataPointer(std::in_place_type<Param<T>*>, static_cast<Param<T>*>(_descr_pool[index]));
}

template <std::size_t CandidateTypeIndex = 0>
ParamMetadataPointer deduceSmallestIntegralParamType(const ::ConfigParam& desc, const int index)
{
    if constexpr (CandidateTypeIndex < std::variant_size_v<ParamMetadataPointer>)
    {
        using T = typename std::remove_pointer_t<std::variant_alternative_t<CandidateTypeIndex,
                                                                            ParamMetadataPointer>>::Type;
        if constexpr (std::is_integral_v<T>)
        {
            constexpr auto min = float(std::numeric_limits<T>::min());
            constexpr auto max = float(std::numeric_limits<T>::max());
            if ((min <= desc.min) && (desc.max <= max))
            {
                return constructParamPointer<T>(index);
            }
            else
            {
                return deduceSmallestIntegralParamType<CandidateTypeIndex + 1>(desc, index);
            }
        }
    }

    // We got through all of the types and couldn't find one matching - last resort is return as float
    return constructParamPointer<float>(index);
}


std::optional<ParamMetadataPointer> getParamMetadata(const char* name)
{
    // Locking is not required here, all access is read-only and the data is immutable
    const int index = (name == nullptr) ? -1 : indexByName(name);
    if (index < 0)
    {
        return {};
    }

    const auto desc = *_descr_pool[index];

    if (desc.type == CONFIG_TYPE_BOOL)
    {
        return constructParamPointer<bool>(index);
    }

    if (desc.type == CONFIG_TYPE_FLOAT)
    {
        return constructParamPointer<float>(index);
    }

    assert(desc.type == CONFIG_TYPE_INT);
    return deduceSmallestIntegralParamType(desc, index);
}

}
}
