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


#ifndef STRINGIZE
#  define STRINGIZE2(x)         #x
#  define STRINGIZE(x)          STRINGIZE2(x)
#endif

#if defined(DEBUG_BUILD) && DEBUG_BUILD
# define MAKE_ASSERT_MSG_(x)      (__FILE__ ":" STRINGIZE(__LINE__) ":" STRINGIZE(x))
#else
# define MAKE_ASSERT_MSG_(x)      (STRINGIZE(__LINE__) ":" STRINGIZE(x))
#endif

#define ASSERT_ALWAYS(x)                                    \
    do {                                                    \
        if ((x) == 0) {                                     \
            chSysHalt(MAKE_ASSERT_MSG_(x));                 \
        }                                                   \
    } while (0)

namespace os
{
/**
 * NuttX-like console print; should be used instead of printf()/chprintf()
 * TODO: use type safe version based on variadic templates.
 */
__attribute__ ((format (printf, 1, 2)))
void lowsyslog(const char* format, ...);

/**
 * Changes current stdout stream.
 */
void setStdOutStream(BaseSequentialStream* stream);

/**
 * Emergency termination hook that can be overriden by the application.
 */
extern void applicationHaltHook(void);

/**
 * Replacement for chThdSleepUntil() that accepts timestamps from the past.
 * http://sourceforge.net/p/chibios/bugs/292/#ec7c
 */
void sleepUntilChTime(systime_t sleep_until);

__attribute__((noreturn))
void panic(const char* msg);

}
