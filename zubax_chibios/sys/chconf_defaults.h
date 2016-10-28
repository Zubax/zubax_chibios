/*
 * Copyright (c) 2015 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * Zubax ChibiOS specific default settings.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Halt hook override.
 * Zubax ChibiOS needs it for panic output.
 */
#ifndef __ASSEMBLER__
extern void zchSysHaltHook(const char* reason);
#endif
#define CH_CFG_SYSTEM_HALT_HOOK(reason)         zchSysHaltHook(reason)

/*
 * Component defaults.
 */
// Max line length must be large enough to accommodate signature installation command with Base64 input.
#ifndef SHELL_MAX_LINE_LENGTH
#define SHELL_MAX_LINE_LENGTH                   200
#endif

#ifdef __cplusplus
}
#endif
