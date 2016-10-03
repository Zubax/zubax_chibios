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

/*
 * The code below assumes that HSI oscillator is up and running,
 * otherwise the Flash controller (FPEC) may misbehave.
 * Any FPEC issues will be detected at run time during write/erase verification.
 */

#if !defined(RDP_KEY)
# define RDP_KEY                0x00A5
#endif

#if !defined(FLASH_KEY1)
# define FLASH_KEY1             0x45670123
# define FLASH_KEY2             0xCDEF89AB
#endif

#if !defined(FLASH_SR_WRPRTERR) // Compatibility
# define FLASH_SR_WRPRTERR      FLASH_SR_WRPERR
#endif

namespace os
{
namespace config
{
/**
 * These values should be defined elsewhere in the application.
 * @{
 */
extern const unsigned StorageAddress;
extern const unsigned StorageSize;
extern const unsigned StorageSectorNumber;
/**
 * @}
 */
}
}

using namespace os::config;


static void checkInvariants(void)
{
    assert(StorageAddress % 256 == 0);
    assert(StorageSize    % 256 == 0);
    assert(StorageAddress      > 0);
    assert(StorageSize         > 0);
    assert(StorageSectorNumber > 0);
}

static void waitReady(void)
{
    do
    {
        assert(!(FLASH->SR & FLASH_SR_WRPRTERR));
        assert(!(FLASH->SR & FLASH_SR_PGAERR));
        assert(!(FLASH->SR & FLASH_SR_PGPERR));
        assert(!(FLASH->SR & FLASH_SR_PGSERR));
    }
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->SR |= FLASH_SR_EOP;
}

static void prologue(void)
{
    checkInvariants();
    chSysLock();
    waitReady();
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    FLASH->SR |= FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;
    FLASH->CR = 0;
}

static void epilogue(void)
{
    FLASH->CR = FLASH_CR_LOCK;  // Reset the FPEC configuration and lock
    chSysUnlock();
}

int configStorageRead(unsigned offset, void* data, unsigned len)
{
    checkInvariants();

    if (!data || (offset + len) > StorageSize)
    {
        assert(0);
        return -EINVAL;
    }

    /*
     * Read directly, FPEC is not involved
     */
    std::memcpy(data, (void*)(StorageAddress + offset), len);
    return 0;
}

int configStorageWrite(unsigned offset, const void* data, unsigned len)
{
    if (!data || (offset + len) > StorageSize)
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

#ifdef FLASH_CR_PSIZE_0
    FLASH->CR = FLASH_CR_PG | FLASH_CR_PSIZE_0;
#else
    FLASH->CR = FLASH_CR_PG;
#endif

    volatile uint16_t* flashptr16 = (uint16_t*)(StorageAddress + offset);
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
    const int cmpres = memcmp(data, (void*)(StorageAddress + offset), len);
    return cmpres ? -EIO : 0;
}

int configStorageErase(void)
{
    /*
     * Erase
     */
    prologue();

#ifdef FLASH_CR_PER
    FLASH->CR = FLASH_CR_PER;
    FLASH->AR = StorageAddress;
#else
    FLASH->CR = FLASH_CR_SER | ((StorageSectorNumber) << 3);
#endif
    FLASH->CR |= FLASH_CR_STRT;

    waitReady();
    FLASH->CR = 0;

    epilogue();

    /*
     * Verify
     */
    for (unsigned i = 0; i < StorageSize; i++)
    {
        uint8_t* ptr = (uint8_t*)(StorageAddress + i);
        if (*ptr != 0xFF)
        {
            return -EIO;
        }
    }
    return 0;
}
