/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void watchdogInit(void);

bool watchdogTriggeredLastReset(void);

int watchdogCreate(unsigned timeout_ms);

void watchdogReset(int id);

#ifdef __cplusplus
}
#endif
