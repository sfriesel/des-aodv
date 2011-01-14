#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include <string.h>
#include "../database/rl_seq_t/rl_seq.h"
#include <pthread.h>
#include <utlist.h>

int aodv_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t* hello_msg;
	dessert_ext_t* ext;

	// create new HELLO message with hello_ext.
	dessert_msg_new(&hello_msg);
	hello_msg->ttl = 2;

	dessert_msg_addext(hello_msg, &ext, HELLO_EXT_TYPE, sizeof(struct aodv_msg_hello));

	void* payload;
	uint16_t size = max(hello_size - sizeof(dessert_msg_t) - sizeof(struct ether_header) - 2, 0);
	dessert_msg_addpayload(hello_msg, &payload, size);
	memset(payload, 0xA, size);

	dessert_meshsend_fast(hello_msg, NULL);
	dessert_msg_destroy(hello_msg);
	return 0;
}

int aodv_periodic_cleanup_database(void *data, struct timeval *scheduled, struct timeval *interval) {
	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	if (aodv_db_cleanup(&timestamp)) return 0;
	else return 1;
}

dessert_msg_t* aodv_create_rerr(_onlb_element_t** head, u_int16_t count) {
	if (*head == NULL) return NULL;
	dessert_msg_t* msg;
	dessert_ext_t* ext;
	dessert_msg_new(&msg);

	// set ttl
	msg->ttl = 255;

	// add l25h header
	dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
	memcpy(rreq_l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(rreq_l25h->ether_dhost, ether_broadcast, ETH_ALEN);

	// add RERR ext
	dessert_msg_addext(msg, &ext, RERR_EXT_TYPE, sizeof(struct aodv_msg_rerr));
	struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) ext->data;
	rerr_msg->flags = AODV_FLAGS_RERR_N;

	// write addresses of all my mesh interfaces
	dessert_meshif_t* iface = dessert_meshiflist_get();
	void* ifaceaddr_pointer = rerr_msg->ifaces;
	u_int8_t ifaces_count = 0;
	while (iface != NULL && ifaces_count < MAX_MESH_IFACES_COUNT) {
		memcpy(ifaceaddr_pointer, iface->hwaddr, ETH_ALEN);
		ifaceaddr_pointer += ETH_ALEN;
		iface = iface->next;
		ifaces_count++;
	}

	rerr_msg->iface_addr_count = ifaces_count;

	// write addresses of affected destinations in RERRDL_EXT
	u_int8_t rerrdl_count = 0;
	u_int8_t ext_el_num = 0;
	void* dl = NULL;
	u_int8_t max_dl_len = DESSERT_MAXEXTDATALEN / ETH_ALEN;
	u_int8_t MAX_RERRDL_COUNT = (dessert_maxlen - sizeof(dessert_msg_t) - sizeof(struct ether_header) - sizeof(struct aodv_msg_rerr) - sizeof(struct aodv_msg_broadcast)) / DESSERT_MAXEXTDATALEN;

	while(*head != NULL && rerrdl_count < MAX_RERRDL_COUNT) {
		if (ext_el_num == 0) {
			dessert_msg_addext(msg, &ext, RERRDL_EXT_TYPE, ((count >= max_dl_len)? max_dl_len : count) * ETH_ALEN);
			dl = ext->data;
		}

		_onlb_element_t* el = *head;
		memcpy(dl, el->dhost_ether, ETH_ALEN);
		dl += ETH_ALEN;
		ext_el_num++;

		if (ext_el_num == max_dl_len) {
			rerrdl_count++;
			ext_el_num = 0;
		}

		DL_DELETE(*head, el);
		count--;
		free(el);
	}

	// add broadcast id ext since RERR is an broadcast message
	dessert_msg_addext(msg, &ext, BROADCAST_EXT_TYPE, sizeof(struct aodv_msg_broadcast));
	struct aodv_msg_broadcast* brc_str = (struct aodv_msg_broadcast*) ext->data;
	pthread_rwlock_wrlock(&pp_rwlock);
	brc_str->id = ++broadcast_id;
	pthread_rwlock_unlock(&pp_rwlock);

	return msg;
}

