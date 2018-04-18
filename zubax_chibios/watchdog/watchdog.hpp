/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <cassert>
#include <zubax_chibios/os.hpp>
#include "watchdog.h"
#include <chrono>


namespace os
{
namespace watchdog
{

class Timer
{
    int id_ = -1;

public:
    bool isStarted() const { return id_ >= 0; }

    template <typename Rep, typename Period>
    void start(const std::chrono::duration<Rep, Period> timeout)
    {
        if (!isStarted())
        {
            id_ = ::watchdogCreate(unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
            ASSERT_ALWAYS(isStarted());
        }
        else
        {
            assert(0);
        }
    }

    void reset()
    {
        assert(isStarted());
        ::watchdogReset(id_);
    }
};

static inline void init()
{
    ::watchdogInit();
}

static inline bool wasLastResetTriggeredByWatchdog()
{
    return ::watchdogTriggeredLastReset();
}

}
}
