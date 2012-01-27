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

aodv_rt_t				rt;
nht_entry_t*				nht = NULL;

void purge_rt_entry(struct timeval* timestamp, void* src_object, void* del_object) {
    aodv_rt_entry_t* rt_entry = del_object;
    aodv_rt_srclist_entry_t* src_list_entry;

    // delete all src_list entries from routing entry
    while(rt_entry->src_list != NULL) {
        src_list_entry = rt_entry->src_list;
        HASH_DEL(rt_entry->src_list, src_list_entry);
        free(src_list_entry);
    }

    if(!(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)) {
        // delete mapping from next hop to this entry
        nht_entry_t* nht_entry;
        nht_destlist_entry_t* dest_entry;
        HASH_FIND(hh, nht, rt_entry->destination_host_next_hop, ETH_ALEN, nht_entry);

        if(nht_entry != NULL) {
            HASH_FIND(hh, nht_entry->dest_list, rt_entry->destination_host, ETH_ALEN, dest_entry);

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
    dessert_debug("delete route to " MAC, EXPLODE_ARRAY6(rt_entry->destination_host));
    HASH_DEL(rt.entries, rt_entry);
    free(rt_entry);
}

int aodv_db_rt_init() {
    rt.entries = NULL;

    struct timeval	mrt; // my route timeout
    mrt.tv_sec = MY_ROUTE_TIMEOUT / 1000;
    mrt.tv_usec = (MY_ROUTE_TIMEOUT % 1000) * 1000;
    return timeslot_create(&rt.ts, &mrt, &rt, purge_rt_entry);
}

int rt_srclist_entry_create(aodv_rt_srclist_entry_t** srclist_entry_out,
                            uint8_t originator_host[ETH_ALEN],
                            uint8_t originator_host_prev_hop[ETH_ALEN],
                            dessert_meshif_t* output_iface) {

    aodv_rt_srclist_entry_t* srclist_entry = malloc(sizeof(aodv_rt_srclist_entry_t));

    if(srclist_entry == NULL) {
        return false;
    }

    memset(srclist_entry, 0x0, sizeof(aodv_rt_srclist_entry_t));
    memcpy(srclist_entry->originator_host, originator_host, ETH_ALEN);
    memcpy(srclist_entry->originator_host_prev_hop, originator_host_prev_hop, ETH_ALEN);
    srclist_entry->output_iface = output_iface;
    srclist_entry->originator_sequence_number = 0; //initial
    srclist_entry->metric = AODV_MAX_METRIC; //initial
    srclist_entry->hop_count = UINT8_MAX; //initial
    srclist_entry->flags = AODV_FLAGS_ROUTE_NEW;

    *srclist_entry_out = srclist_entry;
    return true;
}

int rt_entry_create(aodv_rt_entry_t** rreqt_entry_out, uint8_t destination_host[ETH_ALEN], struct timeval* timestamp) {

    aodv_rt_entry_t* rt_entry = malloc(sizeof(aodv_rt_entry_t));

    if(rt_entry == NULL) {
        return false;
    }

    memset(rt_entry, 0x0, sizeof(aodv_rt_entry_t));
    memcpy(rt_entry->destination_host, destination_host, ETH_ALEN);
    rt_entry->flags = AODV_FLAGS_NEXT_HOP_UNKNOWN | AODV_FLAGS_ROUTE_INVALID;
    rt_entry->src_list = NULL;
    rt_entry->destination_sequence_number = 0; //we know nothing about the destination
    rt_entry->metric = AODV_MAX_METRIC; //initial
    rt_entry->hop_count = UINT8_MAX; //initial

    timeslot_addobject(rt.ts, timestamp, rt_entry);

    *rreqt_entry_out = rt_entry;
    return true;
}

int nht_destlist_entry_create(nht_destlist_entry_t** entry_out, uint8_t destination_host[ETH_ALEN], aodv_rt_entry_t* rt_entry) {
    nht_destlist_entry_t* entry = malloc(sizeof(nht_destlist_entry_t));

    if(entry == NULL) {
        return false;
    }

    memset(entry, 0x0, sizeof(nht_destlist_entry_t));
    memcpy(entry->destination_host, destination_host, ETH_ALEN);
    entry->rt_entry = rt_entry;

    *entry_out = entry;
    return true;
}

int nht_entry_create(nht_entry_t** entry_out, uint8_t destination_host_next_hop[ETH_ALEN]) {
    nht_entry_t* entry = malloc(sizeof(nht_entry_t));

    if(entry == NULL) {
        return false;
    }

    memset(entry, 0x0, sizeof(nht_entry_t));
    memcpy(entry->destination_host_next_hop, destination_host_next_hop, ETH_ALEN);
    entry->dest_list = NULL;

    *entry_out = entry;
    return true;
}

//returns true if input entry is used (newer)
//        false if input entry is unused
int aodv_db_rt_capt_rreq(uint8_t destination_host[ETH_ALEN],
                         uint8_t originator_host[ETH_ALEN],
                         uint8_t originator_host_prev_hop[ETH_ALEN],
                         dessert_meshif_t* output_iface,
                         uint32_t originator_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval* timestamp) {

    aodv_rt_entry_t* rt_entry;
    aodv_rt_srclist_entry_t* srclist_entry;

    // find rreqt_entry with dhost_ether address
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // if not found -> create routing entry
        if(!rt_entry_create(&rt_entry, destination_host, timestamp)) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt.entries, rt_entry->destination_host, ETH_ALEN, rt_entry);
    }

    // find srclist_entry with shost_ether address
    HASH_FIND(hh, rt_entry->src_list, originator_host, ETH_ALEN, srclist_entry);

    if(srclist_entry == NULL) {
        // if not found -> create new source entry of source list
        if(!rt_srclist_entry_create(&srclist_entry, originator_host, originator_host_prev_hop, output_iface)) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt_entry->src_list, srclist_entry->originator_host, ETH_ALEN, srclist_entry);
    }

    int a = hf_comp_u32(srclist_entry->originator_sequence_number, originator_sequence_number);
    int b = hf_comp_metric(srclist_entry->metric, metric); // METRIC

    if(a < 0 || (a == 0 && b > 0)) {

        if(a == 0 && b > 0) {
            //dessert_info("Rcvd RREQ with %" AODV_PRI_METRIC " SEQ known, but process because old metric %" AODV_PRI_METRIC " ", metric, srclist_entry->metric);
            dessert_debug("METRIC HIT: originator_sequence_number=%" PRIu32 ":%" PRIu32 " - metric=%" AODV_PRI_METRIC ":%" AODV_PRI_METRIC "", srclist_entry->originator_sequence_number, originator_sequence_number, srclist_entry->metric, metric);
        }

        dessert_debug("get rreq from " MAC ": originator_sequence_number=%" PRIu32 ":%" PRIu32 " hop_count=%" PRIu8 "",
                      EXPLODE_ARRAY6(originator_host), srclist_entry->originator_sequence_number, originator_sequence_number, hop_count);

        // overwrite several fields of source entry if source seq_num is newer
        memcpy(srclist_entry->originator_host_prev_hop, originator_host_prev_hop, ETH_ALEN);
        srclist_entry->output_iface = output_iface;
        srclist_entry->originator_sequence_number = originator_sequence_number;
        srclist_entry->metric = metric;
        srclist_entry->hop_count = hop_count;
        srclist_entry->flags &= ~AODV_FLAGS_ROUTE_NEW;

        return true;
    }

    dessert_debug("get OLD rreq from " MAC ": originator_sequence_number=%" PRIu32 ":%" PRIu32 " hop_count=%" PRIu8 "",
                  EXPLODE_ARRAY6(originator_host), srclist_entry->originator_sequence_number, originator_sequence_number, hop_count);
    return false;
}

