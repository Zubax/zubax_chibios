/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL
} ConfigDataType;

typedef struct
{
    const char* name;
    float default_;
    float min;
    float max;
    ConfigDataType type;
} ConfigParam;


#if !defined(GLUE)
#  define GLUE_(a, b) a##b
#  define GLUE(a, b)  GLUE_(a, b)
#endif

#define CONFIG_PARAM_RAW_(name, default_, min, max, type)             \
    static const ConfigParam GLUE(_config_local_param_, __LINE__) =   \
        {name, default_, min, max, type};                             \
    __attribute__((constructor, unused))                              \
    static void GLUE(_config_local_constructor_, __LINE__)(void) {    \
        configRegisterParam_(&GLUE(_config_local_param_, __LINE__));  \
    }

/**
 * Parameter definition macros.
 * Defined parameter can be accessed through configGet("param-name"), configGetDescr(...).
 */
#define CONFIG_PARAM_FLOAT(name, default_, min, max)  CONFIG_PARAM_RAW_(name, default_, min, max, CONFIG_TYPE_FLOAT)
#define CONFIG_PARAM_INT(name, default_, min, max)    CONFIG_PARAM_RAW_(name, default_, min, max, CONFIG_TYPE_INT)
#define CONFIG_PARAM_BOOL(name, default_)             CONFIG_PARAM_RAW_(name, default_, 0,   1,   CONFIG_TYPE_BOOL)


/**
 * Internal logic; shall never be called explicitly.
 */
void configRegisterParam_(const ConfigParam* param);

/**
 * Saves the config into the non-volatile memory.
 * May enter a huge critical section, so it shall never be called concurrently with hard real time processes.
 */
int configSave(void);

/**
 * Erases the configuration from the non-volatile memory; current parameter values will not be affected.
 * Same warning as for @ref configSave()
 */
int configErase(void);

/**
 * @param [in] index Non-negative parameter index
 * @return Name, or NULL if the index is out of range
 */
const char* configNameByIndex(int index);

/**
 * @param [in] name  Parameter name
 * @param [in] value Parameter value
 * @return 0 if the parameter does exist and the value is valid, negative errno otherwise.
 */
int configSet(const char* name, float value);

/**
 * @param [in]  name Parameter name
 * @param [out] out  Parameter descriptor
 * @return Returns 0 if the parameter does exist, negative errno otherwise.
 */
int configGetDescr(const char* name, ConfigParam* out);

/**
 * @param [in] name Parameter name
 * @return The parameter value if it does exist; otherwise fires an assert() in debug builds, returns NAN in release.
 */
float configGet(const char* name);

#ifdef __cplusplus
}
#endif
