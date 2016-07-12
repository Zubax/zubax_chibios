/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * This file was originally written in pure C99, but later it had to be migrated to C++.
 * In its current state it's kinda written in C99 with C++ features.
 *
 *                      This is not proper C++.
 */

#include <cstdint>
#include <cassert>
#include <hal.h>
#include <zubax_chibios/os.hpp>

#if !defined(DISABLE_WATCHDOG_IN_RELEASE_BUILD)
# define DISABLE_WATCHDOG_IN_RELEASE_BUILD      0
#endif
#if !defined(DISABLE_WATCHDOG)
# define DISABLE_WATCHDOG                       DISABLE_WATCHDOG_IN_RELEASE_BUILD
#endif
#if DISABLE_WATCHDOG
# if (defined(RELEASE_BUILD) && RELEASE_BUILD) || !(defined(DEBUG_BUILD) && DEBUG_BUILD)
#  if !DISABLE_WATCHDOG_IN_RELEASE_BUILD
#   error "DISABLE_WATCHDOG is not permitted with release build"
#  endif
# endif
#endif

#define KR_KEY_ACCESS   0x5555
#define KR_KEY_RELOAD   0xAAAA
#define KR_KEY_ENABLE   0xCCCC

#define MAX_RELOAD_VAL  0xFFF

#define MAX_NUM_WATCHDOGS 31

#if !defined(RCC_CSR_IWDGRSTF)
# define RCC_CSR_IWDGRSTF   RCC_CSR_WDGRSTF
#endif


static unsigned _wdg_timeout_ms = 0;

static std::uint32_t _mask __attribute__((section (".noinit")));
static std::uint8_t _num_watchdogs __attribute__((section (".noinit")));

static bool _triggered_reset = false;

static void setTimeout(unsigned timeout_ms)
{
    if (timeout_ms <= 0)
    {
        assert(0);
        timeout_ms = 1;
    }

#if DISABLE_WATCHDOG
# pragma message "WARNING: Watchdog is disabled!"
#else
    unsigned reload_value = timeout_ms / 6;  // For 1/256 prescaler
    if (reload_value > MAX_RELOAD_VAL)
    {
        reload_value = MAX_RELOAD_VAL;
    }

    IWDG->KR = KR_KEY_RELOAD;

    // Wait until the IWDG is ready to accept the new parameters
    while (IWDG->SR != 0) { }

    IWDG->KR = KR_KEY_ACCESS;
    IWDG->PR = 6;             // Div 256 yields 6.4ms per clock period at 40kHz
    IWDG->RLR = reload_value;
    IWDG->KR = KR_KEY_RELOAD;
    IWDG->KR = KR_KEY_ENABLE; // Starts if wasn't running yet
#endif
}

void watchdogInit(void)
{
    ASSERT_ALWAYS(_wdg_timeout_ms == 0);      // Make sure it wasn't initialized earlier
    ASSERT_ALWAYS(RCC->CSR & RCC_CSR_LSION);  // Make dure LSI is enabled
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) { }  // Wait for LSI startup

    if (RCC->CSR & RCC_CSR_IWDGRSTF)
    {
        _triggered_reset = true;
#if !defined(WATCHDOG_RETAIN_RESET_CAUSE_CODE)
        chSysSuspend();
        RCC->CSR |= RCC_CSR_RMVF;
        chSysEnable();
#endif
    }

    _mask = 0;
    _num_watchdogs = 0;

#ifdef DBGMCU_CR_DBG_IWDG_STOP
    chSysSuspend();
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;
    chSysEnable();
#endif
}

bool watchdogTriggeredLastReset(void)
{
    return _triggered_reset;
}

int watchdogCreate(unsigned timeout_ms)
{
    if (timeout_ms <= 0)
    {
        assert(0);
        return -1;
    }

    chSysSuspend();
    if (_num_watchdogs >= MAX_NUM_WATCHDOGS)
    {
        chSysEnable();
        assert(0);
        return -1;
    }
    const int new_id = _num_watchdogs++;
    _mask |= 1U << new_id;                 // Reset immediately
    chSysEnable();

    if (timeout_ms > _wdg_timeout_ms)
    {
        setTimeout(timeout_ms);
        _wdg_timeout_ms = timeout_ms;
    }
    return new_id;
}

void watchdogReset(int id)
{
    assert(id >= 0 && id < _num_watchdogs);

    chSysSuspend();
    _mask |= 1U << id;
    const std::uint32_t valid_bits_mask = (1U << _num_watchdogs) - 1;
    if ((_mask & valid_bits_mask) == valid_bits_mask)
    {
        IWDG->KR = KR_KEY_RELOAD;
        _mask = 0;
    }
    chSysEnable();
}