// returns true if rep is newer
//         false if rep is discarded
int aodv_db_rt_capt_rrep(uint8_t destination_host[ETH_ALEN],
                         uint8_t destination_host_next_hop[ETH_ALEN],
                         dessert_meshif_t* output_iface,
                         uint32_t destination_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval* timestamp) {

    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // if not found -> create routing entry
        if(!rt_entry_create(&rt_entry, destination_host, timestamp)) {
            return false;
        }

        dessert_debug("create route to " MAC ": destination_sequence_number=%" PRIu32 "",
                      EXPLODE_ARRAY6(destination_host), destination_sequence_number);

        HASH_ADD_KEYPTR(hh, rt.entries, rt_entry->destination_host, ETH_ALEN, rt_entry);
    }
#ifndef ANDROID
    if(signal_strength_threshold > 0) {
        /* preemptive rreq is turned on */
        //this is a routing update, so reset the max rssi val of the next hop
        db_nt_reset_rssi(destination_host_next_hop, output_iface, timestamp);
    }
#endif

    int z = destination_sequence_number; /* == 0 -> force */
    int u = (rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN);
    int a = hf_comp_u32(rt_entry->destination_sequence_number, destination_sequence_number);
    dessert_trace("destination_sequence_number=%" PRIu32 ":%" PRIu32 " - metric=%" AODV_PRI_METRIC ":%" AODV_PRI_METRIC "", rt_entry->destination_sequence_number, destination_sequence_number, rt_entry->metric, metric);

    if(z == 0 || u || a <= 0) {

        nht_entry_t* nht_entry;
        nht_destlist_entry_t* destlist_entry;

        // remove old next_hop_entry if found
        if(!(rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN)) {
            HASH_FIND(hh, nht, rt_entry->destination_host_next_hop, ETH_ALEN, nht_entry);

            if(nht_entry != NULL) {
                HASH_FIND(hh, nht_entry->dest_list, rt_entry->destination_host, ETH_ALEN, destlist_entry);

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
        memcpy(rt_entry->destination_host_next_hop, destination_host_next_hop, ETH_ALEN);
        rt_entry->output_iface = output_iface;
        rt_entry->destination_sequence_number = destination_sequence_number;
        rt_entry->metric = metric;
        rt_entry->hop_count = hop_count;
        rt_entry->flags &= ~AODV_FLAGS_NEXT_HOP_UNKNOWN;
        rt_entry->flags &= ~AODV_FLAGS_ROUTE_INVALID;
        rt_entry->flags &= ~AODV_FLAGS_ROUTE_WARN;

        // map also this routing entry to next hop
        HASH_FIND(hh, nht, destination_host_next_hop, ETH_ALEN, nht_entry);

        if(nht_entry == NULL) {
            if(nht_entry_create(&nht_entry, destination_host_next_hop) == false) {
                return false;
            }

            HASH_ADD_KEYPTR(hh, nht, nht_entry->destination_host_next_hop, ETH_ALEN, nht_entry);
        }

        HASH_FIND(hh, nht_entry->dest_list, destination_host, ETH_ALEN, destlist_entry);

        if(destlist_entry == NULL) {
            if(nht_destlist_entry_create(&destlist_entry, destination_host, rt_entry) == false) {
                return false;
            }

            HASH_ADD_KEYPTR(hh, nht_entry->dest_list, destlist_entry->destination_host, ETH_ALEN, destlist_entry);
        }

        destlist_entry->rt_entry = rt_entry;

        return true;
    }

    dessert_debug("get OLD rrep from " MAC ": destination_sequence_number=%" PRIu32 ":%" PRIu32 "",
                  EXPLODE_ARRAY6(destination_host), rt_entry->destination_sequence_number, destination_sequence_number);

    return false;
}

int aodv_db_rt_getroute2dest(uint8_t destination_host[ETH_ALEN], uint8_t destination_host_next_hop_out[ETH_ALEN],
                             dessert_meshif_t** output_iface_out, struct timeval* timestamp, uint8_t flags) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN || rt_entry->flags & AODV_FLAGS_ROUTE_INVALID) {
        dessert_debug("route to " MAC " is invalid", EXPLODE_ARRAY6(destination_host));
        return false;
    }

    rt_entry->flags |= flags;

    memcpy(destination_host_next_hop_out, rt_entry->destination_host_next_hop, ETH_ALEN);
    *output_iface_out = rt_entry->output_iface;
    timeslot_addobject(rt.ts, timestamp, rt_entry);
    return true;
}

