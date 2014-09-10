/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include <ch.h>
#include <hal.h>

#if !CH_DBG_ENABLED
extern const char *dbg_panic_msg;
#endif

#ifndef STRINGIZE
#  define STRINGIZE2(x)   #x
#  define STRINGIZE(x)    STRINGIZE2(x)
#endif

#define MAKE_ASSERT_MSG_() __FILE__ ":" STRINGIZE(__LINE__)

#define ASSERT_ALWAYS(x)                                    \
    do {                                                    \
        if ((x) == 0) {                                     \
            dbg_panic_msg = MAKE_ASSERT_MSG_();             \
            chSysHalt();                                    \
        }                                                   \
    } while (0)
