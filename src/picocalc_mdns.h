/**
 * @file picocalc_mdns.h
 * @brief mDNS Service Discovery for 9P Server
 * 
 * Provides mDNS/Bonjour responder to advertise the 9P server
 * on the local network for easy discovery.
 */

#ifndef PICOCALC_MDNS_H
#define PICOCALC_MDNS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize mDNS responder
 * 
 * Must be called after WiFi connection is established.
 * 
 * @param hostname Hostname to advertise (e.g., "picocalc")
 * @param service_name Service name (e.g., "PicoCalc 9P Server")
 * @param port Service port number
 * @return true on success, false on failure
 */
bool mdns_init(const char *hostname, const char *service_name, uint16_t port);

/**
 * @brief Start mDNS responder
 * 
 * Begins responding to mDNS queries on the network.
 * 
 * @return true on success, false on failure
 */
bool mdns_start(void);

/**
 * @brief Stop mDNS responder
 * 
 * Stops responding to mDNS queries and sends goodbye packets.
 */
void mdns_stop(void);

/**
 * @brief Process mDNS events
 * 
 * Must be called periodically to handle mDNS queries and announcements.
 * Typically called from the main network polling loop.
 */
void mdns_poll(void);

/**
 * @brief Check if mDNS is running
 * 
 * @return true if mDNS responder is active
 */
bool mdns_is_running(void);

/**
 * @brief Get mDNS hostname
 * 
 * @return Current hostname, or NULL if not initialized
 */
const char *mdns_get_hostname(void);

#endif /* PICOCALC_MDNS_H */