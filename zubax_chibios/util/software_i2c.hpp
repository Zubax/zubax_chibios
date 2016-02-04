/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * Platform-independent I2C master implemented in software.
 * Sources:
 *      https://en.wikipedia.org/wiki/I%C2%B2C
 *      http://www.robot-electronics.co.uk/i2c-tutorial
 */

#pragma once

#include <zubax_chibios/os.hpp>
#include <unistd.h>
#include <cstdint>
#include <array>

namespace os
{
namespace software_i2c
{
/**
 * Generic bit-banging I2C master on bare GPIO.
 * The pins must be configured in open-drain mode, at high level by default.
 * This class automatically emits I2C stop condition and releases the pins when the instance is destroyed.
 * Usage:
 *     Master master(GPIO_PORT_I2C_SCL, GPIO_PIN_I2C_SCL,
 *                   GPIO_PORT_I2C_SDA, GPIO_PIN_I2C_SDA);
 *     const std::array<std::uint8_t, 3> tx = { 1, 2, 3 };
 *     std::array<std::uint8_t, 5> rx;
 *     auto result = master.exchange(address, tx, rx);
 */
class Master
{
public:
    enum class Result
    {
        OK,
        Timeout,
        ArbitrationLost,
        NACK
    };

private:
    static constexpr unsigned DefaultClockStretchTimeoutUSec = 10000;
    static constexpr unsigned DefaultCycleDelayUSec = 10;

    class I2CPin
    {
        GPIO_TypeDef* const port_;
        const unsigned pin_;

    public:
        I2CPin(GPIO_TypeDef* gpio_port, unsigned gpio_pin) :
            port_(gpio_port), pin_(gpio_pin)
        {
        }

        ~I2CPin()
        {
            set();       // Returning to default state
        }

        void set()       { palSetPad(port_, pin_); }
        void clear()     { palClearPad(port_, pin_); }
        bool get() const { return palReadPad(port_, pin_); }
    };

    I2CPin scl_;
    I2CPin sda_;
    bool started_ = false;
    const unsigned clock_stretch_timeout_usec_;
    const unsigned delay_usec_;

    void delay() const
    {
        ::usleep(delay_usec_);
    }

    bool sclWait() const
    {
        const ::systime_t started_at = chVTGetSystemTimeX();
        while (!scl_.get())
        {
            chThdSleep(1);          // Sleeping one sys tick
            if (chVTTimeElapsedSinceX(started_at) > US2ST(clock_stretch_timeout_usec_))
            {
                return false;
            }
        }
        return true;
    }

    Result writeBit(bool bit)
    {
        if (bit)
        {
            sda_.set();
        }
        else
        {
            sda_.clear();
        }
        delay();
        scl_.set();
        delay();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        if (bit && !sda_.get())
        {
            return Result::ArbitrationLost;
        }
        scl_.clear();
        return Result::OK;
    }

    Result readBit(bool& out_bit)
    {
        sda_.set();
        delay();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay();
        out_bit = sda_.get();
        scl_.clear();
        return Result::OK;
    }

public:
    Master(GPIO_TypeDef* scl_port, unsigned scl_pin,
           GPIO_TypeDef* sda_port, unsigned sda_pin,
           unsigned arg_clock_stretch_timeout_usec = DefaultClockStretchTimeoutUSec,
           unsigned arg_delay_usec = DefaultCycleDelayUSec) :
        scl_(scl_port, scl_pin),
        sda_(sda_port, sda_pin),
        clock_stretch_timeout_usec_(arg_clock_stretch_timeout_usec),
        delay_usec_(arg_delay_usec)
    { }

    /**
     * Destructor ensures that the bus is correctly stopped, and GPIO pins are correctly set to the high level.
     */
    ~Master()
    {
        if (started_)
        {
            (void)stop();
        }
    }

