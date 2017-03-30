/*
 * Copyright (c) 2017 Zubax Robotics, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

#include "../bootloader.hpp"
#include <zubax_chibios/os.hpp>
#include <zubax_chibios/util/heapless.hpp>
#include <cstdint>
#include <array>
#include <utility>
#include <cstddef>
#include <canard.h>                     // This loader requires libcanard
#include <unistd.h>


namespace os
{
namespace bootloader
{
namespace uavcan_loader
{
/**
 * Error codes specific to this module.
 */
static constexpr std::int16_t ErrOK                             = 0;
static constexpr std::int16_t ErrDriverError                    = 30002;
static constexpr std::int16_t ErrProtocolError                  = 30003;
static constexpr std::int16_t ErrTransferCancelledByRemote      = 30004;
static constexpr std::int16_t ErrRemoteRefusedToProvideFile     = 30005;

/**
 * Generic CAN controller driver interface.
 */
class ICANIface
{
public:
    /**
     * Controller operating mode.
     * The driver must support silent mode, because it is required for automatic bit rate detection.
     * The automatic transmission abort on error feature is required for dynamic node ID allocation
     * (read the UAVCAN specification for more info).
     */
    enum class Mode
    {
        Normal,
        Silent,
        AutomaticTxAbortOnError
    };

    /**
     * Acceptance filter configuration.
     * Acceptance filters may be not supported in the underlying driver, this feature is optional.
     * Bit flags used here are the same as in libcanard.
     * The default constructor makes a filter that accepts all frames.
     */
    struct AcceptanceFilterConfig
    {
        std::uint32_t id = 0;
        std::uint32_t mask = 0;
    };

    virtual ~ICANIface() {}

    /**
     * Initializes the CAN hardware in the specified mode.
     * Observe that this implementation needs only one acceptance filter.
     * @retval 0                Success
     * @retval negative         Error
     */
    virtual int init(const std::uint32_t bitrate, const Mode mode, const AcceptanceFilterConfig& acceptance_filter);

    /**
     * Transmits one CAN frame.
     * Timeout value can be only positive.
     *
     * @retval      1               Transmitted successfully
     * @retval      0               Timed out
     * @retval      negative        Error
     */
    virtual int send(const CanardCANFrame& frame, const int timeout_millisec);

    /**
     * Reads one CAN frame from the RX queue.
     * Timeout value can be only positive.
     *
     * @retval      1               Read successfully
     * @retval      0               Timed out
     * @retval      negative        Error
     */
    virtual std::pair<int, CanardCANFrame> receive(const int timeout_millisec);
};

/**
 * A UAVCAN node that is useful solely for the purpose of firmware update.
 * Note that this class is purposefully optimized for code size.
 */
