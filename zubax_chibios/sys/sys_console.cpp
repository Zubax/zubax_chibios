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

static chibios_rt::Mutex mutex_;
static char buffer_[256];

static BaseSequentialStream* stdout_stream_ = (BaseSequentialStream*)&STDOUT_SD;

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
    MutexLocker locker(mutex_);

    /*
     * Printing the string into the buffer using chvprintf()
     */
    MemoryStream ms;
    msObjectInit(&ms, (uint8_t*)buffer_, sizeof(buffer_), 0);

    BaseSequentialStream* chp = (BaseSequentialStream*)&ms;
    chvprintf(chp, format, vl);

    chSequentialStreamPut(chp, 0);

    /*
     * Writing the buffer replacing "\n" --> "\r\n"
     */
    writeExpandingCrLf(stream, buffer_);
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
    stdout_stream_ = stream;
}

} // namespace os

extern "C"
{

using namespace os;

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint(stdout_stream_, format, vl);
    va_end(vl);
    return std::strlen(format);   // This is not standard compliant, but ain't nobody cares about what printf() returns
}

int vprintf(const char* format, va_list vl)
{
    genericPrint(stdout_stream_, format, vl);
    return std::strlen(format);
}

int puts(const char* str)
{
    MutexLocker locker(mutex_);
    writeExpandingCrLf(stdout_stream_, str);
    writeExpandingCrLf(stdout_stream_, "\n");
    return std::strlen(str) + 2;
}

}
