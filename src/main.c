#include "afxdp.h"

//#define TEST_MODE

/**
 * Sets up an AF_XDP socket.
 * 
 * @param dev The interface to bind the AF_XDP sockets to.
 * @param queueId The TX queue ID to use.
 * @param needWakeup If 1, sets the need wakeup flag on the sockets.
 * @param sharedUmem If 1, uses a shared umem allocated between all sockets.
 * @param forceSkb If 1, forces SKB mode (slower, but useful for debugging).
 * @param zeroCopy If 1, will set the zero-copy flag.
 * 
 * @return 0 on success or other value on error.
 */
void* Setup(const char *dev, int queueId, int needWakeup, int sharedUmem, int forceSkb, int zeroCopy) {
#ifdef TEST_MODE
    printf("Setting up AF_XDP socket on '%s' (queueId => %d, needWakeup => %d, sharedUmem => %d, forceSkb => %d, zeroCopy => %d)...\n", dev, queueId, needWakeup, sharedUmem, forceSkb, zeroCopy);
#else
    u32 xdpFlags = XDP_FLAGS_DRV_MODE;

    if (forceSkb)
        xdpFlags = XDP_FLAGS_SKB_MODE;
    
    u32 bindFlags = 0;

    if (needWakeup)
        bindFlags |= XDP_USE_NEED_WAKEUP;

    if (zeroCopy)
        bindFlags |= XDP_ZEROCOPY;
    else
        bindFlags |= XDP_COPY;

    return  (void*) SetupSocket(dev, queueId, xdpFlags, bindFlags, sharedUmem);
#endif

    return NULL;
}

/**
 * Cleans up an AF_XDP socket.
 * 
 * @param xskPtr A pointer to the AF_XDP socket.
 * 
 * @return 0 on success or other value on error.
 */
int Cleanup(void* xskPtr) {
#ifdef TEST_MODE
    printf("Cleaning up AF_XDP socket...\n");
#else
    xsk_socket_info_t* xsk = (xsk_socket_info_t*)xskPtr;

    if (xsk)
        CleanupSocket(xsk);
#endif

    return 0;
}

/**
 * Sends a packet on an AF_XDP socket (at index).
 * 
 * @param xskPtr A pointer to the AF_XDP socket.
 * @param pkt The packet buffer.
 * @param length The packet's full length (includes ethernet header, layer 3/4 headers, and payload).
 * @param batchSize The batch size.
 * 
 * @return 0 on success or other value on error.
 */
int SendPacket(void* xskPtr, void *pkt, int length, int batchSize) {
#ifdef TEST_MODE
    printf("Sending %d bytes on AF_XDP socket (batchSize => %d)...\n", length, batchSize);
#else
    xsk_socket_info_t* xsk = (xsk_socket_info_t*)xskPtr;
    
    if (!xsk)
        return -2;

    // This represents the TX index.
    u32 txIdx = 0;

    // Retrieve the TX index from the TX ring to fill.
    while (xsk_ring_prod__reserve(&xsk->tx, batchSize, &txIdx) < batchSize) {
        CompleteTx(xsk, batchSize);
    }

    unsigned int idx = 0;

    // Loop through to batch size.
    for (int i = 0; i < batchSize; i++) {
        // Retrieve index we want to insert at in UMEM and make sure it isn't equal/above to max number of frames.
        idx = xsk->outstanding_tx + i;

        if (idx > NUM_FRAMES)
            break;

        // We must retrieve the next available address in the UMEM.
        u64 addrat = GetUmemAddr(xsk, idx);

        // We must copy our packet data to the UMEM area at the specific index (idx * frame size). We did this earlier.
        memcpy(GetUmemLoc(xsk, addrat), pkt, length);

        // Retrieve TX descriptor at index.
        struct xdp_desc *tx_desc = xsk_ring_prod__tx_desc(&xsk->tx, txIdx + i);

        // Point the TX ring's frame address to what we have in the UMEM.
        tx_desc->addr = addrat;

        // Tell the TX ring the packet length.
        tx_desc->len = length;
    }

    // Submit the TX batch to the producer ring.
    xsk_ring_prod__submit(&xsk->tx, batchSize);

    // Increase outstanding.
    xsk->outstanding_tx += batchSize;

    // Complete TX again.
    CompleteTx(xsk, batchSize);
#endif

    return 0;
}