#include <api.h>

#include "afxdp.h"

//#define TEST_MODE

static xsk_socket_info_t* sockets[MAX_CPUS];

/**
 * Sets up AF_XDP sockets.
 * 
 * @param dev The interface to bind the AF_XDP sockets to.
 * @param queueId The TX queue ID to use.
 * @param needWakeup If 1, sets the need wakeup flag on the sockets.
 * @param sharedUmem If 1, uses a shared umem allocated between all sockets.
 * @param forceSkb If 1, forces SKB mode (slower, but useful for debugging).
 * @param zeroCopy If 1, will set the zero-copy flag.
 * @param threads The amount of sockets and threads to create.
 * 
 * @return 0 on success or other value on error.
 */
int Setup(const char *dev, int queueId, int needWakeup, int sharedUmem, int forceSkb, int zeroCopy, int threads) {
#ifdef TEST_MODE
    printf("Setting up %d AF_XDP sockets on '%s' (queueId => %d, needWakeup => %d, sharedUmem => %d, forceSkb => %d, zeroCopy => %d)...\n", threads, dev, queueId, needWakeup, sharedUmem, forceSkb, zeroCopy);
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
    
    for (int i = 0; i < threads; i++) {
        int queueIdToUse = queueId;

        if (queueId < 0)
            queueIdToUse = i;

        sockets[i] = SetupSocket(dev, i, queueIdToUse, xdpFlags, bindFlags, sharedUmem);
    }
#endif

    return 0;
}

/**
 * Cleans up AF_XDP sockets.
 * 
 * @param threads The amount of sockets and threads that were created in Setup().
 * 
 * @return 0 on success or other value on error.
 */
int Cleanup(int threads) {
#ifdef TEST_MODE
    printf("Cleaning up %d AF_XDP sockets...\n", threads);
#else
    for (int i = 0; i < threads; i++) {
        xsk_socket_info_t* xsk = sockets[i];

        if (xsk)
            CleanupSocket(xsk);
    }
#endif

    return 0;
}

/**
 * Sends a packet on an AF_XDP socket (at index).
 * 
 * @param pkt The packet buffer.
 * @param length The packet's full length (includes ethernet header, layer 3/4 headers, and payload).
 * @param threadIdx The socket index to send on.
 * @param batchSize The batch size.
 * 
 * @return 0 on success or other value on error.
 */
int SendPacket(void *pkt, int length, int threadIdx, int batchSize) {
#ifdef TEST_MODE
    printf("Sending %d bytes on AF_XDP socket at index %d (batchSize => %d)...\n", length, threadIdx, batchSize);
#else
    xsk_socket_info_t* xsk = sockets[threadIdx];

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