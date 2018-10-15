/*
 * This demo application is distributed under the terms of CC0 (public domain dedication).
 * More info: https://creativecommons.org/publicdomain/zero/1.0/
 */

// This is needed to enable necessary declarations in sys/
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <canard.h>
#include <socketcan.h>      // CAN backend driver for SocketCAN, distributed with Libcanard

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Serialized DSDL definitions
#include "uavcan/protocol/NodeStatus.h"
#include "uavcan/protocol/GetNodeInfo.h"
#include "uavcan/protocol/SoftwareVersion.h"
#include "uavcan/protocol/HardwareVersion.h"

/*
 * Application constants
 */
#define APP_VERSION_MAJOR                                           1
#define APP_VERSION_MINOR                                           0
#define APP_NODE_NAME                                               "org.revolve.uavcan.example"

/*
 * Some useful constants defined by the UAVCAN specification.
 * Data type signature values can be easily obtained with the script show_data_type_info.py
 */

#define UNIQUE_ID_LENGTH_BYTES                                      16

/*
 * Library instance.
 * In simple applications it makes sense to make it static, but it is not necessary.
 */
static CanardInstance canard;                       ///< The library instance
static uint8_t canard_memory_pool[1024];            ///< Arena for memory allocation, used by the library

/*
 * Node status variables
 */
static uint8_t node_health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
static uint8_t node_mode   = UAVCAN_PROTOCOL_NODESTATUS_MODE_INITIALIZATION;


static uint64_t getMonotonicTimestampUSec(void)
{
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        abort();
    }
    return (uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}


/**
 * This function uses a mock unique ID, this is not allowed in real applications!
 */
static void readUniqueID(uint8_t* out_uid)
{
    for (uint8_t i = 0; i < UNIQUE_ID_LENGTH_BYTES; i++)
    {
        out_uid[i] = i;
    }
}

static uavcan_protocol_NodeStatus populateNodeStatus() 
{
    uavcan_protocol_NodeStatus node_status;

    static uint32_t started_at_sec = 0;
    if (started_at_sec == 0)
    {
        started_at_sec = (uint32_t)(getMonotonicTimestampUSec() / 1000000U);
    }

    const uint32_t uptime_sec = (uint32_t)((getMonotonicTimestampUSec() / 1000000U) - started_at_sec);

    node_status.uptime_sec = uptime_sec;
    node_status.health = node_health;
    node_status.mode = node_mode;
    node_status.vendor_specific_status_code = 0;

    return node_status;
}

static void makeNodeStatusMessage(uint8_t buffer[UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE], uint32_t* length)
{
    uavcan_protocol_NodeStatus node_status = populateNodeStatus();

    (*length) = uavcan_protocol_NodeStatus_encode(&node_status, buffer);
}

static uavcan_protocol_SoftwareVersion populateSoftwareVersion() 
{
    uavcan_protocol_SoftwareVersion sw_version;

    sw_version.major = APP_VERSION_MAJOR;
    sw_version.minor = APP_VERSION_MINOR;
    sw_version.vcs_commit = GIT_HASH;
    sw_version.optional_field_flags = UAVCAN_PROTOCOL_SOFTWAREVERSION_OPTIONAL_FIELD_FLAG_VCS_COMMIT;

    return sw_version;
}

static uavcan_protocol_HardwareVersion populateHardwareVersion() 
{
    uavcan_protocol_HardwareVersion hw_version;

    hw_version.certificate_of_authenticity = calloc(0, sizeof(uint8_t));
    hw_version.certificate_of_authenticity_len = 1;

    uint8_t ID[16];
    readUniqueID(ID);

    memcpy(hw_version.unique_id, ID, 16);

    return hw_version;
}

static uavcan_protocol_GetNodeInfoResponse populateNodeInfoResponse()
{
    uavcan_protocol_NodeStatus node_status = populateNodeStatus();
    uavcan_protocol_SoftwareVersion sw_version = populateSoftwareVersion();
    uavcan_protocol_HardwareVersion hw_version = populateHardwareVersion();

    uavcan_protocol_GetNodeInfoResponse response;
    response.status = node_status;
    response.software_version = sw_version;
    response.hardware_version = hw_version;
    response.name_len = strlen(APP_NODE_NAME);

    response.name = calloc(response.name_len, sizeof(char));
    strcpy((char*) response.name, APP_NODE_NAME);
    // memcpy(response.name, APP_NODE_NAME, response.name_len);

    return response;    
}

static void makeNodeInfoResponse(uint8_t buffer[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE], uint32_t* length)
{
    uavcan_protocol_GetNodeInfoResponse response = populateNodeInfoResponse();

    (*length) = uavcan_protocol_GetNodeInfoResponse_encode(&response, buffer);
}


/**
 * This callback is invoked by the library when a new message or request or response is received.
 */
static void onTransferReceived(CanardInstance* ins,
                               CanardRxTransfer* transfer)
{

    if ((transfer->transfer_type == CanardTransferTypeRequest) &&
        (transfer->data_type_id == UAVCAN_PROTOCOL_GETNODEINFO_ID))
    {
        printf("GetNodeInfo request from %d\n", transfer->source_node_id);

        uint8_t buffer[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE];
        uint32_t length;
        makeNodeInfoResponse(buffer, &length);

        /*
         * Transmitting; in this case we don't have to release the payload because it's empty anyway.
         */
        const int16_t resp_res = canardRequestOrRespond(ins,
                                                        transfer->source_node_id,
                                                        UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE,
                                                        UAVCAN_PROTOCOL_GETNODEINFO_ID,
                                                        &transfer->transfer_id,
                                                        transfer->priority,
                                                        CanardResponse,
                                                        &buffer[0],
                                                        (uint16_t) length);
        if (resp_res <= 0)
        {
            (void)fprintf(stderr, "Could not respond to GetNodeInfo; error %d\n", resp_res);
        }
    }
}


