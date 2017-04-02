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


#ifndef STRINGIZE
#  define STRINGIZE2(x)         #x
#  define STRINGIZE(x)          STRINGIZE2(x)
#endif

#if (defined(DEBUG_BUILD) && DEBUG_BUILD) || !(defined(AGGRESSIVE_SIZE_OPTIMIZATION) && AGGRESSIVE_SIZE_OPTIMIZATION)
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
 * A helper that works like @ref lowsyslog() except that it adds the name of the calling module
 * before the message.
 */
class Logger
{
    const char* const name_;

public:
    Logger(const char* module_name) : name_(module_name) { }

    __attribute__ ((format (printf, 2, 3)))
    void println(const char* format, ...);

    void puts(const char* line);

    const char* getName() const { return name_; }
};

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

class TemporaryPriorityChangerImpl
{
    volatile const ::tprio_t old_priority_;

public:
    TemporaryPriorityChangerImpl(const ::tprio_t new_priority) :
        old_priority_(chibios_rt::BaseThread::setPriority(new_priority))
    {
#if CH_CFG_USE_REGISTRY
        DEBUG_LOG("OS: TemporaryPriorityChanger[%s]: Changed %d --> %d\n",
                  chThdGetSelfX()->p_name, int(old_priority_), int(new_priority));
#endif
    }

    ~TemporaryPriorityChangerImpl()
    {
        chibios_rt::BaseThread::setPriority(old_priority_);
#if CH_CFG_USE_REGISTRY
        DEBUG_LOG("OS: TemporaryPriorityChanger[%s]: Restored %d\n",
                  chThdGetSelfX()->p_name, int(old_priority_));
#endif
    }
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
 * RAII priority adjustment helper.
 * Must be volatile in order to prevent the optimizer from throwing it away.
 */
using TemporaryPriorityChanger = volatile impl_::TemporaryPriorityChangerImpl;

}
