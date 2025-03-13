#include "afxdp.h"

static unsigned int global_frame_idx = 0;
static xsk_umem_info_t* sharedUmem = NULL;

/**
 * Completes the TX call via a syscall and also checks if we need to free the TX buffer.
 * 
 * @param xsk A pointer to the xsk_socket_info structure.
 * @param batchSize The batch size to use.
 * 
 * @return Void
**/
void CompleteTx(xsk_socket_info_t *xsk, int batchSize) {
    // Initiate starting variables (completed amount and completion ring index).
    unsigned int completed;
    uint32_t idxCq;

    // If outstanding is below 1, it means we have no packets to TX.
    if (!xsk->outstanding_tx)
        return;

    // If we need to wakeup, execute syscall to wake up socket.
    if (!(xsk->bindFlags & XDP_USE_NEED_WAKEUP) || xsk_ring_prod__needs_wakeup(&xsk->tx))
        sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);

    // Try to free n (batchSize) frames on the completetion ring.
    completed = xsk_ring_cons__peek(&xsk->umem->cq, batchSize, &idxCq);

    if (completed > 0) {
        // Release "completed" frames.
        xsk_ring_cons__release(&xsk->umem->cq, completed);

        xsk->outstanding_tx -= completed;
    }
}

/**
 * Configures the UMEM area for our AF_XDP/XSK sockets to use for rings.
 * 
 * @param buffer The blank buffer we allocated in setup_socket().
 * @param size The buffer size.
 * 
 * @return Returns a pointer to the UMEM area instead of the XSK UMEM information structure (struct xsk_umem_info).
**/
static xsk_umem_info_t *ConfigureXskUmem(void *buffer, u64 size)
{
    // Create umem pointer and return variable.
    xsk_umem_info_t *umem;
    int ret;

    // Allocate memory space to the umem pointer and check.
    umem = calloc(1, sizeof(*umem));

    if (!umem)
        return NULL;

    // Attempt to create the umem area and check.
    ret = xsk_umem__create(&umem->umem, buffer, size, &umem->fq, &umem->cq, NULL);

    if (ret) {
        errno = -ret;

        return NULL;
    }

    // Assign the buffer we created in setup_socket() to umem buffer.
    umem->buffer = buffer;

    // Return umem pointer.
    return umem;
}

/**
 * Configures an AF_XDP/XSK socket.
 * 
 * @param umem A pointer to the umem we created in setup_socket().
 * @param dev The name of the interface we're binding to.
 * @param queueId The TX queue ID to use.
 * @param xdpFlags The XDP flags.
 * @param bindFlags The bind flags.
 * 
 * @return Returns a pointer to the AF_XDP/XSK socket inside of a the XSK socket info structure (struct xsk_socket_info).
**/
static xsk_socket_info_t* ConfigureXsk (xsk_umem_info_t *umem, const char *dev, int queueId, u32 xdpFlags, u32 bindFlags)
{
    // Initialize starting variables.
    struct xsk_socket_config xskCfg;
    struct xsk_socket_info *xsk;
    u32 idx;
    int i;
    int ret;

    // Allocate memory space to our XSK socket.
    xsk = calloc(1, sizeof(*xsk));

    // If it fails, return.
    if (!xsk) {
        fprintf(stderr, "Failed to allocate memory space to AF_XDP/XSK socket.\n");

        return NULL;
    }

    // Assign AF_XDP/XSK's socket umem area to the umem we allocated before.
    xsk->umem = umem;
    
    // Set the TX size (we don't need anything RX-related).
    xskCfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;

    // Make sure we don't load an XDP program via LibBPF.
    xskCfg.libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;

    // Assign our XDP flags.
    xskCfg.xdp_flags = xdpFlags;

    // Assign bind flags.
    xskCfg.bind_flags = bindFlags;
    xsk->bindFlags = bindFlags;

    // Attempt to create the AF_XDP/XSK socket itself at queue ID (we don't allocate a RX queue for obvious reasons).
    ret = xsk_socket__create(&xsk->xsk, dev, queueId, umem->umem, NULL, &xsk->tx, &xskCfg);

    if (ret) {
        fprintf(stderr, "Failed to create AF_XDP/XSK socket at creation (%d) (queue ID => %d).\n", ret, queueId);

        goto error_exit;
    }

    // Assign each umem frame to an address we'll use later.
    for (i = 0; i < NUM_FRAMES; i++) {
        xsk->umem_frame_addr[i] = i * FRAME_SIZE;
    }

    // Assign how many number of frames we can hold.
    xsk->umem_frame_free = NUM_FRAMES;

    // Return the AF_XDP/XSK socket information itself as a pointer.
    return xsk;

    // Handle error and return NULL.
    error_exit:
    errno = -ret;

    return NULL;
}

