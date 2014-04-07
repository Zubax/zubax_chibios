/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#include <ch.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <chprintf.h>
#include "sys.h"

__attribute__((weak))
void *__dso_handle;

#if !CH_DBG_ENABLED
const char *dbg_panic_msg;
#endif

void sysEmergencyPrint(const char* str);

void sysHaltHook_(void)
{
    sysApplicationHaltHook();

    port_disable();
    sysEmergencyPrint("\nPANIC [");
    const Thread *pthread = chThdSelf();
    if (pthread && pthread->p_name)
        sysEmergencyPrint(pthread->p_name);
    sysEmergencyPrint("] ");

    if (dbg_panic_msg != NULL)
        sysEmergencyPrint(dbg_panic_msg);
    sysEmergencyPrint("\n");

#if DEBUG
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        __asm volatile ("bkpt #0\n"); // Break into the debugger
    }
#endif
}

__attribute__((weak))
void sysApplicationHaltHook(void) { }

void __assert_func(const char* file, int line, const char* func, const char* expr)
{
    port_disable();

    char buf[128]; // We don't care about possible stack overflow because we're going to die anyway
    snprintf(buf, sizeof(buf), "%s:%i at %s(..): %s", file, line, func, expr);
    dbg_panic_msg = buf;

    chSysHalt();
    while (1) { }
}

void lowsyslog(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    chvprintf((BaseSequentialStream*)&(STDOUT_SD), format, vl);
    va_end(vl);
}

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    chvprintf((BaseSequentialStream*)&(STDOUT_SD), format, vl);
    va_end(vl);
    return strlen(format);   // This is not standard compliant, but ain't nobody cares about what printf() returns
}

int vprintf(const char* format, va_list vl)
{
    chvprintf((BaseSequentialStream*)&(STDOUT_SD), format, vl);
    return strlen(format);
}

int puts(const char* str)
{
    const int len = strlen(str);
    if (len)
        sdWrite(&STDOUT_SD, (uint8_t*)str, len); // this fires an assert() if len = 0, haha!
    sdWrite(&STDOUT_SD, (uint8_t*)"\n", 1);
    return len + 1;
}

void _exit(int status)
{
    (void) status;
    chSysHalt();
    while (1) { }
}

pid_t _getpid(void) { return 1; }

void _kill(pid_t id) { (void) id; }

/// From unistd
int usleep(useconds_t useconds)
{
    chThdSleepMicroseconds(useconds);
    return 0;
}

/// From unistd
unsigned sleep(unsigned int seconds)
{
    chThdSleepSeconds(seconds);
    return 0;
}