int aodv_db_rt_getnexthop(uint8_t destination_host[ETH_ALEN], uint8_t destination_host_next_hop_out[ETH_ALEN]) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        return false;
    }

    memcpy(destination_host_next_hop_out, rt_entry->destination_host_next_hop, ETH_ALEN);
    return true;
}

int aodv_db_rt_getprevhop(uint8_t destination_host[ETH_ALEN], uint8_t originator_host[ETH_ALEN],
                          uint8_t originator_host_prev_hop_out[ETH_ALEN], dessert_meshif_t** output_iface_out) {

    aodv_rt_entry_t* rt_entry;
    aodv_rt_srclist_entry_t* srclist_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        return false;
    }

    HASH_FIND(hh, rt_entry->src_list, originator_host, ETH_ALEN, srclist_entry);

    if(srclist_entry == NULL) {
        return false;
    }

    memcpy(originator_host_prev_hop_out, srclist_entry->originator_host_prev_hop, ETH_ALEN);
    *output_iface_out = srclist_entry->output_iface;

    return true;
}

// returns true if dest is known
//         false if des is unknown
int aodv_db_rt_get_destination_sequence_number(uint8_t dhost_ether[ETH_ALEN], uint32_t* destination_sequence_number_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, dhost_ether, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        *destination_sequence_number_out = 0;
        return false;
    }

    *destination_sequence_number_out = rt_entry->destination_sequence_number;
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

