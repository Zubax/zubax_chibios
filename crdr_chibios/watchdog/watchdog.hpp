/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#pragma once

#include <cassert>
#include <crdr_chibios/sys/assert_always.h>
#include "watchdog.h"

namespace crdr_chibios
{
namespace watchdog
{

class Timer
{
    int id_ = -1;

public:
    bool isStarted() const { return id_ >= 0; }

    void startMSec(int timeout_ms)
    {
        if (!isStarted())
        {
            id_ = ::watchdogCreate(timeout_ms);
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

}
}
