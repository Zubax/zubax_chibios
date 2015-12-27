/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <type_traits>
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
    static_assert(std::is_floating_point<T>() || std::is_integral<T>(), "One does not simply use T here");

    Param(const char* name, T default_, T min, T max)
    : ConfigParam
    {
        name,
        float(default_),
        float(min),
        float(max),
        std::is_floating_point<T>() ? CONFIG_TYPE_FLOAT : CONFIG_TYPE_INT
    }
    {
        ::configRegisterParam_(this);
    }

    T get() const { return T(::configGet(name)); }

    bool isMax() const { return get() >= T(::ConfigParam::max); }
    bool isMin() const { return get() <= T(::ConfigParam::min); }
};

template <>
struct Param<bool> : public ::ConfigParam
{
    Param(const char* name, bool default_)
    : ConfigParam{name, default_ ? 1.F : 0.F, 0.F, 1.F, CONFIG_TYPE_BOOL}
    {
        ::configRegisterParam_(this);
    }

    bool get() const { return ::configGet(name) != 0; }
    operator bool() const { return get(); }
};

}

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


static inline int init()
{
    return ::configInit();
}

static inline unsigned getModificationCounter()
{
    return ::configGetModificationCounter();
}

/**
 * Allows to read/modify/save/restore commands via CLI
 */
int executeCLICommand(int argc, char *argv[]);

}
}
