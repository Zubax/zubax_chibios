/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <ch.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <zubax_chibios/sys/assert_always.h>
#include <zubax_chibios/sys/sys.h>
#include <zubax_chibios/config/config.h>

#ifndef CONFIG_PARAMS_MAX
#  define CONFIG_PARAMS_MAX     40
#endif

int configStorageRead(unsigned offset, void* data, unsigned len);
int configStorageWrite(unsigned offset, const void* data, unsigned len);
int configStorageErase(void);


#define OFFSET_LAYOUT_HASH      0
#define OFFSET_CRC              4
#define OFFSET_VALUES           8

static const ConfigParam* _descr_pool[CONFIG_PARAMS_MAX];
static float _value_pool[CONFIG_PARAMS_MAX];

static int _num_params = 0;
static uint32_t _layout_hash = 0;
static bool _frozen = false;

static Mutex _mutex;


static uint32_t crc32_step(uint32_t crc, uint8_t new_byte)
{
    crc = crc ^ (uint32_t)new_byte;
    for (int j = 7; j >= 0; j--)
        crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    return crc;
}

static uint32_t crc32(const void* data, int len)
{
    assert(data && len >= 0);
    if (!data)
        return 0;

    uint32_t crc = 0;
    for (int i = 0; i < len; i++)
        crc = crc32_step(crc, *(((const uint8_t*)data) + i));
    return crc;
}

static bool isValid(const ConfigParam* descr, float value)
{
    assert(descr);

    if (!isfinite(value))
        return false;

    switch (descr->type) {
    case CONFIG_TYPE_BOOL:
        if (value != 0.0f && value != 1.0f)
            return false;
        break;

    case CONFIG_TYPE_INT: {
        volatile const long truncated = (long)value;
        if (value != (float)truncated)
            return false;
    }
        /* fallthrough */
    case CONFIG_TYPE_FLOAT:
        if (value > descr->max || value < descr->min)
            return false;
        break;

    default:
        assert(0);
        return false;
    }
    return true;
}

static int indexByName(const char* name)
{
    assert(name);
    if (!name)
        return -1;
    for (int i = 0; i < _num_params; i++) {
        if (!strcmp(_descr_pool[i]->name, name))
            return i;
    }
    return -1;
}

void configRegisterParam_(const ConfigParam* param)
{
    // This function can not be executed after the startup initialization is finished
    assert(!_frozen);
    if (_frozen)
        return;

    ASSERT_ALWAYS(param && param->name);
    ASSERT_ALWAYS(_num_params < CONFIG_PARAMS_MAX);  // If fails here, increase CONFIG_PARAMS_MAX
    ASSERT_ALWAYS(isValid(param, param->default_)); // If fails here, param descriptor is invalid
    ASSERT_ALWAYS(indexByName(param->name) < 0);   // If fails here, param name is not unique

    // Register this param
    const int index = _num_params++;
    ASSERT_ALWAYS(_descr_pool[index] == NULL);
    ASSERT_ALWAYS(_value_pool[index] == 0);
    _descr_pool[index] = param;
    _value_pool[index] = param->default_;

    // Update the layout identification hash
    for (const char* ch = param->name; *ch; ch++)
        _layout_hash = crc32_step(_layout_hash, *ch);
}

static void reinitializeDefaults(const char* reason)
{
    lowsyslog("Config: Initializing defaults - %s\n", reason);
    for (int i = 0; i < _num_params; i++)
        _value_pool[i] = _descr_pool[i]->default_;
}

