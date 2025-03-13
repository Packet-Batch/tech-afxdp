#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>

#include <net/if.h>

#include <sys/socket.h>
#include <linux/if_link.h>
#include <linux/bpf.h>

#include <xdp/xsk.h>

#include <simple_types.h>

#define MAX_CPUS 256
#define NUM_FRAMES 4096
#define FRAME_SIZE XSK_UMEM__DEFAULT_FRAME_SIZE
#define INVALID_UMEM_FRAME UINT64_MAX

typedef struct xsk_umem_info 
{
    struct xsk_ring_prod fq;
    struct xsk_ring_cons cq;
    struct xsk_umem *umem;
    void *buffer;
} xsk_umem_info_t;

typedef struct xsk_socket 
{
    struct xsk_ring_cons *rx;
    struct xsk_ring_prod *tx;
    u64 outstanding_tx;
    struct xsk_ctx *ctx;
    struct xsk_socket_config config;
    int fd;
} xsk_socket_t;

typedef struct xsk_socket_info
{
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_umem_info *umem;
    struct xsk_socket *xsk;

    u64 umem_frame_addr[NUM_FRAMES];
    u32 umem_frame_free;

    u32 outstanding_tx;

    u32 bindFlags;
} xsk_socket_info_t;

u64 GetUmemAddr(struct xsk_socket_info *xsk, int idx);
void *GetUmemLoc(struct xsk_socket_info *xsk, u64 addr);
int GetSocketFd(struct xsk_socket_info *xsk);
void CompleteTx(xsk_socket_info_t *xsk, int batchSize);

struct xsk_umem_info *SetupUmem();
xsk_socket_info_t* SetupSocket(const char *dev, int queueId, u32 xdpFlags, u32 bindFlags, int useSharedUmem);
void CleanupSocket(xsk_socket_info_t *xsk);