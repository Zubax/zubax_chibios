/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "sys.h"
#include <ch.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

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
    {
        sysEmergencyPrint(pthread->p_name);
    }
    sysEmergencyPrint("] ");

    if (dbg_panic_msg != NULL)
    {
        sysEmergencyPrint(dbg_panic_msg);
    }
    sysEmergencyPrint("\n");

#if DEBUG_BUILD
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        __asm volatile ("bkpt #0\n"); // Break into the debugger
    }
#endif
}

void sysPanic(const char* msg)
{
    dbg_panic_msg = msg;
    chSysHalt();
    while (1) { }
}

__attribute__((weak))
void sysApplicationHaltHook(void) { }

void sysSleepUntilChTime(systime_t sleep_until)
{
    chSysLock();
    sleep_until -= chTimeNow();
    if (((int)sleep_until) > 0)
    {
        chThdSleepS(sleep_until);
    }
    chSysUnlock();

#if DEBUG_BUILD
    if (((int)sleep_until) < 0)
    {
        lowsyslog("%s: Lag %d ts\n", chThdSelf()->p_name, (int)sleep_until);
    }
#endif
}

static void reverse(char* s)
{
    for (int i = 0, j = strlen(s) - 1; i < j; i++, j--)
    {
        const char c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

char* itoa(int n, char* pbuf)
{
    const int sign = n;
    if (sign < 0)
    {
        n = -n;
    }
    unsigned i = 0;
    do
    {
        pbuf[i++] = n % 10 + '0';
    }
    while ((n /= 10) > 0);
    if (sign < 0)
    {
        pbuf[i++] = '-';
    }
    pbuf[i] = '\0';
    reverse(pbuf);
    return pbuf;
}


void __assert_func(const char* file, int line, const char* func, const char* expr)
{
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    port_disable();

    char line_buf[11];
    itoa(line, line_buf);

    char buf[256]; // We don't care about possible stack overflow because we're going to die anyway
    char* ptr = buf;
    const unsigned size = sizeof(buf);
    unsigned pos = 0;

#define APPEND_MSG(text) { \
        (void)strncpy(ptr + pos, text, (size > pos) ? (size - pos) : 0); \
        pos += strlen(text); \
    }

    APPEND_MSG(file);
    APPEND_MSG(":");
    APPEND_MSG(line_buf);
    if (func != NULL)
    {
        APPEND_MSG(" ");
        APPEND_MSG(func);
    }
    APPEND_MSG(": ");
    APPEND_MSG(expr);

#undef APPEND_MSG

    dbg_panic_msg = buf;

    chSysHalt();
    while (1) { }
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
