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
#include <functional>
#include <cstdint>
#include <chrono>
#include <array>

namespace os
{
namespace software_i2c
{
/**
 * Generic bit-banging I2C master on bare GPIO.
 * The pins must be configured in open-drain mode, at high level by default.
 * This class automatically emits I2C stop condition and releases the pins when the instance is destroyed.
 *
 * All access must be done from regular threads outside of critical sections.
 *
 * All access is protected with a mutex, so that I2C transactions are atomic, and the class can be used from
 * multiple threads concurrently. If recursive mutexes are enabled, it is also possible to lock the bus
 * for multiple atomic transactions.
 *
 * Usage:
 *     Master master(GPIO_PORT_I2C_SCL, GPIO_PIN_I2C_SCL,
 *                   GPIO_PORT_I2C_SDA, GPIO_PIN_I2C_SDA);
 *     const std::array<std::uint8_t, 3> tx = { 1, 2, 3 };
 *     std::array<std::uint8_t, 5> rx;
 *     auto result = master.exchange(address, tx, rx);
 */
class Master final
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
    class I2CPin
    {
        ::ioportid_t const port_;
        const std::uint8_t pin_;

    public:
        I2CPin(::ioportid_t gpio_port, std::uint8_t gpio_pin) :
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

    chibios_rt::Mutex mutex_;
    I2CPin scl_;
    I2CPin sda_;
    bool started_ = false;
    const std::function<void ()> delay_;
    const std::uint32_t clock_stretch_timeout_ticks_;

    bool sclWait() const
    {
        const ::systime_t started_at = chVTGetSystemTimeX();
        while (!scl_.get())
        {
            chThdSleep(1);          // Sleeping one sys tick
            // Note that we should use greater comparison rather than greater-or-equal to prevent off-by-one errors
            if (chVTTimeElapsedSinceX(started_at) > clock_stretch_timeout_ticks_)
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
        delay_();
        scl_.set();
        delay_();
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
        delay_();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay_();
        out_bit = sda_.get();
        scl_.clear();
        return Result::OK;
    }

    /**
     * Generates I2C start on the bus.
     * If the bus has already been started, generates a repeated start sequence.
     */
    Result start()
    {
        sda_.set();
        delay_();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay_();
        if (!sda_.get())
        {
            return Result::ArbitrationLost;
        }
        sda_.clear();
        delay_();
        scl_.clear();
        delay_();
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
        delay_();
        scl_.set();
        if (!sclWait())
        {
            return Result::Timeout;
        }
        delay_();
        sda_.set();
        delay_();
        if (!sda_.get())
        {
            return Result::ArbitrationLost;
        }
        delay_();
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
            byte = std::uint8_t(byte << 1U);
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
        address = std::uint8_t((address << 1U) | read);
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
            out_byte = std::uint8_t((out_byte << 1) | (bit ? 1U : 0U));
        }

        return writeBit(!ack);
    }

    // Master is non-copyable! Otherwise that breaks the mutex.
    Master(const Master&)  = delete;
    Master(const Master&&) = delete;
    Master& operator=(const Master&)  = delete;
    Master& operator=(const Master&&) = delete;

public:
    template <typename Rep, typename Period>
    Master(::ioportid_t scl_port, std::uint8_t scl_pin,
           ::ioportid_t sda_port, std::uint8_t sda_pin,
           const std::function<void ()>& arg_cycle_delay,
           const std::chrono::duration<Rep, Period> arg_clock_stretch_timeout) :
        scl_(scl_port, scl_pin),
        sda_(sda_port, sda_pin),
        delay_(arg_cycle_delay),
        clock_stretch_timeout_ticks_(US2ST(
            std::chrono::duration_cast<std::chrono::microseconds>(arg_clock_stretch_timeout).count()))
    {
        assert(delay_);
        assert(clock_stretch_timeout_ticks_ > 0);
    }

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
     * Modulates a bus reset sequence.
     * The reset sequence allows the master to bring the bus into a known state.
     * Its use is advised by some EEPROM memory chip vendors, for example, like ROHM BR24G128.
     */
    void reset()
    {
        static constexpr std::uint8_t MaxClockCycles = 30;
        static constexpr std::uint8_t AllowStopIfSDAHighAfterThisManyClockCycles = 14;

        ::os::MutexLocker bus_locker(mutex_);

        for (std::uint8_t i = 0; i < MaxClockCycles; i++)
        {
            bool the_bit = false;
            (void) readBit(the_bit);
            if ((i > AllowStopIfSDAHighAfterThisManyClockCycles) && the_bit)
            {
                break;
            }
        }

        delay_();

        started_ = true;
        (void) stop();
    }

    /**
     * This function first writes bytes to the bus, if any, and then reads data back, if requested.
     * It will automatically generate start and stop sequences.
     */
    Result exchange(std::uint8_t address,
                    const void* tx_data, const std::uint16_t tx_size,
                          void* rx_data, const std::uint16_t rx_size)
    {
        ::os::MutexLocker bus_locker(mutex_);

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

            for (std::uint16_t i = 0; i < tx_size; i++)
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
    template<std::size_t TxSize, std::size_t RxSize>
    Result exchange(std::uint8_t address,
                    const std::array<std::uint8_t, TxSize>& tx,
                          std::array<std::uint8_t, RxSize>& rx)
    {
        return exchange(address, tx.data(), TxSize, rx.data(), RxSize);
    }

#if CH_CFG_USE_MUTEXES_RECURSIVE
    /**
     * This RAII helper allows the user to lock the bus for multiple subsequent atomic transactions.
     * This class is only available if recursive mutexes are enabled.
     */
    class AtomicBusAccessLocker
    {
        ::os::MutexLocker mutex_locker_;
    public:
        explicit AtomicBusAccessLocker(Master& m) : mutex_locker_(m.mutex_) { }
    };
#endif
};

}
}
