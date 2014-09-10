/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <hal.h>

#if !defined(GLUE)
#  define GLUE_(a, b) a##b
#  define GLUE(a, b)  GLUE_(a, b)
#endif

#define USARTX GLUE(USART, SERIAL_CLI_PORT_NUMBER)

void sysEmergencyPrint(const char* str)
{
    for (const char *p = str; *p; p++)
    {
        while (!(USARTX->SR & USART_SR_TXE)) { }
        USARTX->DR = *p;
    }
}

void sysDisableDebugPort(void)
{
    __disable_irq();
    uint32_t mapr = AFIO->MAPR;
    mapr &= ~AFIO_MAPR_SWJ_CFG; // these bits are write-only

    // Disable both SWD and JTAG:
    mapr |= AFIO_MAPR_SWJ_CFG_DISABLE;

    AFIO->MAPR = mapr;
    __enable_irq();
}
