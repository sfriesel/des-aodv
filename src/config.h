#ifndef AODV_CONFIG
#define AODV_CONFIG

#include <dessert.h>

enum bool {TRUE = 1, FALSE = 0};

#define RREQ_RETRIES				2
#define RREQ_RATELIMIT				10
#define TTL_START					2
#define TTL_INCREMENT				2
#define TTL_THRESHOLD				7
#define SEQNO_MAX					((1 << 32 ) - 1)

#define ACTIVE_ROUTE_TIMEOUT		6000 	// milliseconds
#define ALLOWED_HELLO_LOST			7
#define NODE_TRAVERSAL_TIME			10 		// milliseconds
#define NET_DIAMETER				20
#define NET_TRAVERSAL_TIME			2 * NODE_TRAVERSAL_TIME * NET_DIAMETER
#define BLACKLIST_TIMEOUT			RREQ_RETRIES * NET_TRAVERSAL_TIME
#define MY_ROUTE_TIMEOUT			2 * ACTIVE_ROUTE_TIMEOUT
#define PATH_DESCOVERY_TIME			2 * NET_TRAVERSAL_TIME
#define RERR_RATELIMIT				10

#define RREQ_EXT_TYPE				DESSERT_EXT_USER
#define RREP_EXT_TYPE				DESSERT_EXT_USER + 1
#define RERR_EXT_TYPE				DESSERT_EXT_USER + 2
#define RERRDL_EXT_TYPE				DESSERT_EXT_USER + 3
#define HELLO_EXT_TYPE				DESSERT_EXT_USER + 4
#define BROADCAST_EXT_TYPE			DESSERT_EXT_USER + 5
#define PING_EXT_TYPE				DESSERT_EXT_USER + 6
#define PONG_EXT_TYPE				DESSERT_EXT_USER + 7
#define RL_EXT_TYPE					DESSERT_EXT_USER + 8

#define FIFO_BUFFER_MAX_ENTRY_SIZE	128 	// maximal packet count that can be stored in FIFO for one destination
#define DB_CLEANUP_INTERVAL			NET_TRAVERSAL_TIME
#define BUFFER_SENDOUT_DELAY		10
#define SCHEDULE_CHECK_INTERVAL		30 		// milliseconds

/**
 * Schedule type = send out packets from FIFO puffer for
 * destination with ether_addr
 */
#define AODV_SC_SEND_OUT_PACKET		1

/**
 * Schedule type = repeat RREQ with ttl *=2
 */
#define AODV_SC_REPEAT_RREQ			2

/**
 * Schedule type = send out route error for given next hop
 */
#define AODV_SC_SEND_OUT_RERR		3
#define MULTIPATH					FALSE
#define VERBOSE						FALSE
#define HELLO_SIZE					128
#define HELLO_INTERVAL				2000 	// milliseconds
#define RREQ_SIZE					128

// --- Database Flags

#define AODV_FLAGS_ROUTE_INVALID 	1
#define AODV_FLAGS_NEXT_HOP_UNKNOWN	1 << 1
#define MAX_MESH_IFACES_COUNT		8

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

extern dessert_periodic_t *			periodic_send_hello;
extern int 							multipath;
extern int 							verbose;
extern char*						routing_log_file;
extern int 							hello_size;
extern int 							hello_interval;
extern int 							rreq_size;

#endif
