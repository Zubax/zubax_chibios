/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <stdint.h>
#include <assert.h>
#include <hal.h>
#include <zubax_chibios/watchdog/watchdog.h>
#include <zubax_chibios/sys/assert_always.h>
#include <zubax_chibios/sys/sys.h>

#define KR_KEY_ACCESS   0x5555
#define KR_KEY_RELOAD   0xAAAA
#define KR_KEY_ENABLE   0xCCCC

#define MAX_RELOAD_VAL  0xFFF

#define MAX_NUM_WATCHDOGS 31


static int _wdg_timeout_ms = 0;

static uint32_t _mask __attribute__((section (".noinit")));
static int _num_watchdogs __attribute__((section (".noinit")));


static void setTimeout(int timeout_ms)
{
    if (timeout_ms <= 0) {
        assert(0);
        timeout_ms = 1;
    }

    int reload_value = timeout_ms / 6;  // For 1/256 prescaler
    if (reload_value > MAX_RELOAD_VAL)
        reload_value = MAX_RELOAD_VAL;

    IWDG->KR = KR_KEY_RELOAD;

    // Wait until the IWDG is ready to accept the new parameters
    while (IWDG->SR != 0) { }

    IWDG->KR = KR_KEY_ACCESS;
    IWDG->PR = 6;             // Div 256 yields 6.4ms per clock period at 40kHz
    IWDG->RLR = reload_value;
    IWDG->KR = KR_KEY_RELOAD;
    IWDG->KR = KR_KEY_ENABLE; // Starts if wasn't running yet
}

void watchdogInit(void)
{
    ASSERT_ALWAYS(_wdg_timeout_ms == 0);      // Make sure it wasn't initialized earlier
    ASSERT_ALWAYS(RCC->CSR & RCC_CSR_LSION);  // Make dure LSI is enabled
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) { }  // Wait for LSI startup

    if (RCC->CSR & RCC_CSR_IWDGRSTF) {
        lowsyslog("Watchdog: RESET WAS CAUSED BY WATCHDOG TIMEOUT\n");
        lowsyslog("Watchdog: RCC_CSR=0x%08x\n", (unsigned)RCC->CSR);
        lowsyslog("Watchdog: LAST STATE: mask=0x%08x, num=%d\n", (unsigned int)_mask, _num_watchdogs);
        chSysSuspend();
        RCC->CSR |= RCC_CSR_RMVF;
        chSysEnable();
    } else {
        lowsyslog("Watchdog: Reset was not caused by watchdog, it's OK\n");
    }
    _mask = 0;
    _num_watchdogs = 0;

    chSysSuspend();
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;
    chSysEnable();
}

int watchdogCreate(int timeout_ms)
{
    if (timeout_ms <= 0) {
        assert(0);
        return -1;
    }

    chSysSuspend();
    if (_num_watchdogs >= MAX_NUM_WATCHDOGS) {
        chSysEnable();
        assert(0);
        return -1;
    }
    const int new_id = _num_watchdogs++;
    _mask |= 1 << new_id;                 // Reset immediately
    chSysEnable();

    if (timeout_ms > _wdg_timeout_ms) {
        setTimeout(timeout_ms);
        _wdg_timeout_ms = timeout_ms;
        lowsyslog("Watchdog: Global timeout set to %i ms\n", _wdg_timeout_ms);
    }
    return new_id;
}

void watchdogReset(int id)
{
    assert(id >= 0 && id < _num_watchdogs);

    chSysSuspend();
    _mask |= 1 << id;
    const uint32_t valid_bits_mask = (1 << _num_watchdogs) - 1;
    if ((_mask & valid_bits_mask) == valid_bits_mask) {
        IWDG->KR = KR_KEY_RELOAD;
        _mask = 0;
    }
    chSysEnable();
}