int aodv_db_rt_get_metric(uint8_t destination_host[ETH_ALEN], metric_t* last_metric_out) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        *last_metric_out = AODV_MAX_METRIC;
        return false;
    }
    *last_metric_out =  rt_entry->metric;
    return true;
}

int aodv_db_rt_markrouteinv(uint8_t destination_host[ETH_ALEN], uint32_t destination_sequence_number) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, destination_host, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        return false;
    }

    if(rt_entry->destination_sequence_number > destination_sequence_number) {
        dessert_debug("route to " MAC " seq=%" PRIu32 ":%" PRIu32 " NOT marked as invalid", EXPLODE_ARRAY6(destination_host), rt_entry->destination_sequence_number, destination_sequence_number);
        return false;
    }

    dessert_debug("route to " MAC " seq=%" PRIu32 ":%" PRIu32 " marked as invalid", EXPLODE_ARRAY6(destination_host), rt_entry->destination_sequence_number, destination_sequence_number);
    rt_entry->flags |= AODV_FLAGS_ROUTE_INVALID;
    return true;
}

int aodv_db_rt_get_destlist(uint8_t dhost_next_hop[ETH_ALEN], aodv_link_break_element_t** destlist) {
    // find appropriate routing entry
    nht_entry_t* nht_entry;
    HASH_FIND(hh, nht, dhost_next_hop, ETH_ALEN, nht_entry);

    if(nht_entry == NULL) {
        return false;
    }

    struct nht_destlist_entry* dest, *tmp;

    HASH_ITER(hh, nht_entry->dest_list, dest, tmp) {
        aodv_link_break_element_t* el = malloc(sizeof(aodv_link_break_element_t));
        memcpy(el->host, dest->rt_entry->destination_host, ETH_ALEN);
        el->sequence_number = dest->rt_entry->destination_sequence_number;
        dessert_debug("createERR: " MAC " seq=%" PRIu32 "", EXPLODE_ARRAY6(el->host), el->sequence_number);
        DL_APPEND(*destlist, el);
    }

    return true;
}

