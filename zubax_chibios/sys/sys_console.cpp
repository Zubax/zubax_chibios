/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "sys.hpp"
#include <ch.hpp>
#include <hal.h>
#include <chprintf.h>
#include <memstreams.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdarg>


namespace os
{

static MUTEX_DECL(_mutex);  ///< Doesn't require initialization
static char _buffer[256];

static BaseSequentialStream* _stdout_stream = (BaseSequentialStream*)&STDOUT_SD;

static void writeExpandingCrLf(BaseSequentialStream* stream, const char* str)
{
    for (const char* pc = str; *pc != '\0'; pc++)
    {
        if (*pc == '\n')
        {
            chSequentialStreamPut(stream, '\r');
        }
        chSequentialStreamPut(stream, *pc);
    }
}

static void genericPrint(BaseSequentialStream* stream, const char* format, va_list vl)
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
    writeExpandingCrLf(stream, _buffer);
    chMtxUnlock(&_mutex);
}

void lowsyslog(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint((BaseSequentialStream*)&STDOUT_SD, format, vl);   // Lowsyslog is always directed to STDOUT_SD
    va_end(vl);
}


void setStdOutStream(BaseSequentialStream* stream)
{
    _stdout_stream = stream;
}

} // namespace os

extern "C"
{

using namespace os;

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint(_stdout_stream, format, vl);
    va_end(vl);
    return std::strlen(format);   // This is not standard compliant, but ain't nobody cares about what printf() returns
}

int vprintf(const char* format, va_list vl)
{
    genericPrint(_stdout_stream, format, vl);
    return std::strlen(format);
}

int puts(const char* str)
{
    chMtxLock(&_mutex);
    writeExpandingCrLf(_stdout_stream, str);
    writeExpandingCrLf(_stdout_stream, "\n");
    chMtxUnlock(&_mutex);
    return std::strlen(str) + 2;
}

}