template <int StackSize = 4096, int MemoryPoolSize = 8192>
class UAVCANFirmwareUpdateNode : protected ::os::bootloader::IDownloader,
                                 protected chibios_rt::BaseStaticThread<StackSize>
{
    alignas(std::max_align_t) std::array<std::uint8_t, MemoryPoolSize> memory_pool_{};
    CanardInstance canard_{};

    std::uint32_t can_bus_bit_rate_ = 0;
    std::uint8_t confirmed_local_node_id_ = 0;          ///< This field is needed in order to avoid mutexes

    std::uint8_t remote_server_node_id_ = 0;
    os::heapless::String<200> firmware_file_path_;

    ::os::bootloader::Bootloader* bootloader_ = nullptr;
    ICANIface* iface_ = nullptr;

    os::Logger logger_{"Bootloader.UAVCAN"};


    using chibios_rt::BaseStaticThread<StackSize>::start;       // This is overloaded below


    int initCAN(const std::uint32_t bitrate,
                const ICANIface::Mode mode,
                const ICANIface::AcceptanceFilterConfig& acceptance_filter = ICANIface::AcceptanceFilterConfig())
    {
        ASSERT_ALWAYS(iface_ != nullptr);
        const int res = iface_->init(bitrate, mode, acceptance_filter);
        if (res < 0)
        {
            logger_.println("CAN init err @%u bps: %d", unsigned(bitrate), res);
        }
        return res;
    }

    std::pair<int, CanardCANFrame> receive(const int timeout_msec)
    {
        assert(iface_ != nullptr);
        const auto res = iface_->receive(timeout_msec);
        if (res.first < 0)
        {
            logger_.println("RX err: %d", res.first);
        }
        return res;
    }

    void performCANBitRateDetection()
    {
        /// These are defined by the specification; 100 Kbps is added due to its popularity.
        static constexpr std::array<std::uint32_t, 5> StandardBitRates
        {
            1000000,        ///< Recommended by UAVCAN
             500000,        ///< Standard
             250000,        ///< Standard
             125000,        ///< Standard
             100000         ///< Popular bit rate that is not defined by the specification
        };

        int current_bit_rate_index = 0;

        // Loop forever until the bit rate is detected
        while ((!os::isRebootRequested()) && (can_bus_bit_rate_ == 0))
        {
            const std::uint32_t br = StandardBitRates[current_bit_rate_index];
            current_bit_rate_index = (current_bit_rate_index + 1) % StandardBitRates.size();

            if (initCAN(br, ICANIface::Mode::Silent) >= 0)
            {
                const int res = receive(1100).first;
                if (res > 0)
                {
                    can_bus_bit_rate_ = br;
                }
                if (res < 0)
                {
                    ::sleep(1);
                }
            }
            else
            {
                ::sleep(1);
            }
        }
    }

    void performDynamicNodeIDAllocation()
    {
        // CAN bus initialization
        while (true)
        {
            // Accept only anonymous messages with DTID = 1 (Allocation)
            ICANIface::AcceptanceFilterConfig filt;
            filt.id   = 0x00000100 | CANARD_CAN_FRAME_EFF;
            filt.mask = 0x000003FF | CANARD_CAN_FRAME_EFF | CANARD_CAN_FRAME_RTR | CANARD_CAN_FRAME_ERR;

            if (initCAN(can_bus_bit_rate_, ICANIface::Mode::AutomaticTxAbortOnError, filt) >= 0)
            {
                break;
            }

            ::sleep(1);
        }

        while ((!os::isRebootRequested()) && (canardGetLocalNodeID(&canard_) == 0))
        {
            // TODO finish this
            ::usleep(100000);
            chibios_rt::System::halt("DONE");
        }
    }

    void main() override
    {
        ASSERT_ALWAYS(bootloader_ != nullptr);

        this->setName("btlduavcan");

        /*
         * CAN bit rate
         */
        if (can_bus_bit_rate_ == 0)
        {
            logger_.puts("CAN bit rate detection...");
            performCANBitRateDetection();
        }

        if (os::isRebootRequested())
        {
            return;
        }

        logger_.println("CAN bit rate: %u", unsigned(can_bus_bit_rate_));

        /*
         * Node ID
         */
        if (canardGetLocalNodeID(&canard_) == 0)
        {
            logger_.puts("Node ID allocation...");
            performDynamicNodeIDAllocation();
        }

        if (os::isRebootRequested())
        {
            return;
        }

        confirmed_local_node_id_ = canardGetLocalNodeID(&canard_);
        logger_.println("Node ID: %u", unsigned(confirmed_local_node_id_));

        /*
         * Update loop; run forever because there's nothing else to do
         */
        while (!os::isRebootRequested())
        {
            assert((confirmed_local_node_id_ > 0) && (canardGetLocalNodeID(&canard_) > 0));

            /*
             * Waiting for the firmware update request
             */
            if (remote_server_node_id_ == 0)
            {
                logger_.puts("Waiting for FW update request...");
                while ((!os::isRebootRequested()) && (remote_server_node_id_ == 0))
                {
                    chThdSleepMilliseconds(100);
                }
            }

            if (os::isRebootRequested())
            {
                break;
            }

            logger_.println("FW server NID %u, path: %s",
                            unsigned(remote_server_node_id_), firmware_file_path_.c_str());

            /*
             * Rewriting the old firmware with the new file
             */
            assert(bootloader_ != nullptr);
            const int result = bootloader_->upgradeApp(*this);

            logger_.println("FW downloaded, result %d", int(result));

            /*
             * Reset everything to zero and loop again, because there's nothing else to do.
             * The outer logic will request reboot if necessary.
             */
            remote_server_node_id_ = 0;
            firmware_file_path_.clear();
        }
    }

    int download(IDownloadStreamSink& sink) override
    {
        (void) sink;
        return -1;
    }

    void onTransferReception(CanardRxTransfer* const transfer)
    {
        (void) transfer;
    }

    bool shouldAcceptTransfer(std::uint64_t* out_data_type_signature,
                              std::uint16_t data_type_id,
                              CanardTransferType transfer_type,
                              std::uint8_t source_node_id)
    {
        (void) out_data_type_signature;
        (void) data_type_id;
        (void) transfer_type;
        (void) source_node_id;
        return false;
    }


    static void onTransferReceptionTrampoline(CanardInstance* ins,
                                              CanardRxTransfer* transfer)
    {
        assert((ins != nullptr) && (ins->user_reference != nullptr));
        UAVCANFirmwareUpdateNode* const self = reinterpret_cast<UAVCANFirmwareUpdateNode*>(ins->user_reference);
        self->onTransferReception(transfer);
    }

    static bool shouldAcceptTransferTrampoline(const CanardInstance* ins,
                                               std::uint64_t* out_data_type_signature,
                                               std::uint16_t data_type_id,
                                               CanardTransferType transfer_type,
                                               std::uint8_t source_node_id)
    {
        assert((ins != nullptr) && (ins->user_reference != nullptr));
        UAVCANFirmwareUpdateNode* const self = reinterpret_cast<UAVCANFirmwareUpdateNode*>(ins->user_reference);
        return self->shouldAcceptTransfer(out_data_type_signature,
                                          data_type_id,
                                          transfer_type,
                                          source_node_id);
    }

public:
    UAVCANFirmwareUpdateNode() { }

    /**
     * This function can be invoked only once.
     *
     * @param thread_priority           priority of the UAVCAN thread
     * @param bl                        mutable reference to the bootloader instance
     * @param iface                     CAN driver adaptor instance
     * @param can_bus_bit_rate          set if known; defaults to zero, which initiates CAN bit rate autodetect
     * @param node_id                   set if known; defaults to zero, which initiates dynamic node ID allocation
     * @param remote_server_node_id     set if known; defaults to zero, which makes the node wait for an update request
     * @param remote_file_path          set if known; defaults to an empty string, which can be a valid path too
     */
    chibios_rt::ThreadReference start(const ::tprio_t thread_priority,
                                      ::os::bootloader::Bootloader& bl,
                                      ICANIface& iface,
                                      const std::uint32_t can_bus_bit_rate = 0,
                                      const std::uint8_t node_id = 0,
                                      const std::uint8_t remote_server_node_id = 0,
                                      const char* const remote_file_path = "")
    {
        this->bootloader_ = &bl;
        this->iface_      = &iface;
        this->can_bus_bit_rate_ = can_bus_bit_rate;

        if ((remote_server_node_id >= CANARD_MIN_NODE_ID) &&
            (remote_server_node_id <= CANARD_MAX_NODE_ID))
        {
            this->remote_server_node_id_ = remote_server_node_id;
            this->firmware_file_path_ = remote_file_path;
        }

        canardInit(&canard_,
                   memory_pool_.data(),
                   memory_pool_.size(),
                   &UAVCANFirmwareUpdateNode::onTransferReceptionTrampoline,
                   &UAVCANFirmwareUpdateNode::shouldAcceptTransferTrampoline,
                   this);

        if ((node_id >= CANARD_MIN_NODE_ID) &&
            (node_id <= CANARD_MAX_NODE_ID))
        {
            canardSetLocalNodeID(&canard_, node_id);
        }

        return chibios_rt::BaseStaticThread<StackSize>::start(thread_priority);
    }

    /**
     * Returns the CAN bus bit rate, if known, otherwise zero.
     */
    std::uint32_t getCANBusBitRate() const
    {
        return can_bus_bit_rate_;               // No thread sync is needed, read is atomic
    }

    /**
     * Returns the local UAVCAN node ID, if set, otherwise zero.
     */
    std::uint8_t getLocalNodeID() const
    {
        return confirmed_local_node_id_;        // No thread sync is needed, read is atomic
    }
};

}
}
}
