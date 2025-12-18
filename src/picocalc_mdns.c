/**
 * @file picocalc_mdns.c
 * @brief mDNS Service Discovery Implementation
 * 
 * Simplified mDNS responder for advertising the 9P server.
 * This is a basic implementation that responds to queries for
 * the configured hostname and service.
 */

#include "picocalc_mdns.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

/* mDNS constants */
#define MDNS_PORT 5353
#define MDNS_MULTICAST_ADDR "224.0.0.251"
#define MDNS_TTL 120  /* Time to live in seconds */

/* DNS message types */
#define DNS_RRTYPE_A     1   /* IPv4 address */
#define DNS_RRTYPE_PTR   12  /* Pointer */
#define DNS_RRTYPE_TXT   16  /* Text */
#define DNS_RRTYPE_SRV   33  /* Service */

#define DNS_RRCLASS_IN   1   /* Internet */
#define DNS_RRCLASS_FLUSH 0x8000  /* Cache flush bit */

/* mDNS state */
static struct {
    bool initialized;
    bool running;
    struct udp_pcb *pcb;
    char hostname[64];
    char service_name[64];
    uint16_t port;
    ip_addr_t multicast_addr;
} g_mdns;

/* Forward declarations */
static void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port);
static void mdns_send_response(const ip_addr_t *dest_addr, u16_t dest_port,
                               const char *query_name, uint16_t query_type);

/* ========================================================================
 * Public API
 * ======================================================================== */

bool mdns_init(const char *hostname, const char *service_name, uint16_t port) {
    if (g_mdns.initialized) {
        return true;
    }
    
    memset(&g_mdns, 0, sizeof(g_mdns));
    
    /* Store configuration */
    strncpy(g_mdns.hostname, hostname, sizeof(g_mdns.hostname) - 1);
    strncpy(g_mdns.service_name, service_name, sizeof(g_mdns.service_name) - 1);
    g_mdns.port = port;
    
    /* Parse multicast address */
    if (!ipaddr_aton(MDNS_MULTICAST_ADDR, &g_mdns.multicast_addr)) {
        return false;
    }
    
    g_mdns.initialized = true;
    return true;
}

bool mdns_start(void) {
    if (!g_mdns.initialized || g_mdns.running) {
        return false;
    }
    
    /* Create UDP PCB */
    g_mdns.pcb = udp_new();
    if (!g_mdns.pcb) {
        return false;
    }
    
    /* Bind to mDNS port */
    err_t err = udp_bind(g_mdns.pcb, IP_ADDR_ANY, MDNS_PORT);
    if (err != ERR_OK) {
        udp_remove(g_mdns.pcb);
        g_mdns.pcb = NULL;
        return false;
    }
    
    /* Join multicast group */
    /* Note: IGMP support may not be enabled in lwIP configuration.
     * mDNS will still work without explicit multicast group membership,
     * though it may be less efficient. */
    #if LWIP_IGMP
    err = igmp_joingroup(IP_ADDR_ANY, &g_mdns.multicast_addr);
    if (err != ERR_OK) {
        udp_remove(g_mdns.pcb);
        g_mdns.pcb = NULL;
        return false;
    }
    #endif
    
    /* Set receive callback */
    udp_recv(g_mdns.pcb, mdns_recv_callback, NULL);
    
    g_mdns.running = true;
    
    /* Send initial announcement */
    mdns_send_response(&g_mdns.multicast_addr, MDNS_PORT, g_mdns.hostname, DNS_RRTYPE_A);
    
    return true;
}

void mdns_stop(void) {
    if (!g_mdns.running) {
        return;
    }
    
    /* Send goodbye packet (TTL=0) */
    /* TODO: Implement goodbye packet */
    
    /* Leave multicast group */
    if (g_mdns.pcb) {
        #if LWIP_IGMP
        igmp_leavegroup(IP_ADDR_ANY, &g_mdns.multicast_addr);
        #endif
        udp_remove(g_mdns.pcb);
        g_mdns.pcb = NULL;
    }
    
    g_mdns.running = false;
}

void mdns_poll(void) {
    /* lwIP handles polling internally */
}

bool mdns_is_running(void) {
    return g_mdns.running;
}

const char *mdns_get_hostname(void) {
    return g_mdns.initialized ? g_mdns.hostname : NULL;
}

/* ========================================================================
 * Internal Functions
 * ======================================================================== */

/**
 * @brief Parse DNS name from packet
 */
static int parse_dns_name(const uint8_t *data, int offset, int len, char *name, int name_len) {
    int pos = offset;
    int out_pos = 0;
    bool first = true;
    
    while (pos < len) {
        uint8_t label_len = data[pos++];
        
        if (label_len == 0) {
            break;  /* End of name */
        }
        
        if ((label_len & 0xC0) == 0xC0) {
            /* Compression pointer - not fully supported in this simple implementation */
            pos++;
            break;
        }
        
        if (!first && out_pos < name_len - 1) {
            name[out_pos++] = '.';
        }
        first = false;
        
        for (int i = 0; i < label_len && pos < len && out_pos < name_len - 1; i++) {
            name[out_pos++] = data[pos++];
        }
    }
    
    name[out_pos] = '\0';
    return pos;
}

