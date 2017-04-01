/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * Various small helpers.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <utility>

#define EXECUTE_ONCE_CAT1_(a, b) EXECUTE_ONCE_CAT2_(a, b)
#define EXECUTE_ONCE_CAT2_(a, b) a##b

/**
 * This macro can be used in function and method bodies to execute a certain block of code only once.
 * Every instantiation creates one static variable.
 * This macro is not thread safe.
 *
 * Usage:
 *   puts("Regular code");
 *   EXECUTE_ONCE_NON_THREAD_SAFE
 *   {
 *      puts("This block will be executed only once");
 *   }
 *   puts("Regular code again");
 */
#define EXECUTE_ONCE_NON_THREAD_SAFE \
    static bool EXECUTE_ONCE_CAT1_(_executed_once_, __LINE__) = false; \
    for (; EXECUTE_ONCE_CAT1_(_executed_once_, __LINE__) == false; EXECUTE_ONCE_CAT1_(_executed_once_, __LINE__) = true)


namespace os
{
namespace helpers
{
/**
 * Used with @ref LazyConstructor (and possibly something else)
 */
enum class MemoryInitializationPolicy
{
    NoInit,
    ZeroFill
};

/**
 * A regular lazy initialization helper.
 */
template <typename T,
          MemoryInitializationPolicy MemInitPolicy = MemoryInitializationPolicy::ZeroFill>
class LazyConstructor
{
    alignas(T) std::uint8_t pool_[sizeof(T)]{};
    T* ptr_ = nullptr;

    void assertConstructed() const
    {
        assert(ptr_ != nullptr);
    }

public:
    ~LazyConstructor()
    {
        destroy();
    }

    void destroy()
    {
        if (ptr_ != nullptr)
        {
            ptr_->~T();
            ptr_ = nullptr;
        }
    }

    template <typename... Args>
    void construct(Args... args)
    {
        destroy();
        if (MemInitPolicy == MemoryInitializationPolicy::ZeroFill)
        {
            std::fill(std::begin(pool_), std::end(pool_), 0);
        }
        ptr_ = new (pool_) T(std::forward<Args>(args)...);
    }

    bool isConstructed() const { return ptr_ != nullptr; }

    T*       get()       { return ptr_; }
    const T* get() const { return ptr_; }

    T* operator->()
    {
        assertConstructed();
        return ptr_;
    }

    const T* operator->() const
    {
        assertConstructed();
        return ptr_;
    }

    T& operator*()
    {
        assertConstructed();
        return *ptr_;
    }

    const T& operator*() const
    {
        assertConstructed();
        return *ptr_;
    }
};

}
}
