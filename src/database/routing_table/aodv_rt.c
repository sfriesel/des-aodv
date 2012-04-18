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
#include "../neighbor_table/nt.h"

#undef assert
#include <assert.h>

typedef struct aodv_rt_precursor_list_entry {
    mac_addr            addr; // ID
    dessert_meshif_t*   iface;
    UT_hash_handle      hh;
} aodv_rt_precursor_list_entry_t;

typedef struct aodv_rt_entry {
    mac_addr            addr; // ID
    mac_addr            next_hop;
    dessert_meshif_t*	output_iface;
    uint32_t            sequence_number;
    metric_t			metric;
    uint8_t				hop_count;
    /**
     * flags format: 0 0 0 0 0 0 U I
     * I - Invalid flag; route is invalid due of link breakage
     * U - next hop Unknown flag;
     */
    uint8_t				flags;
    aodv_rt_precursor_list_entry_t* precursor_list;
    UT_hash_handle		hh;
} aodv_rt_entry_t;


typedef struct aodv_rt {
    aodv_rt_entry_t*	entries;
    timeslot_t*			timeslot;
} aodv_rt_t;

/**
 * Mapping next_hop -> destination list
 */
typedef struct nht_destlist_entry {
    uint8_t				destination_host[ETH_ALEN];
    aodv_rt_entry_t*	rt_entry;
    UT_hash_handle		hh;
} nht_destlist_entry_t;

typedef struct nht_entry {
    uint8_t				destination_host_next_hop[ETH_ALEN];
    nht_destlist_entry_t*		dest_list;
    UT_hash_handle			hh;
} nht_entry_t;

aodv_rt_t				rt;
nht_entry_t*				nht = NULL;

void purge_rt_entry(struct timeval* timestamp, void* src_object, void* del_object) {
    aodv_rt_entry_t* rt_entry = del_object;

    // delete precursor list from routing entry
    while(rt_entry->precursor_list) {
        aodv_rt_precursor_list_entry_t* precursor = rt_entry->precursor_list;
        HASH_DEL(rt_entry->precursor_list, precursor);
        free(precursor);
    }

    if(!(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)) {
        // delete mapping from next hop to this entry
        nht_entry_t* nht_entry;
        nht_destlist_entry_t* dest_entry;
        HASH_FIND(hh, nht, rt_entry->next_hop, ETH_ALEN, nht_entry);

        if(nht_entry != NULL) {
            HASH_FIND(hh, nht_entry->dest_list, rt_entry->addr, ETH_ALEN, dest_entry);

            if(dest_entry != NULL) {
                HASH_DEL(nht_entry->dest_list, dest_entry);
                free(dest_entry);
            }

            if(nht_entry->dest_list == NULL) {
                HASH_DEL(nht, nht_entry);
                free(nht_entry);
            }
        }
    }

    // delete routing entry
    dessert_debug("delete route to " MAC, EXPLODE_ARRAY6(rt_entry->addr));
    HASH_DEL(rt.entries, rt_entry);
    free(rt_entry);
}

int aodv_db_rt_init() {
    rt.entries = NULL;

    struct timeval	mrt; // my route timeout
    mrt.tv_sec = MY_ROUTE_TIMEOUT / 1000;
    mrt.tv_usec = (MY_ROUTE_TIMEOUT % 1000) * 1000;
    rt.timeslot = timeslot_create(&mrt, &rt, purge_rt_entry);
    return true;
}

int rt_entry_create(aodv_rt_entry_t** rreqt_entry_out, mac_addr destination_host, struct timeval const *timestamp) {

    aodv_rt_entry_t* rt_entry = malloc(sizeof(aodv_rt_entry_t));

    if(rt_entry == NULL) {
        return false;
    }

    memset(rt_entry, 0x0, sizeof(aodv_rt_entry_t));
    mac_copy(rt_entry->addr, destination_host);
    rt_entry->flags = AODV_FLAGS_NEXT_HOP_UNKNOWN | AODV_FLAGS_ROUTE_INVALID;
    rt_entry->precursor_list = NULL;
    rt_entry->sequence_number = 0; //we know nothing about the destination
    rt_entry->metric = AODV_MAX_METRIC; //initial
    rt_entry->hop_count = UINT8_MAX; //initial

    timeslot_addobject(rt.timeslot, timestamp, rt_entry);

    *rreqt_entry_out = rt_entry;
    return true;
}

