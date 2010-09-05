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

#include "aodv_rt.h"
#include "../timeslot.h"
#include "../../config.h"
#include "../../helper.h"

#define  REPORT_RT_STR_LEN 90

typedef struct aodv_rt_srclist_entry {
	u_int8_t					shost_ether[ETH_ALEN]; // ID
	u_int8_t					shost_prev_hop[ETH_ALEN];
	const dessert_meshif_t*		output_iface;
	u_int32_t					shost_seq_num;
	UT_hash_handle				hh;
} aodv_rt_srclist_entry_t;

typedef struct aodv_rt_entry {
	u_int8_t					dhost_ether[ETH_ALEN]; // ID
	u_int8_t					dhost_next_hop[ETH_ALEN];
	const dessert_meshif_t*		output_iface;
	u_int32_t					dhost_seq_num;
	u_int8_t					hop_count;
	/**
	 * flags format: 0 0 0 0 0 0 U I
	 * I - Invalid flag; route is invalid due of link breakage
	 * U - next hop Unknown flag;
	 */
	u_int8_t					flags;
	aodv_rt_srclist_entry_t*	src_list;
	UT_hash_handle				hh;
} aodv_rt_entry_t;

typedef struct aodv_rt {
	aodv_rt_entry_t*			entrys;
	timeslot_t*					ts;
} aodv_rt_t;

aodv_rt_t						rt;

/**
 * Mapping next_hop -> destination list
 */
typedef struct nht_destlist_entry {
	u_int8_t					dhost_ether[ETH_ALEN];
	aodv_rt_entry_t*			rt_entry;
	UT_hash_handle				hh;
} nht_destlist_entry_t;

typedef struct nht_entry {
	u_int8_t					dhost_next_hop[ETH_ALEN];
	nht_destlist_entry_t*		dest_list;
	UT_hash_handle				hh;
} nht_entry_t;

nht_entry_t*					nht = NULL;

void purge_rt_entry(struct timeval* timestamp, void* src_object, void* del_object) {
	aodv_rt_entry_t* 			rt_entry = del_object;
	aodv_rt_srclist_entry_t*	src_list_entry;
	// delete all src_list entrys from routing entry
	while (rt_entry->src_list != NULL) {
		src_list_entry = rt_entry->src_list;
		HASH_DEL(rt_entry->src_list, src_list_entry);
		free(src_list_entry);
	}

	if (!(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)) {
		// delete mapping from next hop to this entry
		nht_entry_t* nht_entry;
		nht_destlist_entry_t* dest_entry;
		HASH_FIND(hh, nht, rt_entry->dhost_next_hop, ETH_ALEN, nht_entry);
		if (nht_entry != NULL) {
			HASH_FIND(hh, nht_entry->dest_list, rt_entry->dhost_ether, ETH_ALEN, dest_entry);
			if (dest_entry != NULL) {
				HASH_DEL(nht_entry->dest_list, dest_entry);
				free(dest_entry);
			}
			if (nht_entry->dest_list == NULL) {
				HASH_DEL(nht, nht_entry);
				free(nht_entry);
			}
		}
	}
	// delete routing entry
	if (be_verbose == TRUE)
		dessert_debug("delete route to *:%02x:%02x", rt_entry->dhost_ether[4], rt_entry->dhost_ether[5]);
	HASH_DEL(rt.entrys, rt_entry);
	free(rt_entry);
}

int aodv_db_rt_init() {
	rt.entrys = NULL;

	struct timeval	mrt; // my route timeout
	mrt.tv_sec = MY_ROUTE_TIMEOUT / 1000;
	mrt.tv_usec = (MY_ROUTE_TIMEOUT % 1000) * 1000;
	return timeslot_create(&rt.ts, &mrt, &rt, purge_rt_entry);
}

int rt_srclist_entry_create(aodv_rt_srclist_entry_t** srclist_entry_out, u_int8_t shost_ether[ETH_ALEN],
		u_int8_t shost_prev_hop[ETH_ALEN], const dessert_meshif_t* output_iface, u_int32_t source_seq_num) {
	aodv_rt_srclist_entry_t* srclist_entry = malloc(sizeof(aodv_rt_srclist_entry_t));
	if (srclist_entry == NULL) return FALSE;
	memcpy(srclist_entry->shost_ether, shost_ether, ETH_ALEN);
	memcpy(srclist_entry->shost_prev_hop, shost_prev_hop, ETH_ALEN);
	srclist_entry->output_iface = output_iface;
	srclist_entry->shost_seq_num = source_seq_num;
	*srclist_entry_out = srclist_entry;
	return TRUE;
}

