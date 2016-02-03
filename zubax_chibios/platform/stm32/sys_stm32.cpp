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

namespace os
{

void emergencyPrint(const char* str)
{
    for (const char *p = str; *p; p++)
    {
#ifdef USART_ISR_TXE
        while (!(USARTX->ISR & USART_ISR_TXE)) { }
#else
        while (!(USARTX->SR & USART_SR_TXE)) { }
#endif
#ifdef USART_TDR_TDR
        USARTX->TDR = *p;
#else
        USARTX->DR = *p;
#endif
    }
}

}

extern "C"
{

void NMI_Handler(void)
{
    chSysHalt("NMI");
}

void HardFault_Handler(void)
{
    chSysHalt("HardFault");
}

void MemManage_Handler(void)
{
    chSysHalt("MemManage");
}

void BusFault_Handler(void)
{
    chSysHalt("BusFault");
}

void UsageFault_Handler(void)
{
    chSysHalt("UsageFault");
}

}
