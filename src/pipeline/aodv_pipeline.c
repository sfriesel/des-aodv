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

#include <pthread.h>
#include <string.h>
#include <utlist.h>
#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include "../helper.h"
#include "../database/data_seq/data_seq.h"

pthread_rwlock_t pp_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t data_seq_lock = PTHREAD_RWLOCK_INITIALIZER;

u_int32_t seq_num = 0;
u_int32_t broadcast_id = 0;
u_int16_t data_seq_num = 0;

// ---------------------------- help functions ---------------------------------------

dessert_msg_t* _create_rreq(u_int8_t dhost_ether[ETH_ALEN], u_int8_t ttl) {
	dessert_msg_t* msg;
	dessert_ext_t* ext;
	dessert_msg_new(&msg);

	msg->ttl = ttl;

	// add l25h header
	dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
	memcpy(rreq_l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(rreq_l25h->ether_dhost, dhost_ether, ETH_ALEN);

	// add RREQ ext
	dessert_msg_addext(msg, &ext, RREQ_EXT_TYPE, sizeof(struct aodv_msg_rreq));
	struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) ext->data;
	rreq_msg->hop_count = 0;
	rreq_msg->flags = 0;
	pthread_rwlock_wrlock(&pp_rwlock);
	rreq_msg->seq_num_src = ++seq_num;
	pthread_rwlock_unlock(&pp_rwlock);
	u_int32_t dhost_seq_num;
	if (aodv_db_getrouteseqnum(dhost_ether, &dhost_seq_num) == TRUE) {
		rreq_msg->seq_num_dest = dhost_seq_num;
	} else {
		rreq_msg->flags |= AODV_FLAGS_RREQ_D | AODV_FLAGS_RREQ_U;
	}

	// add broadcast id ext since RREQ is an broadcast message
	dessert_msg_addext(msg, &ext, BROADCAST_EXT_TYPE, sizeof(struct aodv_msg_broadcast));
	struct aodv_msg_broadcast* brc_str = (struct aodv_msg_broadcast*) ext->data;
	pthread_rwlock_wrlock(&pp_rwlock);
	brc_str->id = ++broadcast_id;
	pthread_rwlock_unlock(&pp_rwlock);

	return msg;
}


dessert_msg_t* _create_rrep(u_int8_t route_dest[ETH_ALEN], u_int8_t route_source[ETH_ALEN],
		u_int8_t rrep_next_hop[ETH_ALEN], u_int32_t route_seq_num, u_int8_t flags) {
	dessert_msg_t* msg;
	dessert_ext_t* ext;
	dessert_msg_new(&msg);

	msg->ttl = 255;

	// add l25h header
	dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
	memcpy(rreq_l25h->ether_shost, route_dest, ETH_ALEN);
	memcpy(rreq_l25h->ether_dhost, route_source, ETH_ALEN);

	// set next hop
	memcpy(msg->l2h.ether_dhost, rrep_next_hop, ETH_ALEN);

	// and add RREP ext
	dessert_msg_addext(msg, &ext, RREP_EXT_TYPE, sizeof(struct aodv_msg_rrep));
	struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) ext->data;
	rrep_msg->flags = flags;
	rrep_msg->hop_count = 0;
	rrep_msg->lifetime = 0;
	rrep_msg->seq_num_dest = route_seq_num;
	return msg;
}