/**
 * This callback is invoked by the library when it detects beginning of a new transfer on the bus that can be received
 * by the local node.
 * If the callback returns true, the library will receive the transfer.
 * If the callback returns false, the library will ignore the transfer.
 * All transfers that are addressed to other nodes are always ignored.
 */
static bool shouldAcceptTransfer(const CanardInstance* ins,
                                 uint64_t* out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id)
{
    (void)source_node_id;

    if ((transfer_type == CanardTransferTypeRequest) &&
    (data_type_id == UAVCAN_PROTOCOL_GETNODEINFO_ID))
    {
        *out_data_type_signature = UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE;
        return true;
    }
        
    return false;
}


/**
 * This function is called at 1 Hz rate from the main loop.
 */
static void process1HzTasks(uint64_t timestamp_usec)
{
    /*
     * Purging transfers that are no longer transmitted. This will occasionally free up some memory.
     */
    canardCleanupStaleTransfers(&canard, timestamp_usec);

    /*
     * Printing the memory usage statistics.
     */
    {
        const CanardPoolAllocatorStatistics stats = canardGetPoolAllocatorStatistics(&canard);
        const uint16_t peak_percent = (uint16_t)(100U * stats.peak_usage_blocks / stats.capacity_blocks);

        printf("Memory pool stats: capacity %u blocks, usage %u blocks, peak usage %u blocks (%u%%)\n",
               stats.capacity_blocks, stats.current_usage_blocks, stats.peak_usage_blocks, peak_percent);

        /*
         * The recommended way to establish the minimal size of the memory pool is to stress-test the application and
         * record the worst case memory usage.
         */
        if (peak_percent > 70)
        {
            puts("WARNING: ENLARGE MEMORY POOL");
        }
    }

    /*
     * Transmitting the node status message periodically.
     */
    {
        uint8_t buffer[UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE];
        uint32_t length;
        makeNodeStatusMessage(buffer, &length);

        static uint8_t transfer_id;  // Note that the transfer ID variable MUST BE STATIC (or heap-allocated)!

        const int16_t bc_res = canardBroadcast(&canard,
                                               UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE,
                                               UAVCAN_PROTOCOL_NODESTATUS_ID,
                                               &transfer_id,
                                               CANARD_TRANSFER_PRIORITY_LOW,
                                               buffer,
                                               length);
        if (bc_res <= 0)
        {
            (void)fprintf(stderr, "Could not broadcast node status; error %d\n", bc_res);
        }
    }

    node_mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
}


/**
 * Transmits all frames from the TX queue, receives up to one frame.
 */
static void processTxRxOnce(SocketCANInstance* socketcan, int32_t timeout_msec)
{
    // Transmitting
    for (const CanardCANFrame* txf = NULL; (txf = canardPeekTxQueue(&canard)) != NULL;)
    {
        const int16_t tx_res = socketcanTransmit(socketcan, txf, 0);
        if (tx_res < 0)         // Failure - drop the frame and report
        {
            canardPopTxQueue(&canard);
            (void)fprintf(stderr, "Transmit error %d, frame dropped, errno '%s'\n", tx_res, strerror(errno));
        }
        else if (tx_res > 0)    // Success - just drop the frame
        {
            canardPopTxQueue(&canard);
        }
        else                    // Timeout - just exit and try again later
        {
            break;
        }
    }

    // Receiving
    CanardCANFrame rx_frame;
    const uint64_t timestamp = getMonotonicTimestampUSec();
    const int16_t rx_res = socketcanReceive(socketcan, &rx_frame, timeout_msec);
    if (rx_res < 0)             // Failure - report
    {
        (void)fprintf(stderr, "Receive error %d, errno '%s'\n", rx_res, strerror(errno));
    }
    else if (rx_res > 0)        // Success - process the frame
    {
        canardHandleRxFrame(&canard, &rx_frame, timestamp);
    }
    else
    {
        ;                       // Timeout - nothing to do
    }
}


int main(int argc, char** argv)
{
    if (argc < 3)
    {
        (void)fprintf(stderr,
                      "Usage:\n"
                      "\t%s <can iface name> <NodeID>\n",
                      argv[0]);
        return 1;
    }

    /*
     * Initializing the CAN backend driver; in this example we're using SocketCAN
     */
    SocketCANInstance socketcan;
    const char* const can_iface_name = argv[1];
    int16_t res = socketcanInit(&socketcan, can_iface_name);
    if (res < 0)
    {
        (void)fprintf(stderr, "Failed to open CAN iface '%s'\n", can_iface_name);
        return 1;
    }

    /*
     * Initializing the Libcanard instance.
     */
    canardInit(&canard, canard_memory_pool, sizeof(canard_memory_pool), onTransferReceived, shouldAcceptTransfer, NULL);

    // char *p;
    // uint8_t local_node_id = (uint8_t) strtol(argv[2], &p, 10);
    uint8_t local_node_id = (uint8_t) atoi(argv[2]);
    canardSetLocalNodeID(&canard, local_node_id);
    
    /*
     * Running the main loop.
     */
    uint64_t next_1hz_service_at = getMonotonicTimestampUSec();

    for (;;)
    {
        processTxRxOnce(&socketcan, 10);

        const uint64_t ts = getMonotonicTimestampUSec();

        if (ts >= next_1hz_service_at)
        {
            next_1hz_service_at += 1000000;
            process1HzTasks(ts);
        }
    }

    return 0;
}
