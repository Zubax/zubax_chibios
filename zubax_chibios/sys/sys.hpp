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
#include <functional>


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

#if defined(DEBUG_BUILD) && DEBUG_BUILD
# define DEBUG_LOG(...)         ::std::printf(__VA_ARGS__)
#else
# define DEBUG_LOG(...)         ((void)0)
#endif


namespace os
{
/**
 * A standard output helper that adds the name of the calling module before the message.
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
 * This delegate is invoked when the OS needs to emit a standard output.
 * It accepts a pointer to the data and the number of data bytes to write,
 * and returns whether all of the data could be written. If not, rest of the write operation will be aborted.
 * Normally, the handler should never block.
 * @ref setStandardOutputSink().
 */
using StandardOutputSink = std::function<bool (const std::uint8_t*, std::size_t)>;

/**
 * Assigns an application-specific sink for stdout.
 * The new sink will replace the default one.
 * The default sink simply prints all data into the system default UART port.
 * Access to the sink is always serialized through a global mutex, i.e. it is guaranteed that only one
 * thread can access the sink at any given time.
 * Assign an empty function to restore the default sink.
 * Note that the library may segment writes into multiple sequential invocations of the sink.
 */
void setStandardOutputSink(const StandardOutputSink& sink);

/**
 * Emergency termination hook that can be overridden by the application.
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
        DEBUG_LOG("OS: TemporaryPriorityChanger[%s]: Changed %d --> %d\n",
                  chThdGetSelfX()->name, int(old_priority_), int(new_priority));
    }

    ~TemporaryPriorityChangerImpl()
    {
        chibios_rt::BaseThread::setPriority(old_priority_);
        DEBUG_LOG("OS: TemporaryPriorityChanger[%s]: Restored %d\n",
                  chThdGetSelfX()->name, int(old_priority_));
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
