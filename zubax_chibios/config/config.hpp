/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <type_traits>
#include <functional>
#include <zubax_chibios/util/float_eq.hpp>
#include "config.h"

namespace os
{
namespace config
{
namespace _internal
{

template <typename T>
struct Param : public ::ConfigParam
{
    Param(const Param&) = delete;
    Param& operator=(const Param&) = delete;

    static_assert(std::is_floating_point<T>() || std::is_integral<T>(), "One does not simply use T here");

    Param(const char* arg_name, T arg_default, T arg_min, T arg_max) : ConfigParam
    {
        arg_name,
        float(arg_default),
        float(arg_min),
        float(arg_max),
        std::is_floating_point<T>() ? CONFIG_TYPE_FLOAT : CONFIG_TYPE_INT
    }
    {
        ::configRegisterParam_(this);
    }

    T get() const { return T(::configGet(name)); }

    int set(const T& value) const
    {
        return ::configSet(name, float(value));
    }

    int setAndSave(const T& value) const
    {
        const int res = set(value);
        if (res < 0)
        {
            return res;
        }
        return ::configSave();
    }

    bool isMax() const { return get() >= T(::ConfigParam::max); }
    bool isMin() const { return get() <= T(::ConfigParam::min); }
};

template <>
struct Param<bool> : public ::ConfigParam
{
    Param(const char* arg_name, bool arg_default) : ConfigParam
    {
        arg_name,
        arg_default ? 1.F : 0.F,
        0.F,
        1.F,
        CONFIG_TYPE_BOOL
    }
    {
        ::configRegisterParam_(this);
    }

    bool get() const { return !float_eq::closeToZero(::configGet(name)); }
    operator bool() const { return get(); }

    int set(bool value) const
    {
        return ::configSet(name, float(value));
    }

    int setAndSave(bool value) const
    {
        const int res = set(value);
        if (res < 0)
        {
            return res;
        }
        return ::configSave();
    }
};

} // namespace _internal

/**
 * Parameter usage crash course:
 *
 * Definition:
 *      static Param<int> param_foo("foo", 1, -1, 1);
 *      static Param<float> param_bar("bar", 72.12, -16.456, 100.0);
 *      static Param<bool> param_baz("baz", true);
 *
 * Usage:
 *      double my_data = param_baz ? (moon_phase * param_foo.get()) : (mercury_phase * param_bar.get());
 *
 * Parameter value access complexity is O(N) where N is the total number of params.
 *
 * Thanks for attending the class.
 */
template <typename T>
using Param = const typename _internal::Param<T>;

/**
 * This interface abstracts the configuration storage.
 */
class IStorageBackend
{
public:
    virtual ~IStorageBackend() { }

    virtual int read(std::size_t offset, void* data, std::size_t len) = 0;
    virtual int write(std::size_t offset, const void* data, std::size_t len) = 0;
    virtual int erase() = 0;
};

/**
 * Returns 0 if everything is OK, even if the configuration could not be restored (this is not an error).
 * All other interface functions assume that the config module was initialized successfully.
 * Returns negative errno in case of unrecoverable fault.
 */
int init(IStorageBackend* storage);

/**
 * Returns the number of times configSet() was executed successfully.
 * The returned value can only grow (with overflow).
 * This value can be used to reload changed parameter values in the background.
 */
unsigned getModificationCounter();

/**
 * Save configuration into the non-volatile memory.
 * @return Non-negative on success, negative errno on failure.
 */
inline int save()
{
    return ::configSave();
}

/**
 * Allows to read/modify/save/restore commands via CLI
 */
int executeCLICommand(int argc, char *argv[]);

}
}