int nht_destlist_entry_create(nht_destlist_entry_t** entry_out, mac_addr destination_host, aodv_rt_entry_t* rt_entry) {
    nht_destlist_entry_t* entry = malloc(sizeof(nht_destlist_entry_t));

    if(entry == NULL) {
        return false;
    }

    memset(entry, 0x0, sizeof(nht_destlist_entry_t));
    mac_copy(entry->destination_host, destination_host);
    entry->rt_entry = rt_entry;

    *entry_out = entry;
    return true;
}

int nht_entry_create(nht_entry_t** entry_out, mac_addr destination_host_next_hop) {
    nht_entry_t* entry = malloc(sizeof(nht_entry_t));

    if(entry == NULL) {
        return false;
    }

    memset(entry, 0x0, sizeof(nht_entry_t));
    mac_copy(entry->destination_host_next_hop, destination_host_next_hop);
    entry->dest_list = NULL;

    *entry_out = entry;
    return true;
}

/** update db according to data in rreq
 *  @return false if an error occured, true otherwise
 *  @param result_out result of capture
 */
int aodv_db_rt_capt_rreq(mac_addr destination_host,
                         mac_addr originator_host,
                         mac_addr prev_hop,
                         dessert_meshif_t* iface,
                         uint32_t originator_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval const *timestamp,
                         aodv_capt_result_t* result_out) {

    aodv_rt_entry_t* dest_entry;
    aodv_rt_entry_t* orig_entry;

    // find rt_entry with dhost_ether address
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, dest_entry);

    if(!dest_entry) {
        // if not found -> create routing entry
        if(!rt_entry_create(&dest_entry, destination_host, timestamp)) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt.entries, dest_entry->addr, ETH_ALEN, dest_entry);
    }

    // find rt_entry with shost_ether address
    HASH_FIND(hh, rt.entries, originator_host, ETH_ALEN, orig_entry);

    if(!orig_entry) {
        // if not found -> create routing entry
        if(!rt_entry_create(&orig_entry, originator_host, timestamp)) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt.entries, orig_entry->addr, ETH_ALEN, orig_entry);
    }

    int seq_num_cmp = hf_comp_u32(orig_entry->sequence_number, originator_sequence_number);
    bool newer = seq_num_cmp < 0;
    bool same_seq_num = (seq_num_cmp == 0);
    bool metric_hit = hf_comp_metric(orig_entry->metric, metric) < 0;

    if(newer || (same_seq_num && metric_hit)) {

        if(same_seq_num && metric_hit) {
            *result_out = AODV_CAPT_METRIC_HIT;
        }
        else {
            *result_out = AODV_CAPT_NEW;
        }

        mac_copy(orig_entry->next_hop, prev_hop);
        orig_entry->output_iface = iface;
        orig_entry->sequence_number = originator_sequence_number;
        orig_entry->metric = metric;
        orig_entry->hop_count = hop_count;
    }
    else {
        *result_out = AODV_CAPT_OLD;
    }
    return true;
}

/** update db according to data in rrep
 *  @return false if an error occured or rrep was not used, true otherwise
 */
