/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/*
 * This file has been modified by Zubax Robotics <zubax.com>.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 *
 * Replacement regexp for KWrite:
 *      Find:        (#define ([A-Za-z_][A-Za-z0-9_]+)[\ \t]+[^\\]+$)
 *      Replace:     #ifndef \2\n\1\n#endif
 */

/**
 * @file    templates/chconf.h
 * @brief   Configuration file template.
 * @details A copy of this file must be placed in each project directory, it
 *          contains the application specific kernel settings.
 *
 * @addtogroup config
 * @details Kernel related settings and hooks.
 * @{
 */

#pragma once

#include "chconf_defaults.h"

/*===========================================================================*/
/**
 * @name System timers settings
 * @{
 */
/*===========================================================================*/

/**
 * @brief   System time counter resolution.
 * @note    Allowed values are 16 or 32 bits.
 */
#ifndef CH_CFG_ST_RESOLUTION
#define CH_CFG_ST_RESOLUTION                32
#endif

/**
 * @brief   System tick frequency.
 * @details Frequency of the system timer that drives the system ticks. This
 *          setting also defines the system tick time unit.
 */
#ifndef CH_CFG_ST_FREQUENCY
#define CH_CFG_ST_FREQUENCY                 10000
#endif

/**
 * @brief   Time delta constant for the tick-less mode.
 * @note    If this value is zero then the system uses the classic
 *          periodic tick. This value represents the minimum number
 *          of ticks that is safe to specify in a timeout directive.
 *          The value one is not valid, timeouts are rounded up to
 *          this value.
 */
#ifndef CH_CFG_ST_TIMEDELTA
#define CH_CFG_ST_TIMEDELTA                 2
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Kernel parameters and options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Round robin interval.
 * @details This constant is the number of system ticks allowed for the
 *          threads before preemption occurs. Setting this value to zero
 *          disables the preemption for threads with equal priority and the
 *          round robin becomes cooperative. Note that higher priority
 *          threads can still preempt, the kernel is always preemptive.
 * @note    Disabling the round robin preemption makes the kernel more compact
 *          and generally faster.
 * @note    The round robin preemption is not supported in tickless mode and
 *          must be set to zero in that case.
 */
#ifndef CH_CFG_TIME_QUANTUM
#define CH_CFG_TIME_QUANTUM                 0
#endif

/**
 * @brief   Managed RAM size.
 * @details Size of the RAM area to be managed by the OS. If set to zero
 *          then the whole available RAM is used. The core memory is made
 *          available to the heap allocator and/or can be used directly through
 *          the simplified core memory allocator.
 *
 * @note    In order to let the OS manage the whole RAM the linker script must
 *          provide the @p __heap_base__ and @p __heap_end__ symbols.
 * @note    Requires @p CH_CFG_USE_MEMCORE.
 */
#ifndef CH_CFG_MEMCORE_SIZE
#define CH_CFG_MEMCORE_SIZE                 0
#endif

/**
 * @brief   Idle thread automatic spawn suppression.
 * @details When this option is activated the function @p chSysInit()
 *          does not spawn the idle thread. The application @p main()
 *          function becomes the idle thread and must implement an
 *          infinite loop.
 */
#ifndef CH_CFG_NO_IDLE_THREAD
#define CH_CFG_NO_IDLE_THREAD               FALSE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Performance options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   OS optimization.
 * @details If enabled then time efficient rather than space efficient code
 *          is used when two possible implementations exist.
 *
 * @note    This is not related to the compiler optimization options.
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_OPTIMIZE_SPEED
#define CH_CFG_OPTIMIZE_SPEED               TRUE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Subsystem options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Time Measurement APIs.
 * @details If enabled then the time measurement APIs are included in
 *          the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_TM
#define CH_CFG_USE_TM                       TRUE
#endif

/**
 * @brief   Threads registry APIs.
 * @details If enabled then the registry APIs are included in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_REGISTRY
#define CH_CFG_USE_REGISTRY                 TRUE
#endif

/**
 * @brief   Threads synchronization APIs.
 * @details If enabled then the @p chThdWait() function is included in
 *          the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_WAITEXIT
#define CH_CFG_USE_WAITEXIT                 TRUE
#endif

/**
 * @brief   Semaphores APIs.
 * @details If enabled then the Semaphores APIs are included in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_SEMAPHORES
#define CH_CFG_USE_SEMAPHORES               TRUE
#endif

/**
 * @brief   Semaphores queuing mode.
 * @details If enabled then the threads are enqueued on semaphores by
 *          priority rather than in FIFO order.
 *
 * @note    The default is @p FALSE. Enable this if you have special
 *          requirements.
 * @note    Requires @p CH_CFG_USE_SEMAPHORES.
 */
#ifndef CH_CFG_USE_SEMAPHORES_PRIORITY
#define CH_CFG_USE_SEMAPHORES_PRIORITY      FALSE
#endif