int aodv_db_rt_inv_over_nexthop(uint8_t next_hop[ETH_ALEN]) {
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

/*
 * cleanup next hop table
 * returns true on success
 */
int aodv_db_rt_remove_nexthop(uint8_t next_hop[ETH_ALEN]) {

    nht_entry_t* nht_entry;
    HASH_FIND(hh, nht, next_hop, ETH_ALEN, nht_entry);

    if(nht_entry == NULL) {
        return false;
    }

    struct nht_destlist_entry* dest, *tmp;

    HASH_ITER(hh, nht_entry->dest_list, dest, tmp) {
        HASH_DEL(nht_entry->dest_list, dest);
        free(dest);
    }

    HASH_DEL(nht, nht_entry);
    free(nht_entry);
    return true;
}

//get all routes over one neighbor
int aodv_db_rt_get_warn_endpoints_from_neighbor_and_set_warn(uint8_t neighbor[ETH_ALEN], aodv_link_break_element_t** head) {
    // find appropriate routing entry
    nht_entry_t* nht_entry;
    HASH_FIND(hh, nht, neighbor, ETH_ALEN, nht_entry);

    if((nht_entry == NULL) || (nht_entry->dest_list == NULL)) {
        return false;
    }

    *head = NULL;
    struct nht_destlist_entry* dest, *tmp;

    HASH_ITER(hh, nht_entry->dest_list, dest, tmp) {
        if(dest->rt_entry->flags & AODV_FLAGS_ROUTE_WARN) {
            continue;
        }

        if(!(dest->rt_entry->flags & AODV_FLAGS_ROUTE_LOCAL_USED)) {
            continue;
        }

        dessert_debug("dest->rt_entry->flags = %" PRIu8 "->%p", dest->rt_entry->flags, dest->rt_entry);
        aodv_link_break_element_t* curr_el = malloc(sizeof(aodv_link_break_element_t));
        memcpy(curr_el->host, dest->rt_entry->destination_host, ETH_ALEN);
        curr_el->sequence_number = dest->rt_entry->destination_sequence_number;
        DL_APPEND(*head, curr_el);
        dest->rt_entry->flags |= AODV_FLAGS_ROUTE_WARN;
    }
    return true;
}

int aodv_db_rt_get_warn_status(uint8_t dhost_ether[ETH_ALEN]) {
    aodv_rt_entry_t* rt_entry;
    HASH_FIND(hh, rt.entries, dhost_ether, ETH_ALEN, rt_entry);

    if(rt_entry == NULL || rt_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
        return false;
    }

    dessert_debug("rt_entry->flags = %" PRIu8 "->%p", rt_entry->flags, rt_entry);
    return ((rt_entry->flags & AODV_FLAGS_ROUTE_WARN) ? true : false);
}

int aodv_db_rt_get_active_routes(aodv_link_break_element_t** head) {
    *head = NULL;
    aodv_rt_entry_t* dest, *tmp;

    HASH_ITER(hh, rt.entries, dest, tmp) {
        if(dest->flags & AODV_FLAGS_ROUTE_LOCAL_USED) {
            aodv_link_break_element_t* curr_el = malloc(sizeof(aodv_link_break_element_t));
            memset(curr_el, 0x0, sizeof(aodv_link_break_element_t));
            memcpy(curr_el->host, dest->destination_host, ETH_ALEN);
            DL_APPEND(*head, curr_el);
        }
    }
    return true;
}

int aodv_db_rt_routing_reset(uint32_t* count_out) {

    *count_out = 0;

    aodv_rt_entry_t* dest = NULL;
    aodv_rt_entry_t* tmp = NULL;
    HASH_ITER(hh, rt.entries, dest, tmp) {
        dest->flags |= AODV_FLAGS_ROUTE_INVALID;
        dessert_debug("routing table reset: " MAC " is now invalid!", EXPLODE_ARRAY6(dest->destination_host));
        (*count_out)++;
    }
    return true;
}

int aodv_db_rt_cleanup(struct timeval* timestamp) {
    return timeslot_purgeobjects(rt.ts, timestamp);
}

int aodv_db_rt_report(char** str_out) {
    aodv_rt_entry_t* current_entry = rt.entries;
    char* output;
    char entry_str[REPORT_RT_STR_LEN  + 1];

    uint32_t len = 0;

    while(current_entry != NULL) {
        len += REPORT_RT_STR_LEN * 2;
        current_entry = current_entry->hh.next;
    }

    current_entry = rt.entries;
    output = malloc(sizeof(char) * REPORT_RT_STR_LEN * (4 + len) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    output[0] = '\0';
    strcat(output, "+-------------------+-------------------+-------------------+-------------+---------------+\n"
           "|    destination    |      next hop     |  out iface addr   |  route inv  | next hop unkn |\n"
           "+-------------------+-------------------+-------------------+-------------+---------------+\n");

    while(current_entry != NULL) {		// first line for best output interface
        if(current_entry->flags & AODV_FLAGS_NEXT_HOP_UNKNOWN) {
            snprintf(entry_str, REPORT_RT_STR_LEN, "| " MAC " |                   |                   |   %5s     |     true      |\n",
                     EXPLODE_ARRAY6(current_entry->destination_host),
                     (current_entry->flags & AODV_FLAGS_ROUTE_INVALID) ? "true" : "false");
        }
        else {
            snprintf(entry_str, REPORT_RT_STR_LEN, "| " MAC " | " MAC " | " MAC " |    %5s    |     false     |\n",
                     EXPLODE_ARRAY6(current_entry->destination_host),
                     EXPLODE_ARRAY6(current_entry->destination_host_next_hop),
                     EXPLODE_ARRAY6(current_entry->output_iface->hwaddr),
                     (current_entry->flags & AODV_FLAGS_ROUTE_INVALID) ? "true" : "false");
        }

        strcat(output, entry_str);
        strcat(output, "+-------------------+-------------------+-------------------+-------------+---------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return true;
}
