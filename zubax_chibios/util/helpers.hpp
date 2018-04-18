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
#include <algorithm>
#include <iterator>

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

/**
 * Branching hints; these are compiler-dependent.
 */
#define LIKELY(x)       (__builtin_expect((x), true))
#define UNLIKELY(x)     (__builtin_expect((x), false))
