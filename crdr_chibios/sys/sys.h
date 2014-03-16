/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#pragma once

#if !DEBUG && !RELEASE
#  error Either DEBUG or RELEASE must be defined
#endif

#include <crdr_chibios/sys/assert_always.h>

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

#ifdef __cplusplus
}
#endif