int rt_entry_create(aodv_rt_entry_t** rreqt_entry_out, u_int8_t dhost_ether[ETH_ALEN]) {
	aodv_rt_entry_t* rt_entry = malloc(sizeof(aodv_rt_entry_t));
	if (rt_entry == NULL) return FALSE;

	memcpy(rt_entry->dhost_ether, dhost_ether, ETH_ALEN);
	rt_entry->flags = AODV_FLAGS_NEXT_HOP_UNKNOWN | AODV_FLAGS_ROUTE_INVALID;
	rt_entry->src_list = NULL;

	*rreqt_entry_out = rt_entry;
	return TRUE;
}

int nht_destlist_entry_create (nht_destlist_entry_t** entry_out, u_int8_t dhost_ether[ETH_ALEN], aodv_rt_entry_t* rt_entry) {
	nht_destlist_entry_t* entry = malloc(sizeof(nht_destlist_entry_t));
	if (entry == NULL) return FALSE;
	memcpy(entry->dhost_ether, dhost_ether, ETH_ALEN);
	entry->rt_entry = rt_entry;
	*entry_out = entry;
	return TRUE;
}

int nht_entry_create (nht_entry_t** entry_out, u_int8_t dhost_next_hop[ETH_ALEN]) {
	nht_entry_t* entry = malloc(sizeof(nht_entry_t));
	if (entry == NULL) return FALSE;
	memcpy(entry->dhost_next_hop, dhost_next_hop, ETH_ALEN);
	entry->dest_list = NULL;
	*entry_out = entry;
	return TRUE;
}

int aodv_db_rt_capt_rreq (u_int8_t dhost_ether[ETH_ALEN], u_int8_t shost_ether[ETH_ALEN],
		u_int8_t shost_prev_hop[ETH_ALEN], const dessert_meshif_t* output_iface,
		u_int32_t shost_seq_num, struct timeval* timestamp) {
	aodv_rt_entry_t* rt_entry;
	aodv_rt_srclist_entry_t* srclist_entry;

	// find rreqt_entry with dhost_ether address
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL) {
		// if not found -> create routing entry
		if (rt_entry_create(&rt_entry, dhost_ether) == FALSE) {
			return FALSE;
		}
		HASH_ADD_KEYPTR(hh, rt.entrys, rt_entry->dhost_ether, ETH_ALEN, rt_entry);
	}
	// find srclist_entry with shost_ether address
	HASH_FIND(hh, rt_entry->src_list, shost_ether, ETH_ALEN, srclist_entry);
	if (srclist_entry == NULL) {
		// if not found -> create new source entry of source list
		if (rt_srclist_entry_create(&srclist_entry, shost_ether, shost_prev_hop,
				output_iface, shost_seq_num) != TRUE) {
			return FALSE;
		}
		HASH_ADD_KEYPTR(hh, rt_entry->src_list, srclist_entry->shost_ether, ETH_ALEN, srclist_entry);
		timeslot_addobject(rt.ts, timestamp, rt_entry);
		return TRUE;
	} else if (hf_seq_comp_i_j(srclist_entry->shost_seq_num, shost_seq_num) < 0) {
		// overwrite several fields of source entry if source seq_num is newer
		memcpy(srclist_entry->shost_prev_hop, shost_prev_hop, ETH_ALEN);
		srclist_entry->output_iface = output_iface;
		srclist_entry->shost_seq_num = shost_seq_num;
		timeslot_addobject(rt.ts, timestamp, rt_entry);
		return TRUE;
	}
	return FALSE;
}

