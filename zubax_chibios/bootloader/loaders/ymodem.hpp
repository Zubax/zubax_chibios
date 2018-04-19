/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include "../bootloader.hpp"
#include <zubax_chibios/os.hpp>
#include <zubax_chibios/watchdog/watchdog.hpp>
#include <cstdint>
#include <array>
#include <utility>


namespace os
{
namespace bootloader
{
namespace ymodem_loader
{
/**
 * Error codes specific to this module.
 */
static constexpr std::int16_t ErrOK                             = 0;
static constexpr std::int16_t ErrChannelWriteTimedOut           = 20001;
static constexpr std::int16_t ErrRetriesExhausted               = 20002;
static constexpr std::int16_t ErrProtocolError                  = 20003;
static constexpr std::int16_t ErrTransferCancelledByRemote      = 20004;
static constexpr std::int16_t ErrRemoteRefusedToProvideFile     = 20005;

/**
 * Downloads data using YMODEM or XMODEM protocol over the specified ChibiOS channel
 * (e.g. serial port, USB CDC ACM, TCP, ...).
 *
 * This class will request Checksum mode, in order to retain compatibility both with XMODEM and YMODEM senders
 * (YMODEM-compatible senders do support checksum mode as well as CRC mode).
 * Both 1K and 128-byte blocks are supported.
 * Overall, the following protocols are supported:
 *      - YMODEM
 *      - XMODEM
 *      - XMODEM-1K
 *
 * Reference: http://pauillac.inria.fr/~doligez/zmodem/ymodem.txt
 */
class YModemReceiver : public IDownloader
{
    static constexpr unsigned BlockSizeXModem = 128;
    static constexpr unsigned BlockSize1K     = 1024;
    static constexpr unsigned WorstCaseBlockSizeWithCRC = BlockSize1K + 2;

    static constexpr unsigned SendTimeoutMSec = 1000;

    static constexpr unsigned InitialTimeoutMSec      = 60000;
    static constexpr unsigned NextBlockTimeoutMSec    = 5000;
    static constexpr unsigned BlockPayloadTimeoutMSec = 1000;

    static constexpr unsigned MaxRetries = 3;

    ::BaseChannel* const channel_;
    watchdog::Timer* const watchdog_reference_;

    std::uint8_t buffer_[WorstCaseBlockSizeWithCRC];


    static int sendResultToErrorCode(int res);

    static std::uint8_t computeChecksum(const void* data, unsigned size);

    void kickTheDog();

    int send(std::uint8_t byte);

    int receive(void* data, unsigned size, unsigned timeout_msec);

    void flushReadQueue();

    void abort();

    enum class BlockReceptionResult
    {
        Success,
        Timeout,
        EndOfTransmission,
        TransmissionCancelled,
        ProtocolError,
        SystemError
    };

    /**
     * Reads a block from the channel. This function does not transmit anything.
     * @return First component: @ref BlockReceptionResult
     *         Second component: system error code, if applicable
     */
    std::pair<BlockReceptionResult, int> receiveBlock(unsigned& out_size,
                                                      std::uint8_t& out_sequence);

    static bool tryParseZeroBlock(const std::uint8_t* const data,
                                  const unsigned size,
                                  bool& out_is_null_block,
                                  std::uint32_t& out_file_size);

    static int processDownloadedBlock(IDownloadStreamSink& sink, void* data, unsigned size);

public:
    /**
     * @param channel                   the serial port channel that will be used for downloading
     * @param watchdog_reference        optional reference to a watchdog timer that will be reset periodically.
     *                                  The watchdog timeout must be greater than 1 second.
     */
    YModemReceiver(::BaseChannel* channel,
                   watchdog::Timer* watchdog_reference = nullptr) :
        channel_(channel),
        watchdog_reference_(watchdog_reference)
    { }

    int download(IDownloadStreamSink& sink) override;
};

}
}
}
