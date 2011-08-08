/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
       http://www.des-testbed.net
*******************************************************************************/

#ifndef AODV_CONFIG
#define AODV_CONFIG

#include <dessert.h>

#define RREQ_RETRIES				5 /* ferhat=5 rfc=2 */
#define RREQ_RATELIMIT				10 /* rfc=10 */
#define TTL_START					1 /* rfc=1 */
#define TTL_INCREMENT				2 /* rfc=2 */
#define TTL_THRESHOLD				7 /* rfc=7 */
#define TTL_MAX						UINT8_MAX

#define ACTIVE_ROUTE_TIMEOUT		3000 /* ms rfc=3000 */
#define ALLOWED_HELLO_LOST			4 /* christian=4 rfc=2 */
#define NODE_TRAVERSAL_TIME			2 /* ms christian=2 rfc=40 */
#define NET_DIAMETER				16 /* christian=8 rfc=35 */
#define NET_TRAVERSAL_TIME			(2 * NODE_TRAVERSAL_TIME * NET_DIAMETER) /* rfc */
#define BLACKLIST_TIMEOUT			(RREQ_RETRIES * NET_TRAVERSAL_TIME) /* rfc */
#define MY_ROUTE_TIMEOUT			(2 * ACTIVE_ROUTE_TIMEOUT) /* rfc */
#define PATH_DESCOVERY_TIME			(2 * NET_TRAVERSAL_TIME) /* rfc */
#define RERR_RATELIMIT				10 /* rfc=10 */

#define RREQ_EXT_TYPE				DESSERT_EXT_USER
#define RREP_EXT_TYPE				(DESSERT_EXT_USER + 1)
#define RERR_EXT_TYPE				(DESSERT_EXT_USER + 2)
#define RERRDL_EXT_TYPE				(DESSERT_EXT_USER + 3)
#define HELLO_EXT_TYPE				(DESSERT_EXT_USER + 4)
#define BROADCAST_EXT_TYPE			(DESSERT_EXT_USER + 5)

#define FIFO_BUFFER_MAX_ENTRY_SIZE	128 /* maximal packet count that can be stored in FIFO for one destination */
#define DB_CLEANUP_INTERVAL			NET_TRAVERSAL_TIME /* not in rfc */
#define SCHEDULE_CHECK_INTERVAL		20 /* ms not in rfc */

#define HELLO_INTERVAL				1000 /* ms rfc=1000 */

#define HELLO_SIZE					128 /* bytes */
#define RREQ_SIZE					128 /* bytes */

#define GOSSIPP						1 /* flooding */
#define DESTONLY					false

/**
 * Schedule type = send out packets from FIFO puffer for
 * destination with ether_addr
 */
#define AODV_SC_SEND_OUT_PACKET		1

/**
 * Schedule type = repeat RREQ
 */
#define AODV_SC_REPEAT_RREQ			2

/**
 * Schedule type = send out route error for given next hop
 */
#define AODV_SC_SEND_OUT_RERR		3

// --- Database Flags
#define AODV_FLAGS_UNUSED				0
#define AODV_FLAGS_ROUTE_INVALID 		1
#define AODV_FLAGS_NEXT_HOP_UNKNOWN		(1 << 1)
#define AODV_FLAGS_ROUTE_LOCAL_USED		(1 << 3)

#define MAX_MESH_IFACES_COUNT			8

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

extern dessert_periodic_t* 			periodic_send_hello;
extern uint16_t 					hello_size;
extern uint16_t 					hello_interval;
extern uint16_t 					rreq_size;
extern double 						gossipp;
extern int 							dest_only;

typedef struct aodv_link_break_element {
    uint8_t host[ETH_ALEN];
    uint32_t sequence_number;
    struct aodv_link_break_element* next;
    struct aodv_link_break_element* prev;
} aodv_link_break_element_t;

typedef struct aodv_mac_seq {
    uint8_t host[ETH_ALEN];
    uint32_t sequence_number;
} __attribute__((__packed__)) aodv_mac_seq_t;

#define MAX_MAC_SEQ_PER_EXT (DESSERT_MAXEXTDATALEN / sizeof(aodv_mac_seq_t))

#endif