int aodv_db_rt_capt_rrep (u_int8_t dhost_ether[ETH_ALEN], u_int8_t dhost_next_hop[ETH_ALEN],
		const dessert_meshif_t* output_iface, u_int32_t dhost_seq_num, u_int8_t hop_count, struct timeval* timestamp) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL) {
		// if not found -> create routing entry
		if (rt_entry_create(&rt_entry, dhost_ether) == FALSE) {
			return FALSE;
		}
		HASH_ADD_KEYPTR(hh, rt.entrys, rt_entry->dhost_ether, ETH_ALEN, rt_entry);
	}
	if ((rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) ||
			(hf_seq_comp_i_j(dhost_seq_num, rt_entry->dhost_seq_num) > 0) ||
			((dhost_seq_num == rt_entry->dhost_seq_num) && (hop_count < rt_entry->hop_count))) {

		nht_entry_t* nht_entry;
		nht_destlist_entry_t* destlist_entry;
		// remove old next_hop_entry if found
		if (!(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)) {
			HASH_FIND(hh, nht, rt_entry->dhost_next_hop, ETH_ALEN, nht_entry);
			if (nht_entry != NULL) {
				HASH_FIND(hh, nht_entry->dest_list, rt_entry->dhost_ether, ETH_ALEN, destlist_entry);
				if (destlist_entry != NULL) {
					HASH_DEL(nht_entry->dest_list, destlist_entry);
					free(destlist_entry);
				}
				if (nht_entry->dest_list == NULL) {
					HASH_DEL(nht, nht_entry);
					free(nht_entry);
				}
			}
		}

		// set next hop and etc. towards this destination
		memcpy(rt_entry->dhost_next_hop, dhost_next_hop, ETH_ALEN);
		rt_entry->output_iface = output_iface;
		rt_entry->dhost_seq_num = dhost_seq_num;
		rt_entry->hop_count = hop_count;
		rt_entry->flags &= ~AODV_FLAGS_NEXT_HOP_UNKNOWN;
		rt_entry->flags &= ~AODV_FLAGS_ROUTE_INVALID;

		// map also this routing entry to next hop
		HASH_FIND(hh, nht, dhost_next_hop, ETH_ALEN, nht_entry);
		if (nht_entry == NULL) {
			if (nht_entry_create(&nht_entry, dhost_next_hop) == FALSE) return FALSE;
			HASH_ADD_KEYPTR(hh, nht, nht_entry->dhost_next_hop, ETH_ALEN, nht_entry);
		}
		HASH_FIND(hh, nht_entry->dest_list, dhost_ether, ETH_ALEN, destlist_entry);
		if (destlist_entry == NULL) {
			if (nht_destlist_entry_create(&destlist_entry, dhost_ether, rt_entry) == FALSE) return FALSE;
			HASH_ADD_KEYPTR(hh, nht_entry->dest_list, destlist_entry->dhost_ether, ETH_ALEN, destlist_entry);
		}
		destlist_entry->rt_entry = rt_entry;

		// set/change timestamp of this routing entry
		timeslot_addobject(rt.ts, timestamp, rt_entry);
		return TRUE;
	}
	return FALSE;
}

int aodv_db_rt_getroute2dest(u_int8_t dhost_ether[ETH_ALEN], u_int8_t dhost_next_hop_out[ETH_ALEN],
		const dessert_meshif_t** output_iface_out, struct timeval* timestamp) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN || rt_entry->flags & AODV_FLAGS_ROUTE_INVALID) {
		return FALSE;
	}
	memcpy(dhost_next_hop_out, rt_entry->dhost_next_hop, ETH_ALEN);
	*output_iface_out = rt_entry->output_iface;
	timeslot_addobject(rt.ts, timestamp, rt_entry);
	return TRUE;
}

int aodv_db_rt_getnexthop(u_int8_t dhost_ether[ETH_ALEN], u_int8_t dhost_next_hop_out[ETH_ALEN]) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)
		return FALSE;
	memcpy(dhost_next_hop_out, rt_entry->dhost_next_hop, ETH_ALEN);
	return TRUE;
}

int aodv_db_rt_getprevhop(u_int8_t dhost_ether[ETH_ALEN], u_int8_t shost_ether[ETH_ALEN],
		u_int8_t shost_next_hop_out[ETH_ALEN], const dessert_meshif_t** output_iface_out) {
	aodv_rt_entry_t* rt_entry;
	aodv_rt_srclist_entry_t* srclist_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL) return FALSE;
	HASH_FIND(hh, rt_entry->src_list, shost_ether, ETH_ALEN, srclist_entry);
	if (srclist_entry == NULL) return FALSE;
	memcpy(shost_next_hop_out, srclist_entry->shost_prev_hop, ETH_ALEN);
	*output_iface_out = srclist_entry->output_iface;
	return TRUE;
}

int aodv_db_rt_getrouteseqnum(u_int8_t dhost_ether[ETH_ALEN], u_int32_t* dhost_seq_num_out) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)
		return FALSE;
	*dhost_seq_num_out = rt_entry->dhost_seq_num;
	return TRUE;
}

int aodv_db_rt_getlastrreqseq(u_int8_t dhost_ether[ETH_ALEN],
		u_int8_t shost_ether[ETH_ALEN], u_int32_t* shost_seq_num_out) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL)
		return FALSE;

	aodv_rt_srclist_entry_t* src_entry;
	HASH_FIND(hh, rt_entry->src_list, shost_ether, ETH_ALEN, src_entry);
	if (src_entry == NULL)
		return FALSE;
	*shost_seq_num_out = src_entry->shost_seq_num;
	return TRUE;
}

