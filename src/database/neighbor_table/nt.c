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

#include "nt.h"
#include "../timeslot.h"
#include "../../config.h"
#include "../schedule_table/aodv_st.h"

typedef struct neighbor_entry {
	struct __attribute__ ((__packed__)) { // key
		u_int8_t 				ether_neighbor[ETH_ALEN];
		const dessert_meshif_t*	iface;
	};
	UT_hash_handle				hh;
} neighbor_entry_t;

typedef struct neighbor_table {
	neighbor_entry_t* 		entrys;
	timeslot_t*				ts;
} neighbor_table_t;

neighbor_table_t nt;

neighbor_entry_t* db_neighbor_entry_create(u_int8_t ether_neighbor_addr[ETH_ALEN],
		const dessert_meshif_t* iface) {
	neighbor_entry_t* new_entry;
	new_entry = malloc(sizeof(neighbor_entry_t));
	if (new_entry == NULL) return NULL;
	memcpy(new_entry->ether_neighbor, ether_neighbor_addr, ETH_ALEN);
	new_entry->iface = iface;
	return new_entry;
}

void db_nt_on_neigbor_timeout(struct timeval* timestamp, void* src_object, void* object) {
	neighbor_entry_t* curr_entry = object;
	dessert_debug("%s <= x => %M", curr_entry->iface->if_name, curr_entry->ether_neighbor);
	HASH_DEL(nt.entrys, curr_entry);
	// add schedule
	struct timeval curr_time;
	gettimeofday(&curr_time, NULL);
	aodv_db_sc_addschedule(&curr_time, curr_entry->ether_neighbor, AODV_SC_SEND_OUT_RERR, 0);
	free(curr_entry);
}

int db_nt_init() {
	timeslot_t* new_ts;
	struct timeval timeout;
	u_int32_t hello_int_msek = HELLO_INTERVAL  * (ALLOWED_HELLO_LOST +1);
	timeout.tv_sec = hello_int_msek / 1000;
	timeout.tv_usec = (hello_int_msek % 1000) * 1000;
	if (timeslot_create(&new_ts, &timeout, NULL, db_nt_on_neigbor_timeout) != TRUE)
		return FALSE;
	nt.entrys = NULL;
	nt.ts = new_ts;
	return TRUE;
}

int db_nt_cap2Dneigh(u_int8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* iface, struct timeval* timestamp) {
	neighbor_entry_t* curr_entry = NULL;
	u_int8_t addr_sum[ETH_ALEN + sizeof(void*)];
	memcpy(addr_sum, ether_neighbor_addr, ETH_ALEN);
	memcpy(addr_sum + ETH_ALEN, &iface, sizeof(void*));
	HASH_FIND(hh, nt.entrys, addr_sum, ETH_ALEN + sizeof(void*), curr_entry);
	if (curr_entry == NULL) {
		curr_entry = db_neighbor_entry_create(ether_neighbor_addr, iface);
		if (curr_entry == NULL) return FALSE;
		HASH_ADD_KEYPTR(hh, nt.entrys, curr_entry->ether_neighbor, ETH_ALEN + sizeof(void*), curr_entry);
		dessert_debug("%s <=====> %M", iface->if_name, ether_neighbor_addr);
	}
	timeslot_addobject(nt.ts, timestamp, curr_entry);
	return TRUE;
}

int db_nt_check2Dneigh(u_int8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* iface, struct timeval* timestamp) {
	timeslot_purgeobjects(nt.ts, timestamp);
	neighbor_entry_t* curr_entry;
	u_int8_t addr_sum[ETH_ALEN + sizeof(void*)];
	memcpy(addr_sum, ether_neighbor_addr, ETH_ALEN);
	memcpy(addr_sum + ETH_ALEN, &iface, sizeof(void*));
	HASH_FIND(hh, nt.entrys, addr_sum, ETH_ALEN + sizeof(void*), curr_entry);
	if (curr_entry == NULL) {
		return FALSE;
	}
	return TRUE;
}

int db_nt_cleanup(struct timeval* timestamp) {
	return timeslot_purgeobjects(nt.ts, timestamp);
}
