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
#include "../schedule_table/aodv_st.h"

#undef assert
#include <assert.h>

/**
 * precursor_entry_t
 * list of neighbors communicating with the destination this precursor list belongs to
 */
typedef struct precursor_entry {
    nt_neighbor_t const *neighbor;
    struct precursor_entry *prev, *next;
} precursor_entry_t;

/**
 * aodv_rt_entry_t
 */
typedef struct rt_entry {
    mac_addr             addr;            // hash key
    nt_neighbor_t const *next_hop;
    uint32_t             sequence_number;
    metric_t             metric;
    uint8_t              hop_count;
    bool                 invalid;
    bool                 seq_num_valid;
    precursor_entry_t*   precursor_list;
    UT_hash_handle       hh;
} rt_entry_t;

struct rt {
    rt_entry_t* entries;
    timeslot_t*      timeouts;
} rt = { NULL, NULL };

static precursor_entry_t *precursor_add(precursor_entry_t *this, nt_neighbor_t const *precursor) {
    precursor_entry_t *new = malloc(sizeof(*new));
    new->neighbor = precursor;
    DL_APPEND(this, new);
    return new;
}

static precursor_entry_t *precursor_find_or_add(precursor_entry_t *this, nt_neighbor_t const *precursor) {
    precursor_entry_t *result;
    DL_SEARCH_SCALAR(this, result, neighbor, precursor);
    return result ? result : precursor_add(this, precursor);
}

static void precursor_delete(precursor_entry_t *this, nt_neighbor_t const *precursor) {
    precursor_entry_t *entry;
    DL_SEARCH_SCALAR(this, entry, neighbor, precursor);
    if(entry) {
        DL_DELETE(this, entry);
        free(entry);
    }
}

static void rt_entry_delete(rt_entry_t *this) {
    precursor_entry_t *i, *tmp;
    DL_FOREACH_SAFE(this->precursor_list, i, tmp) {
        DL_DELETE(this->precursor_list, i);
        free(i);
    }
    free(this);
}

static rt_entry_t *rt_find(mac_addr addr) {
    rt_entry_t *entry;
    HASH_FIND(hh, rt.entries, addr, ETH_ALEN, entry);
    return entry;
}

static rt_entry_t *rt_add(mac_addr addr, struct timeval const *creation_time) {
    rt_entry_t* rt_entry = malloc(sizeof(*rt_entry));

    mac_copy(rt_entry->addr, addr);
    rt_entry->seq_num_valid = false;
    rt_entry->next_hop = NULL;
    rt_entry->metric = AODV_MAX_METRIC;
    rt_entry->hop_count = UINT8_MAX;
    rt_entry->invalid = true;
    rt_entry->precursor_list = NULL;
    HASH_ADD_KEYPTR(hh, rt.entries, rt_entry->addr, ETH_ALEN, rt_entry);

    timeslot_addobject(rt.timeouts, creation_time, rt_entry);

    dessert_trace("new routing entry " MAC, EXPLODE_ARRAY6(addr));

    return rt_entry;
}

static rt_entry_t *rt_find_or_add(mac_addr addr, struct timeval const *creation_time) {
    rt_entry_t* entry = rt_find(addr);
    if(!entry) {
        entry = rt_add(addr, creation_time);
    }
    return entry;
}

/* callback for timeslot, deleting the corresponding timelsot is done there */
static void rt_purge_entry(struct timeval* timestamp __attribute__((unused)), void* src_object __attribute__((unused)), void* del_object) {
    rt_entry_t *entry = del_object;
    HASH_DEL(rt.entries, entry);
    dessert_debug("delete route to " MAC, EXPLODE_ARRAY6(entry->addr));
    rt_entry_delete(entry);
}

int aodv_db_rt_init(void) {
    struct timeval my_route_timeout;
    dessert_ms2timeval(MY_ROUTE_TIMEOUT, &my_route_timeout);
    rt.timeouts = timeslot_create(&my_route_timeout, NULL, rt_purge_entry);
    assert(rt.timeouts);
    return true;
}

static void rt_entry_update(rt_entry_t *this, nt_neighbor_t const  *next_hop, uint32_t seq_num, metric_t metric, uint8_t hop_count) {
    this->next_hop = next_hop;
    this->sequence_number = seq_num;
    this->seq_num_valid = true;
    this->metric = metric;
    this->hop_count = hop_count;
    this->invalid = false;
}

/** update db according to data in rrep
 *  @return result of capture
 */
