/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "sys.h"
#include <ch.h>
#include <chprintf.h>
#include <memstreams.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>


static MUTEX_DECL(_mutex);  ///< Doesn't require initialization
static char _buffer[256];


static void writeExpandingCrLf(const char* str)
{
    for (const char* pc = str; *pc != '\0'; pc++)
    {
        if (*pc == '\n')
        {
            sdPut(&STDOUT_SD, '\r');
        }
        sdPut(&STDOUT_SD, *pc);
    }
}

static void genericPrint(const char* format, va_list vl)
{
    /*
     * Printing the string into the buffer using chvprintf()
     */
    MemoryStream ms;
    msObjectInit(&ms, (uint8_t*)_buffer, sizeof(_buffer), 0);

    BaseSequentialStream* chp = (BaseSequentialStream*)&ms;
    chvprintf(chp, format, vl);

    chSequentialStreamPut(chp, 0);

    /*
     * Writing the buffer replacing "\n" --> "\r\n"
     */
    chMtxLock(&_mutex);
    writeExpandingCrLf(_buffer);
    chMtxUnlock();
}

void lowsyslog(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint(format, vl);
    va_end(vl);
}

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint(format, vl);
    va_end(vl);
    return strlen(format);   // This is not standard compliant, but ain't nobody cares about what printf() returns
}

int vprintf(const char* format, va_list vl)
{
    genericPrint(format, vl);
    return strlen(format);
}

int puts(const char* str)
{
    chMtxLock(&_mutex);
    writeExpandingCrLf(str);
    writeExpandingCrLf("\n");
    chMtxUnlock();
    return strlen(str) + 2;
}