/*
 * Retrieves the socket FD of XSK socket.
 * 
 * @param xsk A pointer to the XSK socket info.
 * 
 * @return The socket FD (-1 on failure)
*/
int GetSocketFd(xsk_socket_info_t *xsk) {
    return xsk_socket__fd(xsk->xsk);
}

/**
 * Retrieves UMEM address at index we can fill with packet data.
 * 
 * @param xsk A pointer to the XSK socket info.
 * @param idx The index we're retrieving (make sure it is below NUM_FRAMES).
 * 
 * @return 64-bit address of location.
**/
u64 GetUmemAddr(xsk_socket_info_t *xsk, int idx) {
    return xsk->umem_frame_addr[idx];
}

/**
 * Retrieves the memory location in the UMEM at address.
 * 
 * @param xsk A pointer to the XSK socket info.
 * @param addr The address received by GetUmemAddr.
 * 
 * @return Pointer to address in memory of UMEM.
**/
void *GetUmemLoc(xsk_socket_info_t *xsk, u64 addr) {
    return xsk_umem__get_data(xsk->umem->buffer, addr);
}

/**
 * Sets up UMEM at specific index.
 * 
 * @return 0 on success and -1 on failure.
**/
xsk_umem_info_t *SetupUmem() {
    // This indicates the buffer for frames and frame size for the UMEM area.
    void *frameBuffer;
    u64 frameBufferSize = NUM_FRAMES * FRAME_SIZE;

    // Allocate blank memory space for the UMEM (aligned in chunks). Check as well.
    if (posix_memalign(&frameBuffer, getpagesize(), frameBufferSize)) {
        fprintf(stderr, "Could not allocate buffer memory for UMEM: %s (%d).\n", strerror(errno), errno);

        return NULL;
    }

    return ConfigureXskUmem(frameBuffer, frameBufferSize);
}

/**
 * Sets up an AF_XDP socket.
 * 
 * @param dev The interface name.
 * @param queueId The queue ID to assign.
 * @param xdpFlags The XDP flags.
 * @param bindFlags The XDP bind flags.
 * @param xsk_umem_info_t Whether to use a shared umem.
 * 
 * @return A pointer to the new XSK socket info structure (or NULL on failure).
**/
xsk_socket_info_t* SetupSocket(const char *dev, int queueId, u32 xdpFlags, u32 bindFlags, int useSharedUmem) {
    xsk_umem_info_t *umem;

    if (useSharedUmem) {
        if (sharedUmem == NULL)
            sharedUmem = SetupUmem();

        umem = sharedUmem;
    } else
        umem = SetupUmem();

    if (!umem)
        return NULL;

    return ConfigureXsk(umem, dev, queueId, xdpFlags, bindFlags);    
}

/**
 * Cleans up a specific AF_XDP/XSK socket.
 * 
 * @param xsk A pointer to the XSK socket info.
 * 
 * @return Void
**/
void CleanupSocket(xsk_socket_info_t *xsk) {
    // If the AF_XDP/XSK socket isn't NULL, delete it.
    if (xsk->xsk != NULL)
        xsk_socket__delete(xsk->xsk);

    // If the UMEM isn't NULL, delete it.
    if (xsk->umem != NULL)
        xsk_umem__delete(xsk->umem->umem);
}