static aodv_capt_result_t capt_rrep(rt_entry_t           *sender,
                                    nt_neighbor_t const  *prev_hop,
                                    uint32_t              rrep_seq_num,
                                    metric_t              metric,
                                    uint8_t               hop_count,
                                    struct timeval const *timestamp) {
    if(!sender->seq_num_valid) { // RFC 6.7.i
        rt_entry_update(sender, prev_hop, rrep_seq_num, metric, hop_count);
        return AODV_CAPT_NEW;
    }

    int seq_num_cmp = hf_comp_u32(rrep_seq_num, sender->sequence_number);
    if(seq_num_cmp > 0) { // RFC 6.7.ii
        rt_entry_update(sender, prev_hop, rrep_seq_num, metric, hop_count);
        return AODV_CAPT_NEW;
    }

    bool metric_hit;
    if(metric_type == AODV_METRIC_RFC) {
        metric_hit = (hop_count < sender->hop_count);
    }
    else {
        metric_hit = hf_comp_metric(sender->metric, metric) < 0;
    }
    if(seq_num_cmp == 0 && metric_hit) { // RFC 6.7.iv
        rt_entry_update(sender, prev_hop, rrep_seq_num, metric, hop_count);
        return AODV_CAPT_METRIC_HIT;
    }

    return AODV_CAPT_OLD;
}


int aodv_db_rt_capt_rrep(mac_addr              destination_host,
                         mac_addr              prev_hop_addr,
                         dessert_meshif_t     *iface,
                         uint32_t              destination_sequence_number,
                         metric_t              metric,
                         uint8_t               hop_count,
                         struct timeval const *timestamp) {

    nt_neighbor_t const *prev_hop = aodv_db_nt_lookup(prev_hop_addr, iface, timestamp);
    rt_entry_t* rt_entry = rt_find_or_add(destination_host, timestamp);
    if(!prev_hop || !rt_entry) {
        return false;
    }

    aodv_capt_result_t capt_result = capt_rrep(rt_entry, prev_hop, destination_sequence_number, metric, hop_count, timestamp);
    switch(capt_result) {
        case AODV_CAPT_NEW:
            return true;
        case AODV_CAPT_METRIC_HIT:
            return true;
        case AODV_CAPT_OLD:
            return false;
        default:
            abort();
    }
}

/** update db according to data in rreq
 *  @return result of capture
 */
static aodv_capt_result_t capt_rreq(rt_entry_t           *destination,
                                    rt_entry_t           *originator,
                                    nt_neighbor_t const  *prev_hop,
                                    uint32_t              orig_seq_num,
                                    metric_t              metric,
                                    uint8_t               hop_count,
                                    struct timeval const *timestamp) {

    precursor_add(destination->precursor_list, prev_hop);
    aodv_capt_result_t result = capt_rrep(originator, prev_hop, orig_seq_num, metric, hop_count, timestamp);
    // RFC does not forward metric hits
    if(metric_type == AODV_METRIC_RFC && result == AODV_CAPT_METRIC_HIT) {
        result = AODV_CAPT_OLD;
    }
    return result;
}

int aodv_db_rt_capt_rreq(mac_addr              destination_host,
                         mac_addr              originator_host,
                         mac_addr              prev_hop_addr,
                         dessert_meshif_t     *iface,
                         uint32_t              originator_sequence_number,
                         metric_t              metric,
                         uint8_t               hop_count,
                         struct timeval const *timestamp,
                         aodv_capt_result_t* result_out) {

    rt_entry_t *dest_entry = rt_find_or_add(destination_host, timestamp);
    rt_entry_t *orig_entry = rt_find_or_add(originator_host, timestamp);
    nt_neighbor_t const *precursor = aodv_db_nt_lookup(prev_hop_addr, iface, timestamp);

    *result_out = capt_rreq(dest_entry, orig_entry, precursor, originator_sequence_number, metric, hop_count, timestamp);
    return true;
}

int aodv_db_rt_getroute2dest(mac_addr              destination_host,
                             mac_addr             *next_hop_out,
                             dessert_meshif_t    **iface_out,
                             struct timeval const *timestamp) {
    rt_entry_t* rt_entry = rt_find(destination_host);

    if(!rt_entry || !rt_entry->next_hop || rt_entry->invalid) {
        dessert_debug("route to " MAC " is invalid", EXPLODE_ARRAY6(destination_host));
        return false;
    }

    mac_copy(*next_hop_out, aodv_db_nt_get_addr(rt_entry->next_hop));
    *iface_out = aodv_db_nt_get_iface(rt_entry->next_hop);
    timeslot_addobject(rt.timeouts, timestamp, rt_entry);
    return true;
}

int aodv_db_rt_getnexthop(mac_addr destination_host, mac_addr *next_hop_out) {
    rt_entry_t* rt_entry = rt_find(destination_host);

    if(!rt_entry || !rt_entry->next_hop) {
        return false;
    }

    mac_copy(*next_hop_out, aodv_db_nt_get_addr(rt_entry->next_hop));
    return true;
}

