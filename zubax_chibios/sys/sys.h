/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#if !DEBUG_BUILD && !RELEASE_BUILD
#  error Either DEBUG_BUILD or RELEASE_BUILD must be defined
#endif

#include <zubax_chibios/sys/assert_always.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NuttX-like console print; should be used instead of printf()/chprintf()
 * TODO: use type safe version for C++.
 */
__attribute__ ((format (printf, 1, 2)))
void lowsyslog(const char* format, ...);

/**
 * Emergency termination hook that can be overriden by the application.
 */
extern void sysApplicationHaltHook(void);

__attribute__((noreturn))
void sysPanic(const char* msg);

#ifdef __cplusplus
}
#endif