int aodv_db_rt_capt_rrep(mac_addr destination_host,
                         mac_addr destination_host_next_hop,
                         dessert_meshif_t* output_iface,
                         uint32_t destination_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval const *timestamp) {

    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // if not found -> create routing entry
        if(!rt_entry_create(&rt_entry, destination_host, timestamp)) {
            return false;
        }
        HASH_ADD_KEYPTR(hh, rt.entries, rt_entry->addr, ETH_ALEN, rt_entry);
    }

    int next_hop_known = !(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN);
    bool old = hf_comp_u32(rt_entry->sequence_number, destination_sequence_number) > 0;

    if(next_hop_known && old) {
        return false;
    }

    nht_entry_t* nht_entry;
    nht_destlist_entry_t* destlist_entry;

    // remove old next_hop_entry if found
    if(next_hop_known) {
        HASH_FIND(hh, nht, rt_entry->next_hop, ETH_ALEN, nht_entry);

        if(nht_entry != NULL) {
            HASH_FIND(hh, nht_entry->dest_list, rt_entry->addr, ETH_ALEN, destlist_entry);

            if(destlist_entry != NULL) {
                HASH_DEL(nht_entry->dest_list, destlist_entry);
                free(destlist_entry);
            }

            if(nht_entry->dest_list == NULL) {
                HASH_DEL(nht, nht_entry);
                free(nht_entry);
            }
        }
    }

    // set next hop and etc. towards this destination
    mac_copy(rt_entry->next_hop, destination_host_next_hop);
    rt_entry->output_iface = output_iface;
    rt_entry->sequence_number = destination_sequence_number;
    rt_entry->metric = metric;
    rt_entry->hop_count = hop_count;
    rt_entry->flags &= ~AODV_FLAGS_NEXT_HOP_UNKNOWN;
    rt_entry->flags &= ~AODV_FLAGS_ROUTE_INVALID;

    // insert this routing entry in the next hop destlist
    HASH_FIND(hh, nht, destination_host_next_hop, ETH_ALEN, nht_entry);

    if(nht_entry == NULL) {
        int success = nht_entry_create(&nht_entry, destination_host_next_hop);
        assert(success);
        HASH_ADD_KEYPTR(hh, nht, nht_entry->destination_host_next_hop, ETH_ALEN, nht_entry);
    }

    HASH_FIND(hh, nht_entry->dest_list, destination_host, ETH_ALEN, destlist_entry);

    if(destlist_entry == NULL) {
        int success = nht_destlist_entry_create(&destlist_entry, destination_host, rt_entry);
        assert(success);

        HASH_ADD_KEYPTR(hh, nht_entry->dest_list, destlist_entry->destination_host, ETH_ALEN, destlist_entry);
    }

    destlist_entry->rt_entry = rt_entry;

    return true;
}

int aodv_db_rt_getroute2dest(mac_addr destination_host, mac_addr *next_hop_out,
                             dessert_meshif_t** output_iface_out, struct timeval const *timestamp) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN || rt_entry->flags & AODV_FLAGS_ROUTE_INVALID) {
        dessert_debug("route to " MAC " is invalid", EXPLODE_ARRAY6(destination_host));
        return false;
    }

    mac_copy(*next_hop_out, rt_entry->next_hop);
    *output_iface_out = rt_entry->output_iface;
    timeslot_addobject(rt.timeslot, timestamp, rt_entry);
    return true;
}

int aodv_db_rt_getnexthop(mac_addr destination_host, mac_addr *next_hop_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        return false;
    }

    mac_copy(*next_hop_out, rt_entry->next_hop);
    return true;
}

// returns true if dest is known
//         false if des is unknown
int aodv_db_rt_get_destination_sequence_number(mac_addr dhost_ether, uint32_t* destination_sequence_number_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, dhost_ether, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        *destination_sequence_number_out = 0;
        return false;
    }

    *destination_sequence_number_out = rt_entry->sequence_number;
    return true;
}

int aodv_db_rt_get_hopcount(mac_addr destination_host, uint8_t* hop_count_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        *hop_count_out = UINT8_MAX;
        return false;
    }
    *hop_count_out =  rt_entry->hop_count;
    return true;
}

int aodv_db_rt_get_metric(mac_addr destination_host, metric_t* last_metric_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        *last_metric_out = AODV_MAX_METRIC;
        return false;
    }
    *last_metric_out =  rt_entry->metric;
    return true;
}

int aodv_db_rt_markrouteinv(mac_addr destination_host, uint32_t destination_sequence_number) {
    aodv_rt_entry_t* destination;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, destination);

    if(!destination) {
        return false;
    }

    if(hf_comp_u32(destination->sequence_number, destination_sequence_number) > 0) {
        dessert_debug("route to " MAC " seq=%" PRIu32 ":%" PRIu32 " NOT marked as invalid", EXPLODE_ARRAY6(destination_host), destination->sequence_number, destination_sequence_number);
        return false;
    }

    dessert_debug("route to " MAC " seq=%" PRIu32 ":%" PRIu32 " marked as invalid", EXPLODE_ARRAY6(destination_host), destination->sequence_number, destination_sequence_number);
    destination->flags |= AODV_FLAGS_ROUTE_INVALID;
    return true;
}