int aodv_db_rt_markrouteinv(u_int8_t dhost_ether[ETH_ALEN]) {
	aodv_rt_entry_t* rt_entry;
	HASH_FIND(hh, rt.entrys, dhost_ether, ETH_ALEN, rt_entry);
	if (rt_entry == NULL) return FALSE;
	rt_entry->flags |= AODV_FLAGS_ROUTE_INVALID;
	return TRUE;
}

int aodv_db_rt_inv_route(u_int8_t dhost_next_hop[ETH_ALEN], u_int8_t dhost_ether_out[ETH_ALEN]) {
	// find appropriate routing entry
	nht_entry_t* nht_entry;
	nht_destlist_entry_t* nht_dest_entry;
	HASH_FIND(hh, nht, dhost_next_hop, ETH_ALEN, nht_entry);
	if ((nht_entry == NULL) || (nht_entry->dest_list == NULL)) return FALSE;
	nht_dest_entry = nht_entry->dest_list;

	// mark route as invalid and give this destination address back
	nht_dest_entry->rt_entry->flags |= AODV_FLAGS_ROUTE_INVALID;
	memcpy(dhost_ether_out, nht_dest_entry->rt_entry->dhost_ether, ETH_ALEN);

	// cleanup next hop table
	HASH_DEL(nht_entry->dest_list, nht_dest_entry);
	free(nht_dest_entry);
	if (nht_entry->dest_list == NULL) {
		HASH_DEL(nht, nht_entry);
		free(nht_entry);
	}
	return TRUE;
}

int aodv_db_rt_cleanup (struct timeval* timestamp) {
	return timeslot_purgeobjects(rt.ts, timestamp);
}

int aodv_db_rt_report(char** str_out) {
	aodv_rt_entry_t* current_entry = rt.entrys;
	char* output;
	char entry_str[REPORT_RT_STR_LEN  + 1];

	// compute str length
	uint len = 0;
	while(current_entry != NULL) {
		len += REPORT_RT_STR_LEN * 2;
		current_entry = current_entry->hh.next;
	}
	current_entry = rt.entrys;
	output = malloc(sizeof (char) * REPORT_RT_STR_LEN * (3 + len) + 1);
	if (output == NULL) return FALSE;

	// initialize first byte to \0 to mark output as empty
	*output = '\0';
	strcat(output, "+-------------------+---------------+-----------+-------------------+-------------------+\n");
	strcat(output, "|  destination addr | next hop unkn | route inv |    out iface addr |          next hop |\n");
	strcat(output, "+-------------------+---------------+-----------+-------------------+-------------------+\n");
	while(current_entry != NULL) {
		// first line for best output interface
		if (current_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
			snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| %02x:%02x:%02x:%02x:%02x:%02x |          TRUE |     %5s |                   |                   |\n",
					current_entry->dhost_ether[0], current_entry->dhost_ether[1],
					current_entry->dhost_ether[2], current_entry->dhost_ether[3],
					current_entry->dhost_ether[4], current_entry->dhost_ether[5],
					(current_entry->flags & AODV_FLAGS_ROUTE_INVALID)? "TRUE" : "FALSE");
		} else {
			snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| %02x:%02x:%02x:%02x:%02x:%02x |         FALSE |     %5s | %02x:%02x:%02x:%02x:%02x:%02x | %02x:%02x:%02x:%02x:%02x:%02x |\n",
					current_entry->dhost_ether[0], current_entry->dhost_ether[1],
					current_entry->dhost_ether[2], current_entry->dhost_ether[3],
					current_entry->dhost_ether[4], current_entry->dhost_ether[5],
					(current_entry->flags & AODV_FLAGS_ROUTE_INVALID)? "TRUE" : "FALSE",
					current_entry->output_iface->hwaddr[0], current_entry->output_iface->hwaddr[1],
					current_entry->output_iface->hwaddr[2], current_entry->output_iface->hwaddr[3],
					current_entry->output_iface->hwaddr[4], current_entry->output_iface->hwaddr[5],
					current_entry->dhost_next_hop[0], current_entry->dhost_next_hop[1],
					current_entry->dhost_next_hop[2], current_entry->dhost_next_hop[3],
					current_entry->dhost_next_hop[4], current_entry->dhost_next_hop[5]);
		}
		strcat(output, entry_str);
		strcat(output, "+-------------------+---------------+-----------+-------------------+-------------------+\n");
		current_entry = current_entry->hh.next;
	}
	*str_out = output;
	return TRUE;
}


