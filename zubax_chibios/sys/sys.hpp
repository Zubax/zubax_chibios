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


namespace os
{

static constexpr unsigned DefaultStdIOByteWriteTimeoutMSec = 1;

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

}