void aodv_send_rreq(u_int8_t dhost_ether[ETH_ALEN], struct timeval* ts, u_int8_t ttl) {
	// fist check if we have sent more then RREQ_LIMITH RREQ messages at last 1 sek.
	u_int32_t rreq_count;
	aodv_db_getrreqcount(ts, &rreq_count);
	if (rreq_count >= RREQ_RATELIMIT) { // we have reached RREQ_RATELIMIT -> send this RREQ later(try in 100ms)!
		struct timeval retry_time;
		retry_time.tv_sec = 0;
		retry_time.tv_usec = (100) * 1000;
		hf_add_tv(ts, &retry_time, &retry_time);
		aodv_db_addschedule(&retry_time, dhost_ether, AODV_SC_REPEAT_RREQ, ttl);
		return;
	}

	dessert_msg_t* rreq_msg = _create_rreq(dhost_ether, (ttl <= TTL_THRESHOLD)? ttl : 255); // create RREQ

	// add task to repeat RREQ after
	u_int32_t rep_time = (ttl > TTL_THRESHOLD)? NET_TRAVERSAL_TIME : (2 * NODE_TRAVERSAL_TIME * ttl);
	struct timeval rreq_repeat_time;
	rreq_repeat_time.tv_sec = rep_time / 1000;
	rreq_repeat_time.tv_usec = (rep_time % 1000) * 1000;
	hf_add_tv(ts, &rreq_repeat_time, &rreq_repeat_time);

	if (ttl <= TTL_THRESHOLD) { // TTL_THRESHOLD + 1 means: this is a first try from RREQ_RETRIES
		aodv_db_addschedule(&rreq_repeat_time, dhost_ether, AODV_SC_REPEAT_RREQ,
				(ttl + TTL_INCREMENT > TTL_THRESHOLD)? TTL_THRESHOLD + 1 : ttl + TTL_INCREMENT);
	} else {
		u_int8_t try_num = ttl - TTL_THRESHOLD;
		if (try_num < RREQ_RETRIES) {
			aodv_db_addschedule(&rreq_repeat_time, dhost_ether, AODV_SC_REPEAT_RREQ, ttl + 1);
		}
	}
	aodv_db_putrreq(ts);

	void* payload;
	uint16_t size = max(rreq_size - sizeof(dessert_msg_t) - sizeof(struct ether_header) - 2, 0);
	dessert_msg_addpayload(rreq_msg, &payload, size);
	memset(payload, 0xA, size);

	// send out and destroy
	dessert_meshsend_fast(rreq_msg, NULL);
	dessert_msg_destroy(rreq_msg);
	dessert_debug("rreq send for " MAC " id=%d", EXPLODE_ARRAY6(dhost_ether), seq_num);
}

void aodv_send_packets_from_buffer(u_int8_t ether_dhost[ETH_ALEN], u_int8_t next_hop[ETH_ALEN], const dessert_meshif_t* iface) {
	// drop RREQ schedule, since we already know the route to destination
	aodv_db_dropschedule(ether_dhost, AODV_SC_REPEAT_RREQ);

	dessert_debug("new route to " MAC " over " MAC " found -> send out packet from buffer",
	              EXPLODE_ARRAY6(ether_dhost),
	              EXPLODE_ARRAY6(next_hop));

	// send out packets from buffer
	dessert_msg_t* buffered_msg;
	while((buffered_msg = aodv_db_pop_packet(ether_dhost)) != NULL) {
		struct ether_header* l25h = dessert_msg_getl25ether(buffered_msg);
		u_int16_t seq_num_copy = 0;
		pthread_rwlock_wrlock(&data_seq_lock);
		seq_num_copy = ++data_seq_num;
		pthread_rwlock_unlock(&data_seq_lock);
		buffered_msg->u16 = seq_num_copy;

		/*  no need to search for next hop. Next hop is the last_hop that send RREP */
		memcpy(buffered_msg->l2h.ether_dhost, next_hop, ETH_ALEN);
		dessert_meshsend_fast(buffered_msg, iface);

		dessert_trace("data packet - id=%d - to mesh - to " MAC " route is known - send over " MAC, seq_num, EXPLODE_ARRAY6(l25h->ether_dhost), EXPLODE_ARRAY6(next_hop));

		dessert_msg_destroy(buffered_msg);
	}
}

// ---------------------------- pipeline callbacks ---------------------------------------------

