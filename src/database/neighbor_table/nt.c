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
#undef assert
#include <assert.h>

typedef struct neighbor_entry {
    struct __attribute__((__packed__)) {  // key
        uint8_t             ether_neighbor[ETH_ALEN];
        dessert_meshif_t*   iface;
        uint16_t            last_hello_seq;
    };
    UT_hash_handle          hh;
} neighbor_entry_t;

neighbor_entry_t* db_neighbor_entry_create(mac_addr ether_neighbor_addr, dessert_meshif_t* iface) {
    neighbor_entry_t* new_entry;
    new_entry = malloc(sizeof(neighbor_entry_t));

    if(new_entry == NULL) {
        return NULL;
    }

    mac_copy(new_entry->ether_neighbor, ether_neighbor_addr);
    new_entry->iface = iface;
    new_entry->last_hello_seq = 0; /* initial */
    return new_entry;
}

struct neighbor {
    mac_addr          addr;
    dessert_meshif_t *iface;
    nt_neighbor_t       *prev, *next;
};

struct neighbor_table {
    neighbor_entry_t  *entries;
    timeslot_t  *ts;
} nt;

static void db_nt_on_neighbor_timeout(struct timeval* timestamp, void* src_object, void* object) {
    neighbor_entry_t* curr_entry = object;
    dessert_debug("%s <= x => " MAC, curr_entry->iface->if_name, EXPLODE_ARRAY6(curr_entry->ether_neighbor));
    HASH_DEL(nt.entries, curr_entry);

    aodv_db_sc_addschedule(timestamp, curr_entry->ether_neighbor, AODV_SC_SEND_OUT_RERR, 0);
    free(curr_entry);
}

int aodv_db_nt_init() {
    struct timeval timeout;
    dessert_ms2timeval(hello_interval * (ALLOWED_HELLO_LOST + 1), &timeout);

    nt.ts = timeslot_create(&timeout, NULL, db_nt_on_neighbor_timeout);
    assert(nt.ts);

    nt.entries = NULL;
    return true;
}

int aodv_db_nt_destroy(uint32_t* count_out) {
    *count_out = 0;

    neighbor_entry_t* neigh = NULL;
    neighbor_entry_t* tmp = NULL;
    HASH_ITER(hh, nt.entries, neigh, tmp) {
        HASH_DEL(nt.entries, neigh);
        free(neigh);
        (*count_out)++;
    }
    timeslot_destroy(nt.ts);
    return true;
}

int aodv_db_nt_reset(uint32_t* count_out) {
    aodv_db_nt_destroy(count_out);
    aodv_db_nt_init();
    return true;
}

int aodv_db_nt_capt_hellorsp(mac_addr addr, uint16_t hello_seq __attribute__((unused)), dessert_meshif_t* iface, struct timeval const *timestamp) {
    neighbor_entry_t* curr_entry = NULL;
    uint8_t addr_sum[ETH_ALEN + sizeof(void*)];
    mac_copy(addr_sum, ether_neighbor_addr);
    memcpy(addr_sum + ETH_ALEN, &iface, sizeof(void*));
    HASH_FIND(hh, nt.entries, addr_sum, ETH_ALEN + sizeof(void*), curr_entry);

    if(curr_entry == NULL) {
        //this neigbor is new, so create an entry
        curr_entry = db_neighbor_entry_create(ether_neighbor_addr, iface);

        if(curr_entry == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, nt.entries, curr_entry->ether_neighbor, ETH_ALEN + sizeof(void*), curr_entry);
        dessert_debug("%s <=====> " MAC, iface->if_name, EXPLODE_ARRAY6(ether_neighbor_addr));
    }

    curr_entry->last_hello_seq = hello_seq;


    timeslot_addobject(nt.ts, timestamp, curr_entry);
    return true;
}

int db_nt_check2Dneigh(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp) {
    timeslot_purgeobjects(nt.ts, timestamp);
    neighbor_entry_t* curr_entry;
    uint8_t addr_sum[ETH_ALEN + sizeof(void*)];
    mac_copy(addr_sum, ether_neighbor_addr);
    memcpy(addr_sum + ETH_ALEN, &iface, sizeof(void*));
    HASH_FIND(hh, nt.entries, addr_sum, ETH_ALEN + sizeof(void*), curr_entry);

    if(curr_entry == NULL) {
        return false;
    }

    return true;
}

void aodv_db_nt_report(char** str_out) {
    timeslot_report(nt.ts, str_out);
}

int aodv_db_nt_cleanup(struct timeval* timestamp) {
    timeslot_purgeobjects(nt.ts, timestamp);
    return true;
}