int aodv_db_rt_get_destlist(mac_addr dhost_next_hop, aodv_link_break_element_t** destlist) {
    // find appropriate routing entry
    nht_entry_t* nht_entry;
    HASH_FIND(hh, nht, dhost_next_hop, ETH_ALEN, nht_entry);

    if(nht_entry == NULL) {
        return false;
    }

    struct nht_destlist_entry* dest, *tmp;

    HASH_ITER(hh, nht_entry->dest_list, dest, tmp) {
        aodv_link_break_element_t* el = malloc(sizeof(aodv_link_break_element_t));
        mac_copy(el->host, dest->rt_entry->addr);
        el->sequence_number = dest->rt_entry->sequence_number;
        dessert_trace("create ERR: " MAC " seq=%" PRIu32 "", EXPLODE_ARRAY6(el->host), el->sequence_number);
        DL_APPEND(*destlist, el);
    }
    return true;
}

int aodv_db_rt_add_precursor(mac_addr destination_addr, mac_addr precursor_addr, dessert_meshif_t *iface) {
    aodv_rt_entry_t* destination;
    HASH_FIND(hh, rt.entries, destination_addr, ETH_ALEN, destination);

    if(!destination) {
        return false;
    }

    aodv_rt_precursor_list_entry_t *precursor;
    HASH_FIND(hh, destination->precursor_list, precursor_addr, ETH_ALEN, precursor);

    if(precursor) {
        return false;
    }

    precursor = malloc(sizeof(*precursor));
    mac_copy(precursor->addr, precursor_addr);
    precursor->iface = iface;

    HASH_ADD_KEYPTR(hh, destination->precursor_list, precursor_addr, ETH_ALEN, precursor);
    return true;
}

int aodv_db_rt_inv_over_nexthop(mac_addr next_hop) {
    // mark route as invalid and give this destination address back
    nht_entry_t* nht_entry;
    HASH_FIND(hh, nht, next_hop, ETH_ALEN, nht_entry);

    if(nht_entry == NULL) {
        return false;
    }

    struct nht_destlist_entry* dest, *tmp;

    HASH_ITER(hh, nht_entry->dest_list, dest, tmp) {
        dest->rt_entry->flags |= AODV_FLAGS_ROUTE_INVALID;
    }

    return true;
}

int aodv_db_rt_routing_reset(uint32_t* count_out) {

    *count_out = 0;

    aodv_rt_entry_t* dest = NULL;
    aodv_rt_entry_t* tmp = NULL;
    HASH_ITER(hh, rt.entries, dest, tmp) {
        dest->flags |= AODV_FLAGS_ROUTE_INVALID;
        dessert_debug("routing table reset: " MAC " is now invalid!", EXPLODE_ARRAY6(dest->addr));
        (*count_out)++;
    }
    return true;
}

int aodv_db_rt_cleanup(struct timeval* timestamp) {
    timeslot_purgeobjects(rt.timeslot, timestamp);
    return true;
}

int aodv_db_rt_report(char **str_out) {
    char entry_str[150];

    uint32_t len = HASH_COUNT(rt.entries);

    *str_out = malloc(sizeof(entry_str) * (3 + 2 * len) + 1);

    if(!*str_out) {
        return false;
    }

    strcpy(*str_out, "+-------------------+-------------------+-------------------+-------------+---------------+\n"
                     "|    destination    |      next hop     |  out iface addr   |  route inv  | next hop unkn |\n"
                     "+-------------------+-------------------+-------------------+-------------+---------------+\n");

    for(aodv_rt_entry_t *entry = rt.entries; entry; entry = entry->hh.next) {
        if(entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
            snprintf(entry_str, sizeof(entry_str), "| " MAC " |                   |                   |   %5s     |     true      |\n",
                     EXPLODE_ARRAY6(entry->addr),
                     (entry->flags & AODV_FLAGS_ROUTE_INVALID) ? "true" : "false");
        }
        else {
            snprintf(entry_str, sizeof(entry_str), "| " MAC " | " MAC " | " MAC " |    %5s    |     false     |\n",
                     EXPLODE_ARRAY6(entry->addr),
                     EXPLODE_ARRAY6(entry->next_hop),
                     EXPLODE_ARRAY6(entry->output_iface->hwaddr),
                     (entry->flags & AODV_FLAGS_ROUTE_INVALID) ? "true" : "false");
        }

        strcat(*str_out, entry_str);
        strcat(*str_out, "+-------------------+-------------------+-------------------+-------------+---------------+\n");
    }
    return true;
}
