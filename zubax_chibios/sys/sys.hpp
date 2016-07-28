/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#if !defined(DEBUG_BUILD) && !defined(RELEASE_BUILD)
#  error Either DEBUG_BUILD or RELEASE_BUILD must be defined
#endif

#include <ch.hpp>
#include <hal.h>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <cassert>
#include "execute_once.hpp"


#ifndef STRINGIZE
#  define STRINGIZE2(x)         #x
#  define STRINGIZE(x)          STRINGIZE2(x)
#endif

#if defined(DEBUG_BUILD) && DEBUG_BUILD
# define MAKE_ASSERT_MSG_(x)      (__FILE__ ":" STRINGIZE(__LINE__) ":" STRINGIZE(x))
#else
# define MAKE_ASSERT_MSG_(x)      (STRINGIZE(__LINE__))
#endif

#define ASSERT_ALWAYS(x)                                    \
    do {                                                    \
        if ((x) == 0) {                                     \
            chSysHalt(MAKE_ASSERT_MSG_(x));                 \
        }                                                   \
    } while (0)


#define LIKELY(x)       (__builtin_expect((x), true))
#define UNLIKELY(x)     (__builtin_expect((x), false))


#if defined(DEBUG_BUILD) && DEBUG_BUILD
# define DEBUG_LOG(...)         ::os::lowsyslog(__VA_ARGS__)
#else
# define DEBUG_LOG(...)         ((void)0)
#endif


namespace os
{

static constexpr unsigned DefaultStdIOByteWriteTimeoutMSec = 2;		///< Enough for 115200 baud and higher

/**
 * NuttX-like console print; should be used instead of printf()/chprintf()
 * This function always outputs into the debug UART regardless of the current stdout configuration.
 * TODO: use type safe version based on variadic templates.
 */
__attribute__ ((format (printf, 1, 2)))
void lowsyslog(const char* format, ...);

/**
 * Changes current stdout stream and its write timeout.
 * This setting does not affect @ref lowsyslog().
 */
void setStdIOStream(::BaseChannel* stream, unsigned byte_write_timeout_msec = DefaultStdIOByteWriteTimeoutMSec);
::BaseChannel* getStdIOStream();

/**
 * Provides access to the stdout mutex.
 */
chibios_rt::Mutex& getStdIOMutex();

/**
 * Emergency termination hook that can be overriden by the application.
 * The hook must return immediately after bringing the hardware into a safe state.
 */
extern void applicationHaltHook(void);

/**
 * Replacement for chThdSleepUntil() that accepts timestamps from the past.
 * http://sourceforge.net/p/chibios/bugs/292/#ec7c
 */
void sleepUntilChTime(systime_t sleep_until);

/**
 * After this function is invoked, @ref isRebootRequested() will be returning true.
 */
void requestReboot();

/**
 * Returns true if the application must reboot.
 */
bool isRebootRequested();


namespace impl_
{

class MutexLockerImpl
{
    chibios_rt::Mutex& mutex_;
public:
    MutexLockerImpl(chibios_rt::Mutex& m) : mutex_(m)
    {
        mutex_.lock();
    }
    ~MutexLockerImpl()
    {
        mutex_.unlock();
    }
};

class CriticalSectionLockerImpl
{
    volatile const syssts_t st_ = chSysGetStatusAndLockX();
public:
    ~CriticalSectionLockerImpl() { chSysRestoreStatusX(st_); }
};

} // namespace impl_

/**
 * RAII mutex locker.
 * Must be volatile in order to prevent the optimizer from throwing it away.
 */
using MutexLocker = volatile impl_::MutexLockerImpl;

/**
 * RAII critical section locker.
 * Must be volatile in order to prevent the optimizer from throwing it away.
 */
using CriticalSectionLocker = volatile impl_::CriticalSectionLockerImpl;

/**
 * Converts any signed or unsigned integer to string and returns it by value.
 * The argument must be integer, otherwise the call will be rejected by SFINAE.
 * Usage examples:
 *      intToString(var)
 *      intToString<16>(var)
 *      intToString<2>(var).c_str()
 * It is safe to obtain a reference to the returned string and pass it to another function as an argument,
 * which enables use cases like this (this example is somewhat made up, but it conveys the idea nicely):
 *      print("%s", intToString(123).c_str());
 * More info on rvalue references:
 *      https://herbsutter.com/2008/01/01/gotw-88-a-candidate-for-the-most-important-const/
 *      http://stackoverflow.com/questions/584824/guaranteed-lifetime-of-temporary-in-c
 */
template <
    int Radix = 10,
    typename T,
    typename std::enable_if<std::is_integral<T>::value>::type...
    >
inline auto intToString(T number)
{
    static constexpr char Alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    static_assert(Radix >= 1, "Radix must be positive");
    static_assert(Radix <= (sizeof(Alphabet) / sizeof(Alphabet[0])), "Radix is too large");

    // Plus 1 to round up, see the standard for details.
    static constexpr unsigned MaxChars =
        ((Radix >= 10) ? std::numeric_limits<T>::digits10 : std::numeric_limits<T>::digits) +
        1 + (std::is_signed<T>::value ? 1 : 0);

    class Container
    {
        std::uint_fast16_t offset_;
        char storage_[MaxChars + 1];   // Plus 1 because of zero termination.

    public:
        Container(T x) :
            offset_(MaxChars)          // Field initialization is not working in GCC in this context, not sure why.
        {
            storage_[offset_] = '\0';

            do
            {
                assert(offset_ > 0);

                if (std::is_signed<T>::value)  // Should be converted to constexpr if.
                {
                    // We can't just do "x = -x", because it would break on int8_t(-128), int16_t(-32768), etc.
                    auto residual = std::int_fast8_t(x % Radix);
                    if (residual < 0)
                    {
                        // Should never happen - since C++11, neg % pos --> pos
                        residual = -residual;
                    }

                    storage_[--offset_] = Alphabet[residual];

                    // Signed integers are really tricky sometimes.
                    // We must not mix negative with positive to avoid implementation-defined behaviors.
                    x = (x < 0) ? -(x / -Radix) : (x / Radix);
                }
                else
                {
                    // Fast branch for unsigned arguments.
                    storage_[--offset_] = Alphabet[x % Radix];
                    x /= Radix;
                }
            }
            while (x != 0);

            if (std::is_signed<T>::value && (x < 0))    // Should be optimized with constexpr if.
            {
                assert(offset_ > 0);
                storage_[--offset_] = '-';
            }

            assert(offset_ < MaxChars);                 // Making sure there was no overflow.
        }

        const char* c_str() const { return &storage_[offset_]; }

        operator const char* () const { return this->c_str(); }
    };

    return Container(number);
}

}