int configInit(void)
{
    ASSERT_ALWAYS(_num_params <= CONFIG_PARAMS_MAX);  // being paranoid
    ASSERT_ALWAYS(!_frozen);
    _frozen = true;

    chMtxInit(&_mutex);

    // Read the layout hash
    uint32_t stored_layout_hash = 0xdeadbeef;
    int flash_res = configStorageRead(OFFSET_LAYOUT_HASH, &stored_layout_hash, 4);
    if (flash_res)
        goto flash_error;

    // If the layout hash has not changed, we can restore the values safely
    if (stored_layout_hash == _layout_hash) {
        const int pool_len = _num_params * sizeof(_value_pool[0]);

        // Read the data
        flash_res = configStorageRead(OFFSET_VALUES, _value_pool, pool_len);
        if (flash_res)
            goto flash_error;

        // Check CRC
        const uint32_t true_crc = crc32(_value_pool, pool_len);
        uint32_t stored_crc = 0;
        flash_res = configStorageRead(OFFSET_CRC, &stored_crc, 4);
        if (flash_res)
            goto flash_error;

        // Reinitialize defaults if restored values are not valid or if CRC does not match
        if (true_crc == stored_crc) {
            lowsyslog("Config: %i params restored\n", _num_params);
            for (int i = 0; i < _num_params; i++) {
                if (!isValid(_descr_pool[i], _value_pool[i])) {
                    lowsyslog("Config: Resetting param [%s]: %f --> %f\n",
                        _descr_pool[i]->name, _value_pool[i], _descr_pool[i]->default_);
                    _value_pool[i] = _descr_pool[i]->default_;
                }
            }
        } else {
            reinitializeDefaults("CRC mismatch");
        }
    } else {
        reinitializeDefaults("Layout mismatch");
    }

    return 0;

    flash_error:
    assert(flash_res);
    reinitializeDefaults("Flash error");
    return flash_res;
}

int configSave(void)
{
    ASSERT_ALWAYS(_frozen);
    chMtxLock(&_mutex);

    // Erase
    int flash_res = configStorageErase();
    if (flash_res)
        goto flash_error;

    // Write Layout
    flash_res = configStorageWrite(OFFSET_LAYOUT_HASH, &_layout_hash, 4);
    if (flash_res)
        goto flash_error;

    // Write CRC
    const int pool_len = _num_params * sizeof(_value_pool[0]);
    const uint32_t true_crc = crc32(_value_pool, pool_len);
    flash_res = configStorageWrite(OFFSET_CRC, &true_crc, 4);
    if (flash_res)
        goto flash_error;

    // Write Values
    flash_res = configStorageWrite(OFFSET_VALUES, _value_pool, pool_len);
    if (flash_res)
        goto flash_error;

    chMtxUnlock();
    return 0;

    flash_error:
    assert(flash_res);
    chMtxUnlock();
    return flash_res;
}

int configErase(void)
{
    ASSERT_ALWAYS(_frozen);
    chMtxLock(&_mutex);
    int res = configStorageErase();
    chMtxUnlock();
    return res;
}

const char* configNameByIndex(int index)
{
    ASSERT_ALWAYS(_frozen);
    assert(index >= 0);
    if (index < 0 || index >= _num_params)
        return NULL;
    return _descr_pool[index]->name;
}

int configSet(const char* name, float value)
{
    int retval = 0;
    ASSERT_ALWAYS(_frozen);
    chMtxLock(&_mutex);

    const int index = indexByName(name);
    if (index < 0) {
        retval = -ENOENT;
        goto leave;
    }

    if (!isValid(_descr_pool[index], value)) {
        retval = -EINVAL;
        goto leave;
    }

    _value_pool[index] = value;

    leave:
    chMtxUnlock();
    return retval;
}

int configGetDescr(const char* name, ConfigParam* out)
{
    ASSERT_ALWAYS(_frozen);
    assert(out);
    if (!out)
        return -EINVAL;

    int retval = 0;
    chMtxLock(&_mutex);

    const int index = indexByName(name);
    if (index < 0) {
        retval = -ENOENT;
        goto leave;
    }

    *out = *_descr_pool[index];

    leave:
    chMtxUnlock();
    return retval;
}

float configGet(const char* name)
{
    ASSERT_ALWAYS(_frozen);
    chMtxLock(&_mutex);
    const int index = indexByName(name);
    assert(index >= 0);
    const float val = (index < 0) ? nanf("") : _value_pool[index];
    chMtxUnlock();
    assert(isfinite(val));
    return val;
}