int aodv_periodic_scexecute(void *data, struct timeval *scheduled, struct timeval *interval) {
	u_int8_t schedule_type;
	u_int64_t schedule_param;
	u_int8_t ether_addr[ETH_ALEN];
	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);

	if (aodv_db_popschedule(&timestamp, ether_addr, &schedule_type, &schedule_param) == FALSE)  {
		return 0;
	}

	if (schedule_type == AODV_SC_SEND_OUT_PACKET) {
		if (multipath) {
			int do_while = TRUE;
			u_int8_t ether_next_hop[ETH_ALEN];
			const dessert_meshif_t* output_iface;
			while(do_while == TRUE) {
				dessert_msg_t* buffered_msg = aodv_db_pop_packet(ether_addr);
				if (buffered_msg != NULL) {
					if (aodv_db_getroute2dest(ether_addr, ether_next_hop, &output_iface, &timestamp) == TRUE) {
						if (be_verbose == TRUE) dessert_debug("send out packet from buffer");
						/* // no need to search for next hop. Next hop is the last_hop that send RREP */
						memcpy(buffered_msg->l2h.ether_dhost, ether_next_hop, ETH_ALEN);
						if (routing_log_file != NULL) {
							struct ether_header* l25h = dessert_msg_getl25ether(buffered_msg);
							u_int32_t seq_num = 0;
							pthread_rwlock_wrlock(&rlseqlock);
							seq_num = rl_get_nextseq(dessert_l25_defsrc, l25h->ether_dhost);
							pthread_rwlock_unlock(&rlseqlock);
							dessert_ext_t* rl_ext;
							dessert_msg_addext(buffered_msg, &rl_ext, RL_EXT_TYPE, sizeof(struct rl_seq));
							struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
							rl_data->seq_num = seq_num;
							rl_data->hop_count = 0;
							rlfile_log(dessert_l25_defsrc, l25h->ether_dhost, seq_num, 0, NULL, output_iface->hwaddr, ether_next_hop);
						}
						dessert_meshsend_fast(buffered_msg, output_iface);
						dessert_msg_destroy(buffered_msg);
					}
				} else {do_while = FALSE;}
			}
		}
	}
	else if (schedule_type == AODV_SC_REPEAT_RREQ) aodv_send_rreq(ether_addr, &timestamp, schedule_param);	// send out rreq
	else if (schedule_type == AODV_SC_SEND_OUT_RERR) {
		u_int32_t rerr_count;
		aodv_db_getrerrcount(&timestamp, &rerr_count);
		if (rerr_count < RERR_RATELIMIT) {
			u_int16_t dest_count = 0;
			u_int8_t dhost_ether[ETH_ALEN];
			_onlb_element_t* curr_el = NULL;
			_onlb_element_t* head = NULL;

			while(aodv_db_invroute(ether_addr, dhost_ether) == TRUE) {
				dessert_debug("invalidate route to %M", dhost_ether);
				dest_count++;
				curr_el = malloc(sizeof(_onlb_element_t));
				memcpy(curr_el->dhost_ether, dhost_ether, ETH_ALEN);
				curr_el->next = curr_el->prev = NULL;
				DL_APPEND(head, curr_el);
			}

			if (dest_count > 0) {
				while(head != NULL) {
				dessert_msg_t* rerr_msg = aodv_create_rerr(&head, dest_count);
					if (rerr_msg != NULL) {
						dessert_debug("link to %M break -> send RERR", ether_addr);
						dessert_meshsend_fast(rerr_msg, NULL);
						dessert_msg_destroy(rerr_msg);
						aodv_db_putrerr(&timestamp);
					}
				}
			}
		}
	}
	return 0;
}
