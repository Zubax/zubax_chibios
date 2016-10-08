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

static int writeExpandingCrLf(::BaseChannel* stream, unsigned timeout_msec, const char* str)
{
    const auto timeout = MS2ST(timeout_msec);
    int ret = 0;

    for (const char* pc = str; *pc != '\0'; pc++)
    {
        if (*pc == '\n')
        {
            if (MSG_OK != chnPutTimeout(stream, '\r', timeout))
            {
                break;
            }
            ret++;
        }
        if (MSG_OK != chnPutTimeout(stream, *pc, timeout))
        {
            break;
        }
        ret++;
    }

    return ret;
}

static int genericPrint(::BaseChannel* stream, unsigned timeout_msec, const char* format, va_list vl)
{
    MutexLocker locker(mutex_);

    /*
     * Printing the string into the buffer
     */
    static char buffer[256];

#if defined(OS_USE_CHPRINTF) && OS_USE_CHPRINTF
    MemoryStream ms;
    msObjectInit(&ms, (uint8_t*)buffer, sizeof(buffer), 0);
    ::BaseSequentialStream* chp = (::BaseSequentialStream*)&ms;
    chvprintf(chp, format, vl);
    chSequentialStreamPut(chp, 0);
#else
    using namespace std;
    vsnprintf(buffer, sizeof(buffer), format, vl);
#endif

    /*
     * Writing the buffer replacing "\n" --> "\r\n"
     */
    return writeExpandingCrLf(stream, timeout_msec, buffer);
}


// Lowsyslog config is fixed
static constexpr unsigned LowsyslogWriteTimeoutMSec = 1000;

void lowsyslog(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    genericPrint((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, format, vl);
    va_end(vl);
}


void Logger::println(const char* format, ...)
{
    MutexLocker locker(mutex_);

    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, name_);
    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, ": ");

    va_list vl;
    va_start(vl, format);
    genericPrint((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, format, vl);
    va_end(vl);

    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, "\n");
}

void Logger::puts(const char* line)
{
    MutexLocker locker(mutex_);

    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, name_);
    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, ": ");
    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, line);
    writeExpandingCrLf((::BaseChannel*)&STDOUT_SD, LowsyslogWriteTimeoutMSec, "\n");
}


static unsigned stdio_byte_write_timeout_msec_ = DefaultStdIOByteWriteTimeoutMSec;
static ::BaseChannel* stdio_stream_ = (::BaseChannel*)&STDOUT_SD;

void setStdIOStream(::BaseChannel* stream, unsigned byte_write_timeout_msec)
{
    MutexLocker locker(mutex_);
    assert(stream != nullptr);
    stdio_stream_ = stream;
    stdio_byte_write_timeout_msec_ = byte_write_timeout_msec;
}

::BaseChannel* getStdIOStream()
{
    return stdio_stream_;
}

chibios_rt::Mutex& getStdIOMutex()
{
    return mutex_;
}

} // namespace os

extern "C"
{

using namespace os;

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    int ret = genericPrint(stdio_stream_, stdio_byte_write_timeout_msec_, format, vl);
    va_end(vl);
    return ret;
}

int vprintf(const char* format, va_list vl)
{
    return genericPrint(stdio_stream_, stdio_byte_write_timeout_msec_, format, vl);
}

int puts(const char* str)
{
    MutexLocker locker(mutex_);
    int ret = writeExpandingCrLf(stdio_stream_, stdio_byte_write_timeout_msec_, str);
    ret += writeExpandingCrLf(stdio_stream_, stdio_byte_write_timeout_msec_, "\n");
    return ret;
}

}