/**
 * @brief Write DNS name to packet
 */
static int write_dns_name(uint8_t *data, int offset, const char *name) {
    int pos = offset;
    const char *p = name;
    
    while (*p) {
        /* Find next dot or end */
        const char *dot = strchr(p, '.');
        int label_len = dot ? (dot - p) : strlen(p);
        
        /* Write label length */
        data[pos++] = label_len;
        
        /* Write label */
        memcpy(data + pos, p, label_len);
        pos += label_len;
        
        /* Move to next label */
        p += label_len;
        if (*p == '.') p++;
    }
    
    /* Write terminator */
    data[pos++] = 0;
    
    return pos;
}

/**
 * @brief Handle incoming mDNS query
 */
static void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port) {
    if (!p || p->tot_len < 12) {
        pbuf_free(p);
        return;
    }
    
    /* Copy packet data */
    uint8_t *data = (uint8_t *)p->payload;
    
    /* Parse DNS header */
    uint16_t flags = (data[2] << 8) | data[3];
    uint16_t qdcount = (data[4] << 8) | data[5];
    
    /* Check if this is a query */
    if ((flags & 0x8000) != 0) {
        /* This is a response, ignore */
        pbuf_free(p);
        return;
    }
    
    /* Parse questions */
    int offset = 12;
    for (int i = 0; i < qdcount && offset < p->tot_len; i++) {
        char qname[256];
        offset = parse_dns_name(data, offset, p->tot_len, qname, sizeof(qname));
        
        if (offset + 4 > p->tot_len) {
            break;
        }
        
        uint16_t qtype = (data[offset] << 8) | data[offset + 1];
        offset += 4;  /* Skip type and class */
        
        /* Check if query matches our hostname */
        char full_hostname[128];
        snprintf(full_hostname, sizeof(full_hostname), "%s.local", g_mdns.hostname);
        
        if (strcasecmp(qname, full_hostname) == 0 || strcasecmp(qname, g_mdns.hostname) == 0) {
            /* Send response */
            mdns_send_response(addr, port, qname, qtype);
        }
    }
    
    pbuf_free(p);
}

/**
 * @brief Send mDNS response
 */
static void mdns_send_response(const ip_addr_t *dest_addr, u16_t dest_port,
                               const char *query_name, uint16_t query_type) {
    /* Allocate packet buffer */
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 512, PBUF_RAM);
    if (!p) {
        return;
    }
    
    uint8_t *data = (uint8_t *)p->payload;
    int pos = 0;
    
    /* DNS header */
    data[pos++] = 0;  /* Transaction ID */
    data[pos++] = 0;
    data[pos++] = 0x84;  /* Flags: Response, Authoritative */
    data[pos++] = 0x00;
    data[pos++] = 0;  /* Questions */
    data[pos++] = 0;
    data[pos++] = 0;  /* Answers */
    data[pos++] = 1;  /* One answer */
    data[pos++] = 0;  /* Authority RRs */
    data[pos++] = 0;
    data[pos++] = 0;  /* Additional RRs */
    data[pos++] = 0;
    
    /* Answer section */
    char full_hostname[128];
    snprintf(full_hostname, sizeof(full_hostname), "%s.local", g_mdns.hostname);
    pos = write_dns_name(data, pos, full_hostname);
    
    /* Type: A record */
    data[pos++] = 0;
    data[pos++] = DNS_RRTYPE_A;
    
    /* Class: IN with cache flush */
    data[pos++] = (DNS_RRCLASS_IN | DNS_RRCLASS_FLUSH) >> 8;
    data[pos++] = (DNS_RRCLASS_IN | DNS_RRCLASS_FLUSH) & 0xFF;
    
    /* TTL */
    data[pos++] = (MDNS_TTL >> 24) & 0xFF;
    data[pos++] = (MDNS_TTL >> 16) & 0xFF;
    data[pos++] = (MDNS_TTL >> 8) & 0xFF;
    data[pos++] = MDNS_TTL & 0xFF;
    
    /* Data length: 4 bytes for IPv4 address */
    data[pos++] = 0;
    data[pos++] = 4;
    
    /* IP address (get from netif) */
    /* TODO: Get actual IP address from network interface */
    data[pos++] = 192;
    data[pos++] = 168;
    data[pos++] = 1;
    data[pos++] = 100;
    
    /* Adjust packet length */
    pbuf_realloc(p, pos);
    
    /* Send packet */
    udp_sendto(g_mdns.pcb, p, dest_addr, dest_port);
    
    pbuf_free(p);
}