int aodv_drop_errors(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	// drop packets sent by myself.
	if (proc->lflags & DESSERT_LFLAG_PREVHOP_SELF) {
		return DESSERT_MSG_DROP;
	}
	if (proc->lflags & DESSERT_LFLAG_SRC_SELF) {
		return DESSERT_MSG_DROP;
	}
	/**
	 * Check neighborhood and broadcast id.
	 * First check neighborhood, since it is possible that one
	 * RREQ from one unidirectional neighbor can be added to broadcast id table
	 * and then dropped as a message from unidirectional neighbor!
	 * -> Dirty entry in broadcast id table.
	 */
	dessert_ext_t* ext;
	struct timeval ts;
	gettimeofday(&ts, NULL);

	// check whether control messages were sent over bidirectional links, otherwise DROP
	// Hint: RERR must be resent in both directions.
	if ((dessert_msg_getext(msg, &ext, RREQ_EXT_TYPE, 0) != 0)
			|| (dessert_msg_getext(msg, &ext, RREP_EXT_TYPE, 0) != 0)) {
		if (aodv_db_check2Dneigh(msg->l2h.ether_shost, iface, &ts) != TRUE) {
			return DESSERT_MSG_DROP;
		}
	}

	// drop broadcast errors (packet with given brc_id is were already processed)
	if (dessert_msg_getext(msg, &ext, BROADCAST_EXT_TYPE, 0) != 0) {
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		struct aodv_msg_broadcast* brc_msg = (struct aodv_msg_broadcast*) ext->data;

		// Drop all broadcasts with broadcast extensions send from me
		if (memcmp(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN) == 0) {
			return DESSERT_MSG_DROP;
		}

		// or if packet were already processed
		if (aodv_db_add_brcid(l25h->ether_shost, brc_msg->id, &ts) != TRUE) {
			return DESSERT_MSG_DROP;
		}
	}
	return DESSERT_MSG_KEEP;
}

int aodv_handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){

	dessert_ext_t* hallo_ext;
	if (dessert_msg_getext(msg, &hallo_ext, HELLO_EXT_TYPE, 0) == 0) {
		return DESSERT_MSG_KEEP;
	}

	msg->ttl--;
	if (msg->ttl >= 1) {
		// hello req
		memcpy(msg->l2h.ether_dhost, msg->l2h.ether_shost, ETH_ALEN);
		dessert_meshsend(msg, iface);
	} else {
		//hello rep
		if (memcmp(iface->hwaddr, msg->l2h.ether_dhost, ETH_ALEN) == 0) {
			struct timeval ts;
			gettimeofday(&ts, NULL);
			aodv_db_cap2Dneigh(msg->l2h.ether_shost, iface, &ts);
		}
	}
	return DESSERT_MSG_DROP;
}

int aodv_handle_rreq(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t* rreq_ext;

	if (dessert_msg_getext(msg, &rreq_ext, RREQ_EXT_TYPE, 0) == 0) {
		return DESSERT_MSG_KEEP;
	}

	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	msg->ttl--;

	struct timeval ts;
	gettimeofday(&ts, NULL);
	struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) rreq_ext->data;

	u_int8_t prev_hop[ETH_ALEN];
	rreq_msg->hop_count++;
	memcpy(prev_hop, msg->l2h.ether_shost, ETH_ALEN);

	if (memcmp(dessert_l25_defsrc, l25h->ether_dhost, ETH_ALEN) != 0) { // RREQ not for me

		dessert_trace("incoming RREQ from " MAC " to " MAC " seq=%i", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->seq_num_src);

		u_int32_t last_rreq_seq;
		int f = !(rreq_msg->flags & (AODV_FLAGS_RREQ_D | AODV_FLAGS_RREQ_U));
		int a = aodv_db_getrouteseqnum(l25h->ether_dhost, &last_rreq_seq);
		int b = (hf_seq_comp_i_j(rreq_msg->seq_num_src, last_rreq_seq) > 0);
		if(a && b && f) {
			// i know route to destination that have seq_num greater then that of source (route is newer)
			dessert_msg_t* rrep_msg = _create_rrep(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, last_rreq_seq, AODV_FLAGS_RREP_A);

			dessert_debug("repair link to " MAC " id=%d", EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->seq_num_src);

			dessert_meshsend_fast(rrep_msg, iface);
			dessert_msg_destroy(rrep_msg);
		} else if (msg->ttl > 0 && a == FALSE) {
			dessert_debug("route to " MAC " id=%d is unknown for me -> rebroadcast RREQ", EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->seq_num_src);
			dessert_meshsend_fast(msg, NULL);
		}
	} else { // RREQ for me

		dessert_debug("incoming RREQ from " MAC " for me seq=%i -> answer with RREP seq=%i", EXPLODE_ARRAY6(l25h->ether_shost), rreq_msg->seq_num_src, seq_num);

		u_int32_t last_rreq_seq;
		int a = aodv_db_getrouteseqnum(l25h->ether_shost, &last_rreq_seq);
		int b = (hf_seq_comp_i_j(rreq_msg->seq_num_src, last_rreq_seq) > 0);
		dessert_trace("rreq_msg->seq_num_src=%u last_rreq_seq=%u -> hf_seq_comp_i_j(rreq_msg->seq_num_src, last_rreq_seq)=%d", rreq_msg->seq_num_src, last_rreq_seq, hf_seq_comp_i_j(rreq_msg->seq_num_src, last_rreq_seq));
		if(!a || (a && b)) {
			// RREQ for me -> answer with RREP
			dessert_debug("got RREQ for me -> answer with RREP to " MAC " over " MAC " id=%d", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), seq_num);
			pthread_rwlock_wrlock(&pp_rwlock);
			u_int32_t seq_num_copy = ++seq_num;
			pthread_rwlock_unlock(&pp_rwlock);
			dessert_msg_t* rrep_msg = _create_rrep(dessert_l25_defsrc, l25h->ether_shost, msg->l2h.ether_shost, seq_num_copy, AODV_FLAGS_RREP_A);
			dessert_meshsend_fast(rrep_msg, iface);
			dessert_msg_destroy(rrep_msg);
		} else {
			dessert_debug("got RREQ for me -> don't answer with RREP route unknown or DUP (rreq_msg->seq_num_src=%u last_rreq_seq=%u) to " MAC " over " MAC,
			              rreq_msg->seq_num_src, last_rreq_seq, EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost));
		}

		/* RREQ gives route to his source. Process RREQ also as RREP */
		int y = aodv_db_capt_rrep(l25h->ether_shost, prev_hop, iface, rreq_msg->seq_num_src, rreq_msg->hop_count, &ts);
		if (y == TRUE) {
			// no need to search for next hop. Next hop is RREQ.prev_hop
			aodv_send_packets_from_buffer(l25h->ether_shost, prev_hop, iface);
		} else {
			// we know a better route already
		}
	}

	int x = aodv_db_capt_rreq(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, iface, rreq_msg->seq_num_src, &ts);
	if(x == -1) {
		dessert_crit("aodv_db_capt_rreq returns error");
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_DROP;
}

int aodv_handle_rerr(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t* rerr_ext;

	if (dessert_msg_getext(msg, &rerr_ext, RERR_EXT_TYPE, 0) == 0) {
		return DESSERT_MSG_KEEP;
	}
	struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) rerr_ext->data;
	int rerrdl_num = 0;
	u_int8_t dhost_ether[ETH_ALEN];
	u_int8_t dhost_next_hop[ETH_ALEN];
	u_int8_t rebroadcast_rerr = FALSE;
	dessert_ext_t* rerrdl_ext;

	while (dessert_msg_getext(msg, &rerrdl_ext, RERRDL_EXT_TYPE, rerrdl_num++) > 0) {
		int i;
		void* dhost_pointer = rerrdl_ext->data;
		for (i = 0; i < rerrdl_ext->len / ETH_ALEN; i++) {
			// get the MAC address of destination that is no more reachable
			memcpy(dhost_ether, dhost_pointer + i*ETH_ALEN, ETH_ALEN);
			// get next hop towards this destination
			if (aodv_db_getnexthop(dhost_ether, dhost_next_hop) == TRUE) {
				// if found, compare with entrys in interface-list this RRER.
				// If equals then this this route is affected and must be invalidated!
				int iface_num;
				for (iface_num = 0; iface_num < rerr_msg->iface_addr_count; iface_num++) {
					if (memcmp(rerr_msg->ifaces + iface_num * ETH_ALEN, dhost_next_hop, ETH_ALEN) == 0) {
						rebroadcast_rerr = TRUE;
						aodv_db_markrouteinv(dhost_ether);
						dessert_debug("route to " MAC " marked as invalid", EXPLODE_ARRAY6(dhost_ether));
					}
				}
			}
		}
	}

	if (rebroadcast_rerr) { // write addresses of all my mesh interfaces
		dessert_meshif_t* iface = dessert_meshiflist_get();
		rerr_msg->iface_addr_count = 0;
		void* iface_addr_pointer = rerr_msg->ifaces;

		while (iface != NULL && rerr_msg->iface_addr_count < MAX_MESH_IFACES_COUNT) {
			memcpy(iface_addr_pointer, iface->hwaddr, ETH_ALEN);
			iface_addr_pointer += ETH_ALEN;
			iface = iface->next;
			rerr_msg->iface_addr_count++;
		}

		dessert_meshsend_fast(msg, NULL);
	}
	return DESSERT_MSG_DROP;
}

int aodv_handle_rrep(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t* rrep_ext;

	if (dessert_msg_getext(msg, &rrep_ext, RREP_EXT_TYPE, 0) == 0) {
		return DESSERT_MSG_KEEP;
	}
	if (memcmp(msg->l2h.ether_dhost, iface->hwaddr, ETH_ALEN) != 0) {
		return DESSERT_MSG_DROP;
	}
	msg->ttl--;
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) rrep_ext->data;
	struct timeval ts;
	gettimeofday(&ts, NULL);
	rrep_msg->hop_count++;

	int x = aodv_db_capt_rrep(l25h->ether_shost, msg->l2h.ether_shost, iface, rrep_msg->seq_num_dest, rrep_msg->hop_count, &ts);
	if(x == -1) {
		dessert_crit("aodv_db_capt_rreq returns error");
		return DESSERT_MSG_DROP;
	}

	if(x == FALSE) {
		// capture and re-send only if route is unknown OR
		// sequence number is greater then that in database OR
		// if seq_nums are equals and known hop count is greater than that in RREP
		return DESSERT_MSG_DROP;
	}

	if(msg->ttl <= 0) {
		dessert_debug("got RREP from " MAC " but TTL ist <= 0", EXPLODE_ARRAY6(l25h->ether_dhost));
		return DESSERT_MSG_DROP;
	}

	if (memcmp(dessert_l25_defsrc, l25h->ether_dhost, ETH_ALEN) != 0) {
		// send RREP to RREQ originator if RREP is not for me
		u_int8_t next_hop[ETH_ALEN];
		const dessert_meshif_t* output_iface;
		
		if (aodv_db_getprevhop(l25h->ether_shost, l25h->ether_dhost, next_hop, &output_iface) == TRUE) {
			dessert_debug("re-send RREP to " MAC, EXPLODE_ARRAY6(l25h->ether_dhost));
			memcpy(msg->l2h.ether_dhost, next_hop, ETH_ALEN);
			dessert_meshsend_fast(msg, output_iface);
		}
	} else {
		// this RREP is for me! -> pop all packets from FIFO buffer and send to destination
		dessert_ext_t* ext;
		dessert_msg_getext(msg, &ext, RREP_EXT_TYPE, 0);
		struct aodv_msg_rrep* aodv_msg_rrep = (struct aodv_msg_rrep *) ext->data;
		dessert_debug("got RREP from " MAC " seq=%i -> aodv_send_packets_from_buffer", EXPLODE_ARRAY6(l25h->ether_shost), aodv_msg_rrep->seq_num_dest);
		/* no need to search for next hop. Next hop is RREP.prev_hop */
		aodv_send_packets_from_buffer(l25h->ether_shost, msg->l2h.ether_shost, iface);
	}
	return DESSERT_MSG_DROP;
}

