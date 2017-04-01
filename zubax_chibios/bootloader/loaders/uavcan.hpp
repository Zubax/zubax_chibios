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
#include <cstdlib>
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


using NodeName = os::heapless::String<80>;

struct HardwareInfo
{
    std::uint8_t major = 0;                                     ///< Required field
    std::uint8_t minor = 0;                                     ///< Required field

    typedef std::array<std::uint8_t, 16> UniqueID;
    UniqueID unique_id{};                                       ///< Required field

    typedef std::array<std::uint8_t, 255> CertificateOfAuthenticity;
    CertificateOfAuthenticity certificate_of_authenticity;      ///< Optional, set length to zero if not defined
    std::uint8_t certificate_of_authenticity_length = 0;
};

/**
 * Implementation details, please do not touch this.
 */
namespace impl_
{
namespace dsdl
{

static inline constexpr std::size_t bitlen2bytelen(std::size_t x)
{
    return (x + 7) / 8;
}

template <std::uint32_t DataTypeID_,
          std::uint64_t DataTypeSignature_,     // Not to be confused with DSDL signature
          std::size_t   MaxEncodedBitLength_>
struct MessageTypeInfo
{
    static constexpr std::uint32_t DataTypeID           = DataTypeID_;
    static constexpr std::uint64_t DataTypeSignature    = DataTypeSignature_;

    static constexpr std::size_t   MaxEncodedBitLength  = MaxEncodedBitLength_;
    static constexpr std::size_t   MaxSizeBytes         = bitlen2bytelen(MaxEncodedBitLength_);
};

template <std::uint32_t DataTypeID_,
          std::uint64_t DataTypeSignature_,     // Not to be confused with DSDL signature
          std::size_t   MaxEncodedBitLengthRequest_,
          std::size_t   MaxEncodedBitLengthResponse_>
struct ServiceTypeInfo
{
    static constexpr std::uint32_t DataTypeID                   = DataTypeID_;
    static constexpr std::uint64_t DataTypeSignature            = DataTypeSignature_;

    static constexpr std::size_t   MaxEncodedBitLengthRequest   = MaxEncodedBitLengthRequest_;
    static constexpr std::size_t   MaxSizeBytesRequest          = bitlen2bytelen(MaxEncodedBitLengthRequest_);

    static constexpr std::size_t   MaxEncodedBitLengthResponse  = MaxEncodedBitLengthResponse_;
    static constexpr std::size_t   MaxSizeBytesResponse         = bitlen2bytelen(MaxEncodedBitLengthResponse_);
};

// The values have been obtained with the help of the script show_data_type_info.py from libcanard.
using NodeStatus                = MessageTypeInfo<341,  0x0f0868d0c1a7c6f1,    56>;
using NodeIDAllocation          = MessageTypeInfo<1,    0x0b2a812620a11d40,   141>;

using GetNodeInfo               = ServiceTypeInfo<1,    0xee468a8121c46a9e,     0,  3015>;
using BeginFirmwareUpdate       = ServiceTypeInfo<40,   0xb7d725df72724126,  1616,  1031>;
using FileRead                  = ServiceTypeInfo<48,   0x8dcdca939f33f678,  1648,  2073>;
using RestartNode               = ServiceTypeInfo<5,    0x569e05394a3017f0,    40,     1>;


enum class NodeHealth : std::uint8_t
{
    Ok = 0,
    Error = 2
};

enum class NodeMode : std::uint8_t
{
    Maintenance = 2,
    SoftwareUpdate = 3
};

}

/**
 * This class provides an absolute time base that never overflows.
 * Note that the implementation requires two things:
 *      - The system tick interval must be an integer number of microseconds (checked at compile time)
 *      - The system time is queried at least once between system time overflows (not checked!)
 */
class MonotonicTimekeeper
{
    static_assert(unsigned(1000000 / unsigned(1000000 / CH_CFG_ST_FREQUENCY)) == CH_CFG_ST_FREQUENCY,
                  "The system tick interval must be an integer number of microseconds!");