    /**
     * Generates I2C start on the bus.
     * If the bus has already been started, generates a repeated start sequence.
     */
    Result start()
    {
        sda_.set();
        delay();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay();
        if (!sda_.get())
        {
            return Result::ArbitrationLost;
        }
        sda_.clear();
        delay();
        scl_.clear();
        delay();
        started_ = true;
        return Result::OK;
    }

    bool isStarted() const { return started_; }

    /**
     * Stops the bus.
     * If the bus has not been started, the function will generate an assertion failure.
     */
    Result stop()
    {
        assert(started_);
        sda_.clear();
        delay();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay();
        sda_.set();
        delay();
        if (!sda_.get())
        {
            return Result::ArbitrationLost;
        }
        delay();
        started_ = false;
        return Result::OK;
    }

    Result writeByte(std::uint8_t byte)
    {
        assert(started_);
        for (std::uint8_t bit = 0; bit < 8; bit++)
        {
            auto res = writeBit((byte & 0x80) != 0);
            if (res != Result::OK)
            {
                return res;
            }
            byte <<= 1;
        }

        bool nack = false;
        auto res = readBit(nack);
        if (res != Result::OK)
        {
            return res;
        }

        return nack ? Result::NACK : Result::OK;
    }

    Result writeAddress7Bit(std::uint8_t address, bool read)
    {
        assert(address < 128U);
        address <<= 1U;
        address |= std::uint8_t(read);
        return writeByte(address);
    }

    Result readByte(std::uint8_t& out_byte, bool ack)
    {
        assert(started_);
        out_byte = 0;
        for (std::uint8_t i = 0; i < 8; i++)
        {
            bool bit = false;
            auto res = readBit(bit);
            if (res != Result::OK)
            {
                return res;
            }
            out_byte = (out_byte << 1) | (bit ? 1U : 0U);
        }

        return writeBit(!ack);
    }

    /**
     * This function first writes bytes to the bus, if any, and then reads data back, if requested.
     * It will automatically generate start and stop sequences.
     */
    Result exchange(std::uint8_t address,
                    const void* tx_data, const std::uint16_t tx_size,
                          void* rx_data, const std::uint16_t rx_size)
    {
        // This will ensure that the bus is correctly stopped at exit
        struct RAIIStopper
        {
            Master& owner_;
            RAIIStopper(Master& master) : owner_(master) { }
            ~RAIIStopper()
            {
                if (owner_.isStarted())
                {
                    (void)owner_.stop();
                }
            }
        } const volatile stopper(*this);

        // Writing
        if (tx_size > 0)
        {
            auto res = start();
            if (res != Result::OK)
            {
                return res;
            }

            res = writeAddress7Bit(address, false);
            if (res != Result::OK)
            {
                return res;
            }

            const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(tx_data);

            for (unsigned i = 0; i < tx_size; i++)
            {
                res = writeByte(*p++);
                if (res != Result::OK)
                {
                    return res;
                }
            }
        }

        // Reading
        if (rx_size > 0)
        {
            auto res = start();
            if (res != Result::OK)
            {
                return res;
            }

            res = writeAddress7Bit(address, true);
            if (res != Result::OK)
            {
                return res;
            }

            std::uint8_t* p = reinterpret_cast<std::uint8_t*>(rx_data);
            std::uint16_t left = rx_size;

            while (left --> 0)
            {
                res = readByte(*p++, left > 0);
                if (res != Result::OK)
                {
                    return res;
                }
            }
        }

        return Result::OK;
    }

    /**
     * Safer wrapper over @ref exchange(). Usage:
     *     const std::array<std::uint8_t, 3> tx = { 1, 2, 3 };
     *     std::array<std::uint8_t, 5> rx;
     *     auto result = master.exchange(address, tx, rx);
     */
    template<unsigned TxSize, unsigned RxSize>
    Result exchange(std::uint8_t address,
                    const std::array<uint8_t, TxSize>& tx,
                          std::array<uint8_t, RxSize>& rx)
    {
        return exchange(address, tx.data(), TxSize, rx.data(), RxSize);
    }
};

}
}
