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
 * Default overrides.
 */
#ifndef CH_CFG_USE_MUTEXES_RECURSIVE
#define CH_CFG_USE_MUTEXES_RECURSIVE            TRUE
#endif

#ifndef CH_CFG_USE_MEMPOOLS
#define CH_CFG_USE_MEMPOOLS                     FALSE
#endif

#ifndef CH_CFG_USE_HEAP
#define CH_CFG_USE_HEAP                         FALSE
#endif

#ifndef CH_CFG_USE_DYNAMIC
#define CH_CFG_USE_DYNAMIC                      FALSE
#endif

/*
 * Debug support.
 */
#ifndef CH_DBG_FILL_THREADS
#define CH_DBG_FILL_THREADS                     TRUE
#endif

#if defined(DEBUG_BUILD) && DEBUG_BUILD
#   define CH_DBG_SYSTEM_STATE_CHECK            TRUE
#   define CH_DBG_ENABLE_CHECKS                 TRUE
#   define CH_DBG_ENABLE_ASSERTS                TRUE
#   define CH_DBG_ENABLE_STACK_CHECK            TRUE
#   define CH_DBG_FILL_THREADS                  TRUE
#elif defined(RELEASE_BUILD) && RELEASE_BUILD
#else
#   error "Invalid configuration: Either DEBUG_BUILD or RELEASE_BUILD must be true"
#endif

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