int aodv_forward_broadcast(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if (proc->lflags & DESSERT_LFLAG_DST_BROADCAST) {
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		dessert_debug("got BROADCAST from " MAC " over " MAC, EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost));
		dessert_meshsend_fast(msg, NULL); //forward to mesh
		dessert_syssend_msg(msg); //forward to sys
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

int aodv_forward_multicast(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if (proc->lflags & DESSERT_LFLAG_DST_MULTICAST) {
		dessert_meshsend_fast(msg, NULL); //forward to mesh
		dessert_syssend_msg(msg); //forward to sys
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

int aodv_forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {

	if(proc->lflags & DESSERT_LFLAG_DST_SELF)
		return DESSERT_MSG_KEEP;
	if(proc->lflags & DESSERT_LFLAG_DST_SELF_OVERHEARD)
		return DESSERT_MSG_KEEP;
	if(proc->lflags & DESSERT_LFLAG_NEXTHOP_SELF)
		return DESSERT_MSG_KEEP;
	if(proc->lflags & DESSERT_LFLAG_NEXTHOP_SELF_OVERHEARD)
		return DESSERT_MSG_KEEP;

	const dessert_meshif_t* output_iface;
	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	u_int8_t next_hop[ETH_ALEN];
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	
	if(TRUE != aodv_db_data_capt_data_seq(l25h->ether_shost, msg->u16)) {
		//data packet is known -> DUP
		return DESSERT_MSG_DROP;
	}
		
	if (aodv_db_getroute2dest(l25h->ether_dhost, next_hop, &output_iface, &timestamp)) {
		dessert_debug(MAC " -------> " MAC, EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost));
		memcpy(msg->l2h.ether_dhost, next_hop, ETH_ALEN);
		dessert_meshsend_fast(msg, output_iface);
	} else {
		u_int32_t rerr_count;
		aodv_db_getrerrcount(&timestamp, &rerr_count);
		if (rerr_count >= RERR_RATELIMIT)
			return DESSERT_MSG_DROP;
		// route unknown -> send rerr towards source
		_onlb_element_t *head, *curr_el;
		
		curr_el = malloc(sizeof(_onlb_element_t));
		memcpy(curr_el->dhost_ether, l25h->ether_dhost, ETH_ALEN);
		head = NULL;
		DL_APPEND(head, curr_el);
		dessert_msg_t* rerr_msg = aodv_create_rerr(&head, 1);
		if (rerr_msg != NULL) {
			dessert_meshsend_fast(rerr_msg, NULL);
			dessert_msg_destroy(rerr_msg);
			aodv_db_putrerr(&timestamp);
		}
	}
	return DESSERT_MSG_DROP;
}

// --------------------------- TUN ----------------------------------------------------------

int aodv_sys2rp (dessert_msg_t *msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id) {
	struct ether_header* l25h = dessert_msg_getl25ether(msg);

	if (memcmp(l25h->ether_dhost, ether_broadcast, ETH_ALEN) == 0) {
		dessert_trace("send broadcast message -> add broadcast extension to avoid broadcast loops");
		dessert_ext_t* ext;
		dessert_msg_addext(msg, &ext, BROADCAST_EXT_TYPE, sizeof(struct aodv_msg_broadcast));
		struct aodv_msg_broadcast* brc_str = (struct aodv_msg_broadcast*) ext->data;
		pthread_rwlock_wrlock(&pp_rwlock);
		brc_str->id = ++broadcast_id;
		pthread_rwlock_unlock(&pp_rwlock);
		dessert_meshsend_fast(msg, NULL);
	} else {
		u_int8_t dhost_next_hop[ETH_ALEN];
		const dessert_meshif_t* output_iface;
		struct timeval ts;
		gettimeofday(&ts, NULL);
		if (aodv_db_getroute2dest(l25h->ether_dhost, dhost_next_hop, &output_iface, &ts) == TRUE) {
			u_int16_t seq_num_copy = 0;
			pthread_rwlock_wrlock(&data_seq_lock);
			seq_num_copy = ++data_seq_num;
			pthread_rwlock_unlock(&data_seq_lock);
			msg->u16 = seq_num_copy;

			memcpy(msg->l2h.ether_dhost, dhost_next_hop, ETH_ALEN);
			dessert_meshsend_fast(msg, output_iface);

			dessert_trace("send data packet to mesh - to " MAC " over " MAC " id=%d route is known", EXPLODE_ARRAY6(l25h->ether_dhost), EXPLODE_ARRAY6(dhost_next_hop), seq_num);
		} else {
			aodv_db_push_packet(l25h->ether_dhost, msg, &ts);
			aodv_send_rreq(l25h->ether_dhost, &ts, TTL_START); // create and send RREQ
			dessert_trace("send data packet to mesh - to " MAC " id=%d but route is unknown -> push packet to FIFO and send RREQ", EXPLODE_ARRAY6(l25h->ether_dhost), seq_num);
		}
	}
	return DESSERT_MSG_DROP;
}

// ----------------- common callbacks ---------------------------------------------------

/**
 * Forward packets addressed to me to tun pipeline
 */
int aodv_local_unicast(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if(proc->lflags & DESSERT_LFLAG_DST_SELF) {
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		if(TRUE == aodv_db_data_capt_data_seq(l25h->ether_shost, msg->u16)) {
			dessert_trace("data packet from mesh - from " MAC " over " MAC " id=%d", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), msg->u16);
			dessert_syssend_msg(msg);
		} else {
			dessert_trace("data packet from mesh - from " MAC " over " MAC " id=%d -> DUP", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), msg->u16);
		}
	}
	return DESSERT_MSG_DROP;
}
