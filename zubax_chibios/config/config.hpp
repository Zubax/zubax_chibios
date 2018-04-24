/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <type_traits>
#include <functional>
#include <variant>
#include <optional>
#include <cstdint>
#include "config.h"


namespace os
{
namespace config
{
/**
 * Implementation details, do not use directly.
 */
namespace _internal
{
/**
 * A convenient typesafe wrapper that hides the ugly C API.
 * Someday in the future this will have to be re-implemented into a proper, better library.
 */
template <typename T>
struct Param : public ::ConfigParam
{
    using Type = T;

    using ::ConfigParam::name;

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

    bool isMin() const { return get() <= T(::ConfigParam::min); }
    bool isMax() const { return get() >= T(::ConfigParam::max); }

    T getDefaultValue() const { return T(::ConfigParam::default_); }
    T getMinValue()     const { return T(::ConfigParam::min); }
    T getMaxValue()     const { return T(::ConfigParam::max); }
};

template <>
struct Param<bool> : public ::ConfigParam
{
    using Type = bool;

    using ::ConfigParam::name;

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

    bool get() const { return ::configGet(name) > 1e-6F; }
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

    bool getDefaultValue() const { return ::ConfigParam::default_ > 1e-6F; }
    bool getMinValue()     const { return false; }
    bool getMaxValue()     const { return true; }
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
 * Total number of known configuration parameters.
 */
std::uint16_t getParamCount();

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
 * Erase the non-volatile memory and reset to factory defaults.
 * @return Non-negative on success, negative errno on failure.
 */
inline int erase()
{
    return ::configErase();
}

/**
 * Returns the name of the configuration parameter by index (zero-based).
 * Returns nullptr if the index exceeds the set of parameters.
 */
inline const char* getNameOfParamAtIndex(std::uint16_t index)
{
    return ::configNameByIndex(int(index));
}

/**
 * This variant is used with the metadata accessor below. Otherwise it's mostly useless.
 * The underlying type is deduced via some clever magic tricks at runtime.
 * Observe that the parameters are ordered in the order of their value range, unsigned first.
 */
using ParamMetadataPointer = std::variant<
    Param<bool>*,
    Param<std::uint8_t >*, Param<std::int8_t >*,
    Param<std::uint16_t>*, Param<std::int16_t>*,
    Param<std::uint32_t>*, Param<std::int32_t>*,
    Param<std::uint64_t>*, Param<std::int64_t>*,
    Param<float>*
>;

/**
 * Returns typed pointer to the parameter metadata.
 * If name is a nullptr, or the name is not known, returns an empty option.
 * The fact that the function accepts nullptr allows one to use it with the index-based accessor as follows:
 *      out = getParamMetadata(getNameOfParamAtIndex(index))
 */
std::optional<ParamMetadataPointer> getParamMetadata(const char* name);

}
}
