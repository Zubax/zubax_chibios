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

static bool defaultSink(const std::uint8_t*, std::size_t);

static chibios_rt::Mutex g_mutex;
static StandardOutputSink g_sink{&defaultSink};

// Sink invocations are guaranteed to be protected by the mutex, so no extra locking is needed.
static bool defaultSink(const std::uint8_t* const data, const std::size_t sz)
{
    return chnWriteTimeout(&STDOUT_SD, data, sz, MS2ST(10)) == sz;
}

static std::size_t writeExpandingCrLf(const char* str)
{
    std::size_t ret = 0;
    const char* end = str;

    while (*str != '\0')
    {
        if ((*end == '\n') ||
            (*end == '\0'))
        {
            if (end != str)
            {
                const std::size_t range = end - str;
                if (!g_sink(reinterpret_cast<const std::uint8_t*>(str), range))
                {
                    break;
                }

                ret += range;
                str += range;
            }

            if (*end == '\n')
            {
                if (!g_sink(reinterpret_cast<const std::uint8_t*>("\r\n"), 2))
                {
                    break;
                }

                ret += 2U;
                str++;
            }
        }

        end++;
    }

    return ret;
}

static std::size_t genericPrint(const char* format, va_list vl)
{
    MutexLocker locker(g_mutex);

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
    (void) vsnprintf(buffer, sizeof(buffer), format, vl);
#endif

    /*
     * Writing the buffer replacing "\n" --> "\r\n"
     */
    return writeExpandingCrLf(buffer);
}


void Logger::println(const char* format, ...)
{
    MutexLocker locker(g_mutex);

    writeExpandingCrLf(name_);
    writeExpandingCrLf(": ");

    va_list vl;
    va_start(vl, format);
    genericPrint(format, vl);
    va_end(vl);

    writeExpandingCrLf("\n");
}

void Logger::puts(const char* line)
{
    MutexLocker locker(g_mutex);
    writeExpandingCrLf(name_);
    writeExpandingCrLf(": ");
    writeExpandingCrLf(line);
    writeExpandingCrLf("\n");
}


void setStandardOutputSink(const StandardOutputSink& sink)
{
    MutexLocker locker(g_mutex);
    if (sink)
    {
        g_sink = sink;
    }
    else
    {
        g_sink = defaultSink;
    }
}

} // namespace os

extern "C"
{

using namespace os;

int printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    int ret = int(genericPrint(format, vl));
    va_end(vl);
    return ret;
}

int vprintf(const char* format, va_list vl)
{
    return int(genericPrint(format, vl));
}

int puts(const char* str)
{
    MutexLocker locker(g_mutex);
    int ret = int(writeExpandingCrLf(str));
    ret += int(writeExpandingCrLf("\n"));
    return ret;
}

}