/** @return true if sequence number retrieved, false otherwise */
int aodv_db_rt_get_destination_sequence_number(mac_addr destination_host, uint32_t* destination_sequence_number_out) {
    rt_entry_t* rt_entry = rt_find(destination_host);

    if(!rt_entry || !rt_entry->seq_num_valid) {
        return false;
    }

    *destination_sequence_number_out = rt_entry->sequence_number;
    return true;
}

int aodv_db_rt_get_hopcount(mac_addr destination_host, uint8_t* hop_count_out) {
    rt_entry_t* rt_entry = rt_find(destination_host);

    if(!rt_entry || !rt_entry->next_hop) {
        return false;
    }
    *hop_count_out =  rt_entry->hop_count;
    return true;
}

int aodv_db_rt_get_metric(mac_addr destination_host, metric_t* last_metric_out) {
    rt_entry_t* rt_entry = rt_find(destination_host);

    if(!rt_entry || !rt_entry->next_hop) {
        *last_metric_out = AODV_MAX_METRIC;
        return false;
    }
    *last_metric_out =  rt_entry->metric;
    return true;
}

int aodv_db_rt_markrouteinv(mac_addr destination_host, uint32_t destination_sequence_number) {
    rt_entry_t* destination = rt_find(destination_host);

    if(!destination) {
        return false;
    }
    if(destination->invalid) {
        return false;
    }
    if(destination->seq_num_valid && hf_comp_u32(destination->sequence_number, destination_sequence_number) > 0) {
        return false;
    }

    dessert_debug("route to " MAC " seq=%" PRIu32 ":%" PRIu32 " marked as invalid", EXPLODE_ARRAY6(destination_host), destination->sequence_number, destination_sequence_number);
    destination->invalid = true;
    return true;
}

int aodv_db_rt_inv_nexthop(nt_neighbor_t const *next_hop, struct timeval *timestamp) {
    aodv_link_break_element_t* destlist = NULL;

    for(rt_entry_t* dest = rt.entries; dest; dest = dest->hh.next) {
        precursor_delete(dest->precursor_list, next_hop);
        if(dest->next_hop != next_hop) {
            continue;
        }
        dest->invalid = true;

        aodv_link_break_element_t* el = malloc(sizeof(*el));
        mac_copy(el->host, dest->addr);
        assert(dest->seq_num_valid);
        el->sequence_number = dest->sequence_number;
        dessert_trace("create ERR: " MAC " seq=%ju", EXPLODE_ARRAY6(el->host), (uintmax_t)el->sequence_number);
        DL_APPEND(destlist, el);
    }
    aodv_db_sc_addschedule(timestamp, aodv_db_nt_get_addr(next_hop), AODV_SC_SEND_OUT_RERR, destlist);
    return true;
}

int aodv_db_rt_add_precursor(mac_addr destination_host, mac_addr precursor_addr, dessert_meshif_t* iface) {
    rt_entry_t *destination = rt_find(destination_host);
    nt_neighbor_t const *precursor = aodv_db_nt_lookup(precursor_addr, iface, NULL);

    if(!destination || !precursor) {
        return false;
    }

    precursor_find_or_add(destination->precursor_list, precursor);

    return true;
}

int aodv_db_rt_routing_reset(uint32_t* count_out) {
    *count_out = 0;

    for(rt_entry_t *entry = rt.entries; entry; entry = entry->hh.next){
        entry->invalid = true;
        dessert_debug("routing table reset: " MAC " is now invalid!", EXPLODE_ARRAY6(entry->addr));
        (*count_out)++;
    }
    return true;
}

int aodv_db_rt_cleanup(struct timeval* timestamp) {
    timeslot_purgeobjects(rt.timeouts, timestamp);
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

    for(rt_entry_t *entry = rt.entries; entry; entry = entry->hh.next) {
        if(!entry->next_hop) {
            snprintf(entry_str, sizeof(entry_str), "| " MAC " |                   |                   |   %5s     |     true      |\n",
                     EXPLODE_ARRAY6(entry->addr),
                     entry->invalid ? "true" : "false");
        }
        else {
            snprintf(entry_str, sizeof(entry_str), "| " MAC " | " MAC " | " MAC " |    %5s    |     false     |\n",
                     EXPLODE_ARRAY6(entry->addr),
                     EXPLODE_ARRAY6(aodv_db_nt_get_addr(entry->next_hop)),
                     EXPLODE_ARRAY6(aodv_db_nt_get_iface(entry->next_hop)->hwaddr),
                     entry->invalid ? "true" : "false");
        }

        strcat(*str_out, entry_str);
        strcat(*str_out, "+-------------------+-------------------+-------------------+-------------+---------------+\n");
    }
    return true;
}