/**
 * @brief   Mutexes APIs.
 * @details If enabled then the mutexes APIs are included in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_MUTEXES
#define CH_CFG_USE_MUTEXES                  TRUE
#endif

/**
 * @brief   Enables recursive behavior on mutexes.
 * @note    Recursive mutexes are heavier and have an increased
 *          memory footprint.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_CFG_USE_MUTEXES_RECURSIVE
#define CH_CFG_USE_MUTEXES_RECURSIVE        TRUE
#endif

/**
 * @brief   Conditional Variables APIs.
 * @details If enabled then the conditional variables APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_MUTEXES.
 */
#ifndef CH_CFG_USE_CONDVARS
#define CH_CFG_USE_CONDVARS                 FALSE
#endif

/**
 * @brief   Conditional Variables APIs with timeout.
 * @details If enabled then the conditional variables APIs with timeout
 *          specification are included in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_CONDVARS.
 */
#ifndef CH_CFG_USE_CONDVARS_TIMEOUT
#define CH_CFG_USE_CONDVARS_TIMEOUT         CH_CFG_USE_CONDVARS
#endif

/**
 * @brief   Events Flags APIs.
 * @details If enabled then the event flags APIs are included in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_EVENTS
#define CH_CFG_USE_EVENTS                   TRUE
#endif

/**
 * @brief   Events Flags APIs with timeout.
 * @details If enabled then the events APIs with timeout specification
 *          are included in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_EVENTS.
 */
#ifndef CH_CFG_USE_EVENTS_TIMEOUT
#define CH_CFG_USE_EVENTS_TIMEOUT           CH_CFG_USE_EVENTS
#endif

/**
 * @brief   Synchronous Messages APIs.
 * @details If enabled then the synchronous messages APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_MESSAGES
#define CH_CFG_USE_MESSAGES                 FALSE
#endif

/**
 * @brief   Synchronous Messages queuing mode.
 * @details If enabled then messages are served by priority rather than in
 *          FIFO order.
 *
 * @note    The default is @p FALSE. Enable this if you have special
 *          requirements.
 * @note    Requires @p CH_CFG_USE_MESSAGES.
 */
#ifndef CH_CFG_USE_MESSAGES_PRIORITY
#define CH_CFG_USE_MESSAGES_PRIORITY        FALSE
#endif

/**
 * @brief   Mailboxes APIs.
 * @details If enabled then the asynchronous messages (mailboxes) APIs are
 *          included in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_SEMAPHORES.
 */
#ifndef CH_CFG_USE_MAILBOXES
#define CH_CFG_USE_MAILBOXES                FALSE
#endif

/**
 * @brief   I/O Queues APIs.
 * @details If enabled then the I/O queues APIs are included in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_QUEUES
#define CH_CFG_USE_QUEUES                   TRUE
#endif

/**
 * @brief   Core Memory Manager APIs.
 * @details If enabled then the core memory manager APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_MEMCORE
#define CH_CFG_USE_MEMCORE                  TRUE
#endif

/**
 * @brief   Heap Allocator APIs.
 * @details If enabled then the memory heap allocator APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_MEMCORE and either @p CH_CFG_USE_MUTEXES or
 *          @p CH_CFG_USE_SEMAPHORES.
 * @note    Mutexes are recommended.
 */
#ifndef CH_CFG_USE_HEAP
#define CH_CFG_USE_HEAP                     FALSE
#endif

/**
 * @brief   Memory Pools Allocator APIs.
 * @details If enabled then the memory pools allocator APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 */
#ifndef CH_CFG_USE_MEMPOOLS
#define CH_CFG_USE_MEMPOOLS                 FALSE
#endif

/**
 * @brief   Dynamic Threads APIs.
 * @details If enabled then the dynamic threads creation APIs are included
 *          in the kernel.
 *
 * @note    The default is @p TRUE.
 * @note    Requires @p CH_CFG_USE_WAITEXIT.
 * @note    Requires @p CH_CFG_USE_HEAP and/or @p CH_CFG_USE_MEMPOOLS.
 */
#ifndef CH_CFG_USE_DYNAMIC
#define CH_CFG_USE_DYNAMIC                  FALSE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Debug options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Debug option, kernel statistics.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_STATISTICS
#define CH_DBG_STATISTICS                   TRUE
#endif

/**
 * @brief   Debug option, system state check.
 * @details If enabled the correct call protocol for system APIs is checked
 *          at runtime.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_SYSTEM_STATE_CHECK
#define CH_DBG_SYSTEM_STATE_CHECK           TRUE
#endif

/**
 * @brief   Debug option, parameters checks.
 * @details If enabled then the checks on the API functions input
 *          parameters are activated.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_ENABLE_CHECKS
#define CH_DBG_ENABLE_CHECKS                TRUE
#endif

/**
 * @brief   Debug option, consistency checks.
 * @details If enabled then all the assertions in the kernel code are
 *          activated. This includes consistency checks inside the kernel,
 *          runtime anomalies and port-defined checks.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_ENABLE_ASSERTS
#define CH_DBG_ENABLE_ASSERTS               TRUE
#endif

/**
 * @brief   Debug option, trace buffer.
 * @details If enabled then the context switch circular trace buffer is
 *          activated.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_ENABLE_TRACE
#define CH_DBG_ENABLE_TRACE                 FALSE
#endif

/**
 * @brief   Debug option, stack checks.
 * @details If enabled then a runtime stack check is performed.
 *
 * @note    The default is @p FALSE.
 * @note    The stack check is performed in a architecture/port dependent way.
 *          It may not be implemented or some ports.
 * @note    The default failure mode is to halt the system with the global
 *          @p panic_msg variable set to @p NULL.
 */
