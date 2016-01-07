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

#include <hal.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#ifdef STM32F10X_XL
#  error "Add support for XL devices"
#endif

/*
 * The code below assumes that HSI oscillator is up and running,
 * otherwise the Flash controller (FPEC) may misbehave.
 * Any FPEC issues will be detected at run time during write/erase verification.
 */

#define RDP_KEY                 0x00A5
#define FLASH_KEY1              0x45670123
#define FLASH_KEY2              0xCDEF89AB

#if defined(STM32F10X_HD) || defined(STM32F10X_HD_VL) || defined(STM32F10X_CL) || defined(STM32F10X_XL)
# define FLASH_SIZE            (*((uint16_t*)0x1FFFF7E0))
# define FLASH_PAGE_SIZE        0x800
#elif defined(STM32F042x6) || defined(STM32F072xB)
# define FLASH_SIZE            (*((uint16_t*)0x1FFFF7CC))
# define FLASH_PAGE_SIZE        0x400
#else
# error Unknown device.
#endif

#define FLASH_END               ((FLASH_SIZE * 1024) + FLASH_BASE)
#define FLASH_PAGE_ADR          (FLASH_END - FLASH_PAGE_SIZE)
#define DATA_SIZE_MAX           FLASH_PAGE_SIZE


static void waitReady(void)
{
    do
    {
        assert(!(FLASH->SR & FLASH_SR_PGERR));
        assert(!(FLASH->SR & FLASH_SR_WRPRTERR));
    }
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->SR |= FLASH_SR_EOP;
}

static void prologue(void)
{
    chSysLock();
    waitReady();
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    FLASH->SR |= FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR; // Reset flags
    FLASH->CR = 0;
}

static void epilogue(void)
{
    FLASH->CR = FLASH_CR_LOCK;  // Reset the FPEC configuration and lock
    chSysUnlock();
}

int configStorageRead(unsigned offset, void* data, unsigned len)
{
    if (!data || (offset + len) > DATA_SIZE_MAX)
    {
        assert(0);
        return -EINVAL;
    }

    /*
     * Read directly, FPEC is not involved
     */
    std::memcpy(data, (void*)(FLASH_PAGE_ADR + offset), len);
    return 0;
}

int configStorageWrite(unsigned offset, const void* data, unsigned len)
{
    if (!data || (offset + len) > DATA_SIZE_MAX)
    {
        assert(0);
        return -EINVAL;
    }

    /*
     * Data alignment
     */
    if (((size_t)data) % 2)
    {
        assert(0);
        return -EIO;
    }
    unsigned num_data_halfwords = len / 2;
    if (num_data_halfwords * 2 < len)
    {
        num_data_halfwords += 1;
    }

    /*
     * Write
     */
    prologue();

    FLASH->CR = FLASH_CR_PG;

    volatile uint16_t* flashptr16 = (uint16_t*)(FLASH_PAGE_ADR + offset);
    const uint16_t* ramptr16 = (const uint16_t*)data;
    for (unsigned i = 0; i < num_data_halfwords; i++)
    {
        *flashptr16++ = *ramptr16++;
        waitReady();
    }

    waitReady();
    FLASH->CR = 0;

    epilogue();

    /*
     * Verify
     */
    const int cmpres = memcmp(data, (void*)(FLASH_PAGE_ADR + offset), len);
    return cmpres ? -EIO : 0;
}

int configStorageErase(void)
{
    /*
     * Erase
     */
    prologue();

    FLASH->CR = FLASH_CR_PER;
    FLASH->AR = FLASH_PAGE_ADR;
    FLASH->CR |= FLASH_CR_STRT;

    waitReady();
    FLASH->CR = 0;

    epilogue();

    /*
     * Verify
     */
    for (int i = 0; i < DATA_SIZE_MAX; i++)
    {
        uint8_t* ptr = (uint8_t*)(FLASH_PAGE_ADR + i);
        if (*ptr != 0xFF)
        {
            return -EIO;
        }
    }
    return 0;
}