    mutable ::systime_t prev_sample_at_st_ = 0;
    mutable std::uint64_t base_usec_ = 0;
    const std::uint64_t started_at_usec_;

public:
    MonotonicTimekeeper() :
        started_at_usec_(getMicroseconds())
    { }

    std::uint64_t getMicroseconds() const
    {
        // Computing increment since last invocation
        const ::systime_t ts = chVTGetSystemTimeX();
        const std::uint64_t increment_usec = ST2US(::systime_t(ts - prev_sample_at_st_)); // this shall be 64 bit wide!
        prev_sample_at_st_ = ts;
        // Add the increment to the absolute time estimate
        base_usec_ += increment_usec;
        return base_usec_;
    }

    std::uint64_t getUptimeMicroseconds() const
    {
        return getMicroseconds() - started_at_usec_;
    }
};

}       // namespace impl_

/**
 * A UAVCAN node that is useful solely for the purpose of firmware update.
 *
 * This class looks like a bowl of spaghetti because is has been carefully optimized for ROM footprint.
 * Aviod reading this code unless you've familiarized yourself with the UAVCAN specification.
 */
template <int StackSize = 4096, int MemoryPoolSize = 8192>
class UAVCANFirmwareUpdateNode : protected ::os::bootloader::IDownloader,
                                 protected chibios_rt::BaseStaticThread<StackSize>
{
    ::os::bootloader::Bootloader& bootloader_;
    ICANIface& iface_;

    const NodeName node_name_;
    const HardwareInfo hw_info_;

    impl_::MonotonicTimekeeper timekeeper_;
    std::uint64_t next_1hz_task_invocation_ = 0;

    alignas(std::max_align_t) std::array<std::uint8_t, MemoryPoolSize> memory_pool_{};
    CanardInstance canard_{};

    std::uint32_t can_bus_bit_rate_ = 0;
    std::uint8_t confirmed_local_node_id_ = 0;          ///< This field is needed in order to avoid mutexes

    std::uint8_t remote_server_node_id_ = 0;
    os::heapless::String<200> firmware_file_path_;

    os::Logger logger_{"Bootloader.UAVCAN"};

    std::uint64_t send_next_node_id_allocation_request_at_ = 0;
    std::uint8_t node_id_allocation_unique_id_offset_ = 0;

    impl_::dsdl::NodeHealth node_health_ = impl_::dsdl::NodeHealth::Ok;
    impl_::dsdl::NodeMode node_mode_     = impl_::dsdl::NodeMode::Maintenance;

    std::uint8_t node_status_transfer_id_ = 0;
    std::uint8_t node_id_allocation_transfer_id_ = 0;


    using chibios_rt::BaseStaticThread<StackSize>::start;       // This is overloaded below


    std::uint64_t getMonotonicTimestampUSec() const
    {
        return timekeeper_.getMicroseconds();
    }

    std::uint64_t getRandomDurationMicrosecond(std::uint64_t lower_bound_usec,
                                               std::uint64_t upper_bound_usec) const
    {
        const std::uint64_t rnd = std::uint64_t(std::rand()) * 128UL;
        return lower_bound_usec + rnd % (upper_bound_usec - lower_bound_usec);
    }

    void makeNodeStatusMessage(std::uint8_t* buffer) const
    {
        std::memset(buffer, 0, impl_::dsdl::NodeStatus::MaxSizeBytes);
        const std::uint32_t uptime_sec = (timekeeper_.getUptimeMicroseconds() + 500000UL) / 1000000UL;
        canardEncodeScalar(buffer,  0, 32, &uptime_sec);
        canardEncodeScalar(buffer, 32,  2, &node_health_);
        canardEncodeScalar(buffer, 34,  3, &node_mode_);
    }

    int initCAN(const std::uint32_t bitrate,
                const ICANIface::Mode mode,
                const ICANIface::AcceptanceFilterConfig& acceptance_filter = ICANIface::AcceptanceFilterConfig())
    {
        const int res = iface_.init(bitrate, mode, acceptance_filter);
        if (res < 0)
        {
            logger_.println("CAN init err @%u bps: %d", unsigned(bitrate), res);
        }
        return res;
    }

    std::pair<int, CanardCANFrame> receive(const int timeout_msec)
    {
        const auto res = iface_.receive(timeout_msec);
        if (res.first < 0)
        {
            logger_.println("RX err: %d", res.first);
        }
        return res;
    }

    int send(const CanardCANFrame& frame, const int timeout_msec)
    {
        const int res = iface_.send(frame, timeout_msec);
        if (res < 0)
        {
            logger_.println("TX err: %d", res);
        }
        return res;
    }

    void handle1HzTasks()
    {
        using namespace impl_;

        canardCleanupStaleTransfers(&canard_, getMonotonicTimestampUSec());

        // NodeStatus broadcasting
        if (canardGetLocalNodeID(&canard_) > 0)
        {
            std::uint8_t buffer[dsdl::NodeStatus::MaxSizeBytes]{};
            makeNodeStatusMessage(buffer);
            const int res = canardBroadcast(&canard_,
                                            dsdl::NodeStatus::DataTypeSignature,
                                            dsdl::NodeStatus::DataTypeID,
                                            &node_status_transfer_id_,
                                            CANARD_TRANSFER_PRIORITY_LOW,
                                            buffer,
                                            dsdl::NodeStatus::MaxSizeBytes);
            if (res <= 0)
            {
                logger_.println("NodeStatus bc err %d", res);
            }
        }
    }

    void poll()
    {
        constexpr int MaxFramesPerSpin = 10;

        // Receive
        for (int i = 0; i < MaxFramesPerSpin; i++)
        {
            const auto res = receive(1);        // Blocking call
            if (res.first < 1)
            {
                break;                          // Error or no frames
            }

            canardHandleRxFrame(&canard_, &res.second, getMonotonicTimestampUSec());
        }

        // Transmit
        for (int i = 0; i < MaxFramesPerSpin; i++)
        {
            const CanardCANFrame* txf = canardPeekTxQueue(&canard_);
            if (txf == nullptr)
            {
                break;                          // Nothing to transmit
            }

            const int res = send(*txf, 0);      // Non-blocking call
            if (res == 0)
            {
                break;                          // Queue is full
            }

            canardPopTxQueue(&canard_);         // Transmitted successfully or error, either way remove the frame
        }

        // 1Hz process
        if (getMonotonicTimestampUSec() >= next_1hz_task_invocation_)
        {
            next_1hz_task_invocation_ += 1000000UL;
            handle1HzTasks();
        }
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
            // Observe that we need both responses from allocators and requests from other nodes!
            ICANIface::AcceptanceFilterConfig filt;
            filt.id   = 0b00000000000000000000100000000 | CANARD_CAN_FRAME_EFF;
            filt.mask = 0b00000000000000000001110000000 | CANARD_CAN_FRAME_EFF | CANARD_CAN_FRAME_RTR |
                CANARD_CAN_FRAME_ERR;

            if (initCAN(can_bus_bit_rate_, ICANIface::Mode::AutomaticTxAbortOnError, filt) >= 0)
            {
                break;
            }

            ::sleep(1);
        }

        using namespace impl_;

        while ((!os::isRebootRequested()) && (canardGetLocalNodeID(&canard_) == 0))
        {
            send_next_node_id_allocation_request_at_ =
                getMonotonicTimestampUSec() + getRandomDurationMicrosecond(600000, 1000000);

            while ((getMonotonicTimestampUSec() < send_next_node_id_allocation_request_at_) &&
                   (canardGetLocalNodeID(&canard_) == 0))
            {
                poll();
            }

            if (canardGetLocalNodeID(&canard_) != 0)
            {
                break;
            }

            // Structure of the request is documented in the DSDL definition
            // See http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
            std::uint8_t allocation_request[7]{};

            if (node_id_allocation_unique_id_offset_ == 0)
            {
                allocation_request[0] |= 1;     // First part of unique ID
            }

            static constexpr std::uint8_t MaxLenOfUniqueIDInRequest = 6;
            std::uint8_t uid_size = std::uint8_t(hw_info_.unique_id.size() - node_id_allocation_unique_id_offset_);
            if (uid_size > MaxLenOfUniqueIDInRequest)
            {
                uid_size = MaxLenOfUniqueIDInRequest;
            }

            // Paranoia time
            assert(node_id_allocation_unique_id_offset_ < hw_info_.unique_id.size());
            assert(uid_size <= MaxLenOfUniqueIDInRequest);
            assert(uid_size > 0);
            assert((uid_size + node_id_allocation_unique_id_offset_) <= hw_info_.unique_id.size());

            std::memmove(&allocation_request[1], &hw_info_.unique_id[node_id_allocation_unique_id_offset_], uid_size);

            // Broadcasting the request
            const int bcast_res = canardBroadcast(&canard_,
                                                  dsdl::NodeIDAllocation::DataTypeSignature,
                                                  dsdl::NodeIDAllocation::DataTypeID,
                                                  &node_id_allocation_transfer_id_,
                                                  CANARD_TRANSFER_PRIORITY_LOW,
                                                  &allocation_request[0],
                                                  std::uint16_t(uid_size + 1U));
            if (bcast_res < 0)
            {
                logger_.println("NID alloc bc err %d", bcast_res);
            }

            // Preparing for timeout; if response is received, this value will be updated from the callback.
            node_id_allocation_unique_id_offset_ = 0;
        }
    }

    void main() override
    {
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

        using namespace impl_;

        /*
         * Update loop; run forever because there's nothing else to do
         */
        while (true)
        {
            // Accept only correctly addressed service requests and responses
            // We don't need message transfers anymore
            ICANIface::AcceptanceFilterConfig filt;
            filt.id   = 0b00000000000000000000010000000 | (confirmed_local_node_id_ << 8) | CANARD_CAN_FRAME_EFF;
            filt.mask = 0b00000000000000111111110000000 |
                CANARD_CAN_FRAME_EFF | CANARD_CAN_FRAME_RTR | CANARD_CAN_FRAME_ERR;

            if (initCAN(can_bus_bit_rate_, ICANIface::Mode::Normal, filt) >= 0)
            {
                break;
            }

            ::sleep(1);
        }

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
                    poll();
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
            node_mode_ = dsdl::NodeMode::SoftwareUpdate;
            node_health_ = dsdl::NodeHealth::Ok;

            const int result = bootloader_.upgradeApp(*this);

            logger_.println("FW downloaded, result %d", int(result));

            if (result >= 0)
            {
                node_health_ = dsdl::NodeHealth::Ok;
            }
            else
            {
                node_health_ = dsdl::NodeHealth::Error;
            }

            /*
             * Reset everything to zero and loop again, because there's nothing else to do.
             * The outer logic will request reboot if necessary.
             */
            node_mode_ = dsdl::NodeMode::Maintenance;
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
        using namespace impl_;

        DEBUG_LOG("RX TRANSFER %x %u\n", transfer->data_type_id, transfer->transfer_type);

        /*
         * Dynamic node ID allocation protocol.
         * Taking this branch only if we don't have a node ID, ignoring otherwise.
         * This piece of dark magic has been carefully transplanted from the libcanard demo application.
         */
        if ((canardGetLocalNodeID(&canard_) == CANARD_BROADCAST_NODE_ID) &&
            (transfer->transfer_type == CanardTransferTypeBroadcast) &&
            (transfer->data_type_id == dsdl::NodeIDAllocation::DataTypeID))
        {
            // Rule C - updating the randomized time interval
            send_next_node_id_allocation_request_at_ =
                getMonotonicTimestampUSec() + getRandomDurationMicrosecond(600000, 1000000);

            if (transfer->source_node_id == CANARD_BROADCAST_NODE_ID)
            {
                node_id_allocation_unique_id_offset_ = 0;
                logger_.puts("Foreign allocation response");
                return;
            }

            // Copying the unique ID from the message
            static constexpr unsigned UniqueIDBitOffset = 8;
            std::uint8_t received_unique_id[hw_info_.unique_id.size()];
            std::uint8_t received_unique_id_len = 0;
            for (;
                 received_unique_id_len < (transfer->payload_len - (UniqueIDBitOffset / 8U));
                 received_unique_id_len++)
            {
                assert(received_unique_id_len < hw_info_.unique_id.size());
                const std::uint8_t bit_offset = std::uint8_t(UniqueIDBitOffset + received_unique_id_len * 8U);
                (void) canardDecodeScalar(transfer, bit_offset, 8, false, &received_unique_id[received_unique_id_len]);
            }

            // Matching the received UID against the local one
            if (std::memcmp(received_unique_id, hw_info_.unique_id.data(), received_unique_id_len) != 0)
            {
                logger_.puts("Mismatching allocation response");
                node_id_allocation_unique_id_offset_ = 0;
                return;         // No match, return
            }

            if (received_unique_id_len < hw_info_.unique_id.size())
            {
                // The allocator has confirmed part of unique ID, switching to the next stage and updating the timeout.
                node_id_allocation_unique_id_offset_ = received_unique_id_len;
                send_next_node_id_allocation_request_at_ =
                    getMonotonicTimestampUSec() + getRandomDurationMicrosecond(0, 400000);

                logger_.println("Matching allocation response from %d offset %d",
                       transfer->source_node_id, node_id_allocation_unique_id_offset_);
            }
            else
            {
                // Allocation complete - copying the allocated node ID from the message
                std::uint8_t allocated_node_id = 0;
                (void) canardDecodeScalar(transfer, 0, 7, false, &allocated_node_id);
                assert(allocated_node_id <= 127);

                canardSetLocalNodeID(&canard_, allocated_node_id);

                logger_.println("Node ID %d allocated by %d", allocated_node_id, transfer->source_node_id);
            }
        }

        /*
         * GetNodeInfo request.
         * Someday this mess should be replaced with auto-generated message serialization code, like in libuavcan.
         */
        if ((transfer->transfer_type == CanardTransferTypeRequest) &&
            (transfer->data_type_id == dsdl::GetNodeInfo::DataTypeID))
        {
            std::uint8_t buffer[dsdl::GetNodeInfo::MaxSizeBytesResponse]{};

            // NodeStatus
            makeNodeStatusMessage(buffer);

            // SoftwareVersion (query the bootloader)
            const auto sw_success = bootloader_.getAppInfo();
            if (sw_success.second)
            {
                const AppInfo sw = sw_success.first;
                buffer[7] = sw.major_version;
                buffer[8] = sw.minor_version;
                buffer[9] = 3;                                              // Optional field flags
                canardEncodeScalar(buffer,  80, 32, &sw.vcs_commit);
                canardEncodeScalar(buffer, 112, 64, &sw.image_crc);
            }

            // HardwareVersion
            buffer[22] = hw_info_.major;
            buffer[23] = hw_info_.minor;
            std::memmove(&buffer[24], hw_info_.unique_id.data(), hw_info_.unique_id.size());
            buffer[40] = hw_info_.certificate_of_authenticity_length;
            std::memmove(&buffer[41],
                         hw_info_.certificate_of_authenticity.data(),
                         hw_info_.certificate_of_authenticity_length);

            // Name
            std::memcpy(&buffer[41 + hw_info_.certificate_of_authenticity_length],
                        node_name_.c_str(),
                        node_name_.length());

            const std::size_t total_size = 41 + hw_info_.certificate_of_authenticity_length + node_name_.length();
            assert(total_size <= dsdl::GetNodeInfo::MaxSizeBytesResponse);

            const int resp_res = canardRequestOrRespond(&canard_,
                                                        transfer->source_node_id,
                                                        dsdl::GetNodeInfo::DataTypeSignature,
                                                        dsdl::GetNodeInfo::DataTypeID,
                                                        &transfer->transfer_id,
                                                        transfer->priority,
                                                        CanardResponse,
                                                        &buffer[0],
                                                        std::uint16_t(total_size));
            if (resp_res <= 0)
            {
                logger_.println("GetNodeInfo resp err %d", resp_res);
            }
        }

        /*
         * RestartNode request.
         */
        if ((transfer->transfer_type == CanardTransferTypeRequest) &&
            (transfer->data_type_id == dsdl::RestartNode::DataTypeID))
        {
            std::uint64_t magic_number = 0;
            (void) canardDecodeScalar(transfer, 0, 40, false, &magic_number);
            if (magic_number == 0xACCE551B1E)
            {
                std::uint8_t response = 1 << 7;                 // 1 - ok, 0 - rejected
                (void) canardRequestOrRespond(&canard_,
                                              transfer->source_node_id,
                                              dsdl::RestartNode::DataTypeSignature,
                                              dsdl::RestartNode::DataTypeID,
                                              &transfer->transfer_id,
                                              transfer->priority,
                                              CanardResponse,
                                              &response,
                                              1);
                // TODO: Delegate the decision to the application via callback
                os::requestReboot();
            }
        }
    }

    bool shouldAcceptTransfer(std::uint64_t* out_data_type_signature,
                              std::uint16_t data_type_id,
                              CanardTransferType transfer_type,
                              std::uint8_t source_node_id)
    {
        using namespace impl_::dsdl;

        (void)source_node_id;

        if (canardGetLocalNodeID(&canard_) == CANARD_BROADCAST_NODE_ID)
        {
            // Dynamic node ID allocation broadcast
            if ((transfer_type == CanardTransferTypeBroadcast) &&
                (data_type_id == NodeIDAllocation::DataTypeID))
            {
                *out_data_type_signature = NodeIDAllocation::DataTypeSignature;
                return true;
            }
        }
        else
        {
            // GetNodeInfo REQUEST
            if ((transfer_type == CanardTransferTypeRequest) &&
                (data_type_id == GetNodeInfo::DataTypeID))
            {
                *out_data_type_signature = GetNodeInfo::DataTypeSignature;
                return true;
            }

            // BeginFirmwareUpdate REQUEST
            if ((transfer_type == CanardTransferTypeRequest) &&
                (data_type_id == BeginFirmwareUpdate::DataTypeID))
            {
                *out_data_type_signature = BeginFirmwareUpdate::DataTypeSignature;
                return true;
            }

            // FileRead RESPONSE (we don't serve requests of this type)
            if ((transfer_type == CanardTransferTypeResponse) &&
                (data_type_id == FileRead::DataTypeID))
            {
                *out_data_type_signature = FileRead::DataTypeSignature;
                return true;
            }

            // RestartNode REQUEST
            if ((transfer_type == CanardTransferTypeRequest) &&
                (data_type_id == RestartNode::DataTypeID))
            {
                *out_data_type_signature = RestartNode::DataTypeSignature;
                return true;
            }
        }

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
    /**
     * @param bl                        mutable reference to the bootloader instance
     * @param iface                     CAN driver adaptor instance
     * @param name                      product ID, UAVCAN node name
     * @param hw
     */
    UAVCANFirmwareUpdateNode(::os::bootloader::Bootloader& bl,
                             ICANIface& iface,
                             const NodeName& name,
                             const HardwareInfo& hw) :
        bootloader_(bl),
        iface_(iface),
        node_name_(name),
        hw_info_(hw)
    {
        next_1hz_task_invocation_ = getMonotonicTimestampUSec();
    }

    /**
     * This function can be invoked only once.
     *
     * @param thread_priority           priority of the UAVCAN thread
     * @param can_bus_bit_rate          set if known; defaults to zero, which initiates CAN bit rate autodetect
     * @param node_id                   set if known; defaults to zero, which initiates dynamic node ID allocation
     * @param remote_server_node_id     set if known; defaults to zero, which makes the node wait for an update request
     * @param remote_file_path          set if known; defaults to an empty string, which can be a valid path too
     */
    chibios_rt::ThreadReference start(const ::tprio_t thread_priority,
                                      const std::uint32_t can_bus_bit_rate = 0,
                                      const std::uint8_t node_id = 0,
                                      const std::uint8_t remote_server_node_id = 0,
                                      const char* const remote_file_path = "")
    {
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
