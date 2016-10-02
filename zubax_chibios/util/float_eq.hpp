/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * Floating point comparison functions borrowed from Libuavcan (http://uavcan.org).
 * These allow to safely compare floating point numbers of arbitrary precision, as well as objects containing them.
 */

#pragma once

#include <limits>
#include <type_traits>
#include <cmath>
#include <cstdlib>
#include <algorithm>


namespace os
{
/**
 * The following functions defined in this namespace are of interest:
 *
 *  - close()           Compare two values. If the values are floats or CONTAIN floats, perform fuzzy comparison,
 *                      otherwise perform exact comparison.
 *
 *  - closeToZero()     Like close(), where the second argument is set to zero of the appropriate type.
 *
 *  - exactlyEqual()    Perform equality comparison in a way that doesn't trigger compiler warnings.
 *                      The method is (x <= y) && (x >= y), so it may not work for complex types.
 */
namespace float_eq
{
/**
 * Float comparison precision. The default should be safe in all meaningful use cases.
 * For details refer to:
 *  http://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 *  https://code.google.com/p/googletest/source/browse/trunk/include/gtest/internal/gtest-internal.h
 */
#ifdef DEFAULT_FLOAT_COMPARISON_EPSILON_MULT
static constexpr unsigned DefaultEpsilonMult = DEFAULT_FLOAT_COMPARISON_EPSILON_MULT;
#else
static constexpr unsigned DefaultEpsilonMult = 10;
#endif

/**
 * Exact comparison of two floats that suppresses the compiler warnings.
 * Most of the time you DON'T want to use this function! Consider using close() instead.
 */
template<typename T>
inline bool exactlyEqual(const T left,
                         const T right)
{
    return (left <= right) && (left >= right);
}

/**
 * This function performs fuzzy comparison of two floating point numbers.
 * Type of T can be either float, double or long double.
 * For details refer to http://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 * See also: @ref DEFAULT_FLOAT_COMPARISON_EPSILON_MULT.
 */
template<typename T>
inline bool close(const T a,
                  const T b,
                  const T absolute_epsilon,
                  const T relative_epsilon)
{
    // NAN
    if (std::isnan(a) || std::isnan(b))
    {
        return false;
    }

    // Infinity
    if (std::isinf(a) || std::isinf(b))
    {
        return exactlyEqual(a, b);
    }

    // Close numbers near zero
    if (std::abs(a - b) <= absolute_epsilon)
    {
        return true;
    }

    // General case
    return std::abs(a - b) <= (std::max(std::abs(a), std::abs(b)) * relative_epsilon);
}

/**
 * This namespace contains implementation details for close().
 * Don't try this at home.
 */
namespace impl_
{

template<typename This, typename Rhs>
inline constexpr auto hasIsCloseMethod(const This& t, const Rhs& r) -> decltype(t.isClose(r), std::true_type())
{
    return std::true_type();
}

inline constexpr std::false_type hasIsCloseMethod(...)
{
    return std::false_type();
}

/// First stage: bool L::isClose(R)
template<typename L, typename R>
inline bool first(const L& left, const R& right, std::true_type)
{
    return left.isClose(right);
}

/// Second stage: bool R::isClose(L)
template<typename L, typename R>
inline bool second(const L& left, const R& right, std::true_type)
{
    return right.isClose(left);  // Right/left swapped!
}

/// Second stage: L == R
template<typename L, typename R>
inline bool second(const L& left, const R& right, std::false_type)
{
    return left == right;
}

/// First stage: select either L == R or bool R::isClose(L)
template<typename L, typename R>
inline bool first(const L& left, const R& right, std::false_type)
{
    return impl_::second(left, right,  // Right/left swapped!
                         hasIsCloseMethod(right, left));
}

} // namespace impl_

/**
 * Generic fuzzy comparison function.
 *
 * This function properly handles floating point comparison, including mixed floating point type comparison,
 * e.g. float with long double.
 *
 * Two objects of types A and B will be fuzzy comparable if either method is defined:
 *  - bool A::isClose(const B&) const
 *  - bool A::isClose(B) const
 *  - bool B::isClose(const A&) const
 *  - bool B::isClose(A) const
 *
 * Call close(A, B) will be dispatched as follows:
 *
 *  - If A and B are both floating point types (float, double, long double) - possibly different - the call will be
 *    dispatched to @ref close(). If A and B are different types, value of the larger type will be coerced
 *    to the smaller type, e.g. close(long double, float) --> close(float, float).
 *
 *  - If A defines isClose() that accepts B, the call will be dispatched there.
 *
 *  - If B defines isClose() that accepts A, the call will be dispatched there (A/B swapped).
 *
 *  - Last resort is A == B.
 *
 * Alternatively, a custom behavior can be implemented via template specialization.
 *
 * See also: @ref DEFAULT_FLOAT_COMPARISON_EPSILON_MULT.
 *
 * Examples:
 *  close(1.0, 1.0F)                                         --> true
 *  close(1.0, 1.0F + std::numeric_limits<float>::epsilon()) --> true
 *  close(1.0, 1.1)                                          --> false
 *  close("123", std::string("123"))                         --> true (using std::string's operator ==)
 *  close(inf, inf)                                          --> true
 *  close(inf, -inf)                                         --> false
 *  close(nan, nan)                                          --> false (any comparison with nan returns false)
 *  close(123, "123")                                        --> compilation error: operator == is not defined
 */
template<typename L, typename R>
inline bool close(const L& left, const R& right)
{
    return impl_::first(left, right, impl_::hasIsCloseMethod(left, right));
}

/*
 * Float comparison specializations
 */
inline bool close(const float left,
                  const float right)
{
    return close(left, right,
                 std::numeric_limits<float>::epsilon(),
                 std::numeric_limits<float>::epsilon() * DefaultEpsilonMult);
}

inline bool close(const double left,
                  const double right)
{
    return close(left, right,
                 std::numeric_limits<double>::epsilon(),
                 std::numeric_limits<double>::epsilon() * DefaultEpsilonMult);
}

inline bool close(const long double left,
                  const long double right)
{
    return close(left, right,
                 std::numeric_limits<long double>::epsilon(),
                 std::numeric_limits<long double>::epsilon() * DefaultEpsilonMult);
}

/*
 * Mixed floating type comparison - coercing larger type to smaller type
 */
inline bool close(const float left,
                  const double right)
{
    return close(left, static_cast<float>(right));
}

inline bool close(const double left,
                  const float right)
{
    return close(static_cast<float>(left), right);
}

inline bool close(const float left,
                  const long double right)
{
    return close(left, static_cast<float>(right));
}

inline bool close(const long double left,
                  const float right)
{
    return close(static_cast<float>(left), right);
}

inline bool close(const double left,
                  const long double right)
{
    return close(left, static_cast<double>(right));
}

inline bool close(const long double left,
                  const double right)
{
    return close(static_cast<double>(left), right);
}

/**
 * Comparison against zero.
 * Helps to compare a floating point number against zero if the exact type is unknown.
 * For non-floating point types performs exact comparison against an object of the same type constructed from
 * integer zero.
 */
template<typename T>
inline bool closeToZero(const T& x)
{
    return x == T(0);
}

inline bool closeToZero(const float x)
{
    return close(x, static_cast<float>(0.0F));
}

inline bool closeToZero(const double x)
{
    return close(x, static_cast<double>(0.0));
}

inline bool closeToZero(const long double x)
{
    return close(x, static_cast<long double>(0.0L));
}


template<typename T>
inline bool positive(const T& x)
{
    return (x > T(0)) && !closeToZero(x);
}

template<typename T>
inline bool negative(const T& x)
{
    return (x < T(0)) && !closeToZero(x);
}

}
}