#ifndef CH_DBG_ENABLE_STACK_CHECK
#define CH_DBG_ENABLE_STACK_CHECK           TRUE
#endif

/**
 * @brief   Debug option, stacks initialization.
 * @details If enabled then the threads working area is filled with a byte
 *          value when a thread is created. This can be useful for the
 *          runtime measurement of the used stack.
 *
 * @note    The default is @p FALSE.
 */
#ifndef CH_DBG_FILL_THREADS
#define CH_DBG_FILL_THREADS                 TRUE
#endif

/**
 * @brief   Debug option, threads profiling.
 * @details If enabled then a field is added to the @p thread_t structure that
 *          counts the system ticks occurred while executing the thread.
 *
 * @note    The default is @p FALSE.
 * @note    This debug option is not currently compatible with the
 *          tickless mode.
 */
#ifndef CH_DBG_THREADS_PROFILING
#define CH_DBG_THREADS_PROFILING            FALSE
#endif

#if CH_DBG_THREADS_PROFILING
#error "CH_DBG_THREADS_PROFILING is superseded with CH_DBG_STATISTICS"
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Kernel hooks
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Threads descriptor structure extension.
 * @details User fields added to the end of the @p thread_t structure.
 */
#ifndef CH_CFG_THREAD_EXTRA_FIELDS
#define CH_CFG_THREAD_EXTRA_FIELDS                                          \
  /* Add threads custom fields here.*/
#endif

/**
 * @brief   Threads initialization hook.
 * @details User initialization code added to the @p chThdInit() API.
 *
 * @note    It is invoked from within @p chThdInit() and implicitly from all
 *          the threads creation APIs.
 */
#ifndef CH_CFG_THREAD_INIT_HOOK
#define CH_CFG_THREAD_INIT_HOOK(tp) {                                       \
  /* Add threads initialization code here.*/                                \
}
#endif

/**
 * @brief   Threads finalization hook.
 * @details User finalization code added to the @p chThdExit() API.
 *
 * @note    It is inserted into lock zone.
 * @note    It is also invoked when the threads simply return in order to
 *          terminate.
 */
#ifndef CH_CFG_THREAD_EXIT_HOOK
#define CH_CFG_THREAD_EXIT_HOOK(tp) {                                       \
  /* Add threads finalization code here.*/                                  \
}
#endif

/**
 * @brief   Context switch hook.
 * @details This hook is invoked just before switching between threads.
 */
#ifndef CH_CFG_CONTEXT_SWITCH_HOOK
#define CH_CFG_CONTEXT_SWITCH_HOOK(ntp, otp) {                              \
  /* Context switch code here.*/                                            \
}
#endif

/**
 * @brief   Idle thread enter hook.
 * @note    This hook is invoked within a critical zone, no OS functions
 *          should be invoked from here.
 * @note    This macro can be used to activate a power saving mode.
 */
#ifndef CH_CFG_IDLE_ENTER_HOOK
#define CH_CFG_IDLE_ENTER_HOOK() {                                          \
}
#endif

/**
 * @brief   Idle thread leave hook.
 * @note    This hook is invoked within a critical zone, no OS functions
 *          should be invoked from here.
 * @note    This macro can be used to deactivate a power saving mode.
 */
#ifndef CH_CFG_IDLE_LEAVE_HOOK
#define CH_CFG_IDLE_LEAVE_HOOK() {                                          \
}
#endif

/**
 * @brief   Idle Loop hook.
 * @details This hook is continuously invoked by the idle thread loop.
 */
#ifndef CH_CFG_IDLE_LOOP_HOOK
#define CH_CFG_IDLE_LOOP_HOOK() {                                           \
  /* Idle loop code here.*/                                                 \
}
#endif

/**
 * @brief   System tick event hook.
 * @details This hook is invoked in the system tick handler immediately
 *          after processing the virtual timers queue.
 */
#ifndef CH_CFG_SYSTEM_TICK_HOOK
#define CH_CFG_SYSTEM_TICK_HOOK() {                                         \
  /* System tick event code here.*/                                         \
}
#endif

/**
 * @brief   System halt hook.
 * @details This hook is invoked in case to a system halting error before
 *          the system is halted.
 */
// Defined in Zubax ChibiOS
#ifndef CH_CFG_SYSTEM_HALT_HOOK
# error CH_CFG_SYSTEM_HALT_HOOK should be defined by Zubax ChibiOS
#endif

/** @} */

/*===========================================================================*/
/* Port-specific settings (override port settings defaulted in chcore.h).    */
/*===========================================================================*/

/** @} */
