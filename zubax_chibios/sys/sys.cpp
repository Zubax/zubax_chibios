/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "sys.hpp"
#include <ch.hpp>
#include <unistd.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <type_traits>
#include <zubax_chibios/util/heapless.hpp>

#if !CH_CFG_USE_REGISTRY
# pragma message "CH_CFG_USE_REGISTRY is disabled, panic reports will be incomplete"
#endif

namespace os
{

extern void emergencyPrint(const char* str);

__attribute__((weak))
void applicationHaltHook(void) { }

void sleepUntilChTime(systime_t sleep_until)
{
    chSysLock();
    sleep_until -= chVTGetSystemTimeX();
    if (static_cast<std::make_signed<systime_t>::type>(sleep_until) > 0)
    {
        chThdSleepS(sleep_until);
    }
    chSysUnlock();

#if defined(DEBUG_BUILD) && DEBUG_BUILD
    if (static_cast<std::make_signed<systime_t>::type>(sleep_until) < 0)
    {
#if CH_CFG_USE_REGISTRY
        const char* const name = chThdGetSelfX()->p_name;
#else
        const char* const name = "<?>";
#endif
        lowsyslog("%s: Lag %d ts\n", name,
                  static_cast<int>(static_cast<std::make_signed<systime_t>::type>(sleep_until)));
    }
#endif
}

static bool reboot_request_flag = false;

void requestReboot()
{
    reboot_request_flag = true;
}

bool isRebootRequested()
{
    return reboot_request_flag;
}

} // namespace os

extern "C"
{

__attribute__((weak))
void *__dso_handle;

__attribute__((weak))
int __errno;


void zchSysHaltHook(const char* msg)
{
    using namespace os;

    applicationHaltHook();

    /*
     * Printing the general panic message
     */
    port_disable();
    emergencyPrint("\r\nPANIC [");
#if CH_CFG_USE_REGISTRY
    const thread_t *pthread = chThdGetSelfX();
    if (pthread && pthread->p_name)
    {
        emergencyPrint(pthread->p_name);
    }
#endif
    emergencyPrint("] ");

    if (msg != NULL)
    {
        emergencyPrint(msg);
    }
    emergencyPrint("\r\n");

#if !defined(AGGRESSIVE_SIZE_OPTIMIZATION) || (AGGRESSIVE_SIZE_OPTIMIZATION == 0)
    static const auto print_register = [](const char* name, std::uint32_t value)
        {
            emergencyPrint(name);
            emergencyPrint("\t");
            emergencyPrint(os::heapless::intToString<16>(value));
            emergencyPrint("\r\n");
        };

    static const auto print_stack = [](const std::uint32_t* const ptr)
        {
            print_register("Pointer", reinterpret_cast<std::uint32_t>(ptr));
            print_register("R0",      ptr[0]);
            print_register("R1",      ptr[1]);
            print_register("R2",      ptr[2]);
            print_register("R3",      ptr[3]);
            print_register("R12",     ptr[4]);
            print_register("R14[LR]", ptr[5]);
            print_register("R15[PC]", ptr[6]);
            print_register("PSR",     ptr[7]);
        };

    /*
     * Printing registers
     */
    emergencyPrint("\r\nCore registers:\r\n");
#define PRINT_CORE_REGISTER(name)       print_register(#name, __get_##name())
    PRINT_CORE_REGISTER(CONTROL);
    PRINT_CORE_REGISTER(IPSR);
    PRINT_CORE_REGISTER(APSR);
    PRINT_CORE_REGISTER(xPSR);
    PRINT_CORE_REGISTER(PRIMASK);
#if __CORTEX_M >= 3
    PRINT_CORE_REGISTER(BASEPRI);
    PRINT_CORE_REGISTER(FAULTMASK);
#endif
#if __CORTEX_M >= 4
    PRINT_CORE_REGISTER(FPSCR);
#endif
#undef PRINT_CORE_REGISTER

    emergencyPrint("\r\nProcess stack:\r\n");
    print_stack(reinterpret_cast<std::uint32_t*>(__get_PSP()));

    emergencyPrint("\r\nMain stack:\r\n");
    print_stack(reinterpret_cast<std::uint32_t*>(__get_MSP()));

    emergencyPrint("\r\nSCB:\r\n");
#define PRINT_SCB_REGISTER(name)        print_register(#name, SCB->name)
    PRINT_SCB_REGISTER(AIRCR);
    PRINT_SCB_REGISTER(SCR);
    PRINT_SCB_REGISTER(CCR);
    PRINT_SCB_REGISTER(SHCSR);
    PRINT_SCB_REGISTER(CFSR);
    PRINT_SCB_REGISTER(HFSR);
    PRINT_SCB_REGISTER(DFSR);
    PRINT_SCB_REGISTER(MMFAR);
    PRINT_SCB_REGISTER(BFAR);
    PRINT_SCB_REGISTER(AFSR);
#undef PRINT_SCB_REGISTER
#endif      // AGGRESSIVE_SIZE_OPTIMIZATION

    /*
     * Emulating a breakpoint if we're in debug mode
     */
#if defined(DEBUG_BUILD) && DEBUG_BUILD && defined(CoreDebug_DHCSR_C_DEBUGEN_Msk)
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        __asm volatile ("bkpt #0\n"); // Break into the debugger
    }
#endif
}

void __assert_func(const char* file, int line, const char* func, const char* expr)
{
    port_disable();

    chSysHalt(os::heapless::concatenate(file, ":", line, " ", (func == nullptr) ? "" : func, ": ", expr).c_str());

    while (true) { }
}

/// From unistd
int usleep(useconds_t useconds)
{
    assert((((uint64_t)useconds * (uint64_t)CH_CFG_ST_FREQUENCY + 999999ULL) / 1000000ULL)
           < (1ULL << CH_CFG_ST_RESOLUTION));
    chThdSleepMicroseconds(useconds);
    return 0;
}

/// From unistd
unsigned sleep(unsigned int seconds)
{
    assert(((uint64_t)seconds * (uint64_t)CH_CFG_ST_FREQUENCY) < (1ULL << CH_CFG_ST_RESOLUTION));
    chThdSleepSeconds(seconds);
    return 0;
}

void* malloc(size_t sz)
{
#if CH_CFG_USE_MEMCORE
    return chCoreAlloc(sz);
#else
    (void) sz;
    chSysHalt("malloc");
    return nullptr;
#endif
}

void* calloc(size_t, size_t)
{
    chSysHalt("calloc");
    return nullptr;
}

void* realloc(void*, size_t)
{
    chSysHalt("realloc");
    return nullptr;
}

void free(void* p)
{
    /*
     * Certain stdlib functions, like mktime(), may call free() with zero argument, which can be safely ignored.
     */
    if (p != nullptr)
    {
        chSysHalt("free");
    }
}

}
