/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#ifdef SYSTEM_HALT_HOOK
#  error SYSTEM_HALT_HOOK must not be defined by the application
#endif

void sysHaltHook_(void);
#define SYSTEM_HALT_HOOK                sysHaltHook_

#include <chibios/os/kernel/templates/chconf.h>
