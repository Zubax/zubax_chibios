/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Disables hardware debug port (SWD/JTAG)
 * In RELEASE builds should be called a few seconds after boot
 */
void sysDisableDebugPort(void);

#ifdef __cplusplus
}
#endif
