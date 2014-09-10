/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#ifdef SYSTEM_HALT_HOOK
#  error SYSTEM_HALT_HOOK must not be defined by the application
#endif

#ifdef __cplusplus
extern "C" {
#endif

void sysHaltHook_(void);
#define SYSTEM_HALT_HOOK                sysHaltHook_

#ifdef __cplusplus
}
#endif

// These are required for C++ wrapper
#define CH_USE_MESSAGES     TRUE

#include <chibios/os/kernel/templates/chconf.h>
