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

#include <uthash.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_seq.h"

typedef struct data_packet_id {
	u_int8_t src_dest_addr[ETH_ALEN * 2]; // key
	u_int32_t seq_num;
	UT_hash_handle hh;
} data_packet_id_t;

data_packet_id_t* entrys = NULL;

u_int32_t data_get_nextseq(u_int8_t src_addr[ETH_ALEN], u_int8_t dest_addr[ETH_ALEN]) {
	u_int8_t key[ETH_ALEN * 2];
	memcpy(key, src_addr, ETH_ALEN);
	memcpy(key + ETH_ALEN, dest_addr, ETH_ALEN);
	data_packet_id_t* entry;
	HASH_FIND(hh, entrys, key, ETH_ALEN * 2, entry);
	if (entry == NULL) {
		entry = malloc(sizeof(data_packet_id_t));
		if (entry == NULL) return 0;
		memcpy(entry->src_dest_addr, key, ETH_ALEN * 2);
		entry->seq_num = 0;
		HASH_ADD_KEYPTR(hh, entrys, entry->src_dest_addr, ETH_ALEN * 2, entry);
	}
	entry->seq_num++;
	return entry->seq_num;
}

void data_set_seq(u_int8_t src_addr[ETH_ALEN], u_int8_t dest_addr[ETH_ALEN], u_int32_t seq_num) {
	u_int8_t key[ETH_ALEN * 2];
	memcpy(key, src_addr, ETH_ALEN);
	memcpy(key + ETH_ALEN, dest_addr, ETH_ALEN);
	data_packet_id_t* entry = NULL;
	HASH_FIND(hh, entrys, key, ETH_ALEN * 2, entry);
	if (entry == NULL) {
		entry = malloc(sizeof(data_packet_id_t));
		if (entry == NULL) return;
		memcpy(entry->src_dest_addr, key, ETH_ALEN * 2);
		HASH_ADD_KEYPTR(hh, entrys, entry->src_dest_addr, ETH_ALEN * 2, entry);
		entry->seq_num = seq_num;
	}
	if ((entry->seq_num - seq_num > (1<<30)) || (entry->seq_num < seq_num)) {
		entry->seq_num = seq_num;
	}
}
