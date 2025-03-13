#include <pb/afxdp/api.h>

#include <linux/bpf.h>
#include <xdp/xsk.h>

#include <net/if.h>
#include <linux/if_link.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include <arpa/inet.h>

#include <string.h>

#define INTERFACE "enp1s0"

const char sMac[] = { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 };
const char dMac[] = { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 };

#define SRC_IP "127.0.0.1"
#define DST_IP "192.168.1.1"

#define SRC_PORT 51234
#define DST_PORT 80

#define BATCH_SIZE 1

int main() {
    int ret;

    void* xsk = Setup(INTERFACE, 0, 0, 0, 0, 0);

    // Setup.
    if (!xsk) {
        fprintf(stderr, "[ERR] Failed to setup AF_XDP socket: socket is NULL.\n");

        return 1;
    }

    // Create packet buffer.
    char buffer[2048];
    memset(&buffer, 0, sizeof(buffer));

    // Fill out ethernet header.
    struct ethhdr *eth = (struct ethhdr *)buffer;

    memcpy(&eth->h_source, &sMac, ETH_ALEN);
    memcpy(&eth->h_dest, &dMac, ETH_ALEN);
    eth->h_proto = htons(ETH_P_IP);

    // Convert IP strings to decimal.
    struct in_addr srcAddr, dstAddr;

    if (inet_pton(AF_INET, SRC_IP, &srcAddr) != 1) {
        fprintf(stderr, "[ERR] Failed to convert source IP (%s) to decimal.\n", SRC_IP);

        return 1;
    }

    if (inet_pton(AF_INET, DST_IP, &dstAddr) != 1) {
        fprintf(stderr, "[ERR] Failed to convert destination IP (%s) to decimal\n", DST_IP);

        return 1;
    }

    // Fill out IPv4 header.
    struct iphdr *iph = (struct iphdr *)(buffer + sizeof(struct ethhdr));

    iph->version = 4;
    iph->id = 0;
    iph->ihl = 5;
    iph->tos = 0;
    iph->ttl = 64;
    
    iph->protocol = IPPROTO_TCP;
    iph->saddr = srcAddr.s_addr;
    iph->daddr = dstAddr.s_addr;

    iph->tot_len = htons((iph->ihl * 4) + sizeof(struct tcphdr));

    iph->check = 0;

    // Fill out TCP header.
    struct tcphdr *tcph = (struct tcphdr *)(buffer + sizeof(struct ethhdr) + (iph->ihl * 4));

    tcph->source = htons(SRC_PORT);
    tcph->dest = htons(DST_PORT);

    tcph->syn = 1;

    tcph->check = 0;

    unsigned short pktLen = sizeof(struct ethhdr) + (iph->ihl * 4) + sizeof(struct tcphdr);

    // Send packet.
    if ((ret = SendPacket(xsk, (void *)buffer, pktLen, BATCH_SIZE)) != 0) {
        fprintf(stderr, "[ERR] Failed to send packet (size %d bytes): %d\n", pktLen, ret);

        return 1;
    }

    // Cleanup.
    if ((ret = Cleanup(xsk)) != 0) {
        fprintf(stderr, "[ERR] Failed to cleanup AF_XDP socket: %d\n", ret);

        return ret;
    }

    printf("Test successful!\n");

    return 0;
}