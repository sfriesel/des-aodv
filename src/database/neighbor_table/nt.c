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
#include <utlist.h>
#undef assert
#include <assert.h>

struct neighbor {
    mac_addr          addr;
    dessert_meshif_t *iface;
    nt_neighbor_t       *prev, *next;
};

struct neighbor_table {
    nt_neighbor_t  *entries;
    timeslot_t  *ts;
} nt;

static void db_nt_on_neighbor_timeout(struct timeval* timestamp, void* src_object, void* object) {
    nt_neighbor_t* curr_entry = object;
    dessert_debug("neighbor " MAC " timed out on %s", EXPLODE_ARRAY6(curr_entry->addr), curr_entry->iface->if_name);
    DL_DELETE(nt.entries, curr_entry);

    aodv_db_sc_addschedule(timestamp, curr_entry->addr, AODV_SC_SEND_OUT_RERR, 0);
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

    nt_neighbor_t* ngbr = NULL;
    nt_neighbor_t* tmp = NULL;
    DL_FOREACH_SAFE(nt.entries, ngbr, tmp) {
        DL_DELETE(nt.entries, ngbr);
        free(ngbr);
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

static nt_neighbor_t *aodv_db_nt_lookup_nonconst(mac_addr addr, dessert_meshif_t *iface, struct timeval const *timestamp) {
    if(timestamp) {
        timeslot_purgeobjects(nt.ts, timestamp);
    }
    nt_neighbor_t *ngbr;
    DL_FOREACH(nt.entries, ngbr) {
        if(mac_equal(addr, ngbr->addr) && iface == ngbr->iface) {
            break;
        }
    }
    return ngbr;
}

nt_neighbor_t const *aodv_db_nt_lookup(mac_addr addr, dessert_meshif_t *iface, struct timeval const *timestamp) {
    return aodv_db_nt_lookup_nonconst(addr, iface, timestamp);
}

int aodv_db_nt_capt_hellorsp(mac_addr addr, uint16_t hello_seq __attribute__((unused)), dessert_meshif_t* iface, struct timeval const *timestamp) {
    nt_neighbor_t* curr_entry = aodv_db_nt_lookup_nonconst(addr, iface, timestamp);

    if(curr_entry == NULL) {
        curr_entry = malloc(sizeof(*curr_entry));
        mac_copy(curr_entry->addr, addr);
        curr_entry->iface = iface;
        DL_APPEND(nt.entries, curr_entry);
        dessert_debug("neighbor " MAC " established on %s", EXPLODE_ARRAY6(curr_entry->addr), curr_entry->iface->if_name);
    }
    timeslot_addobject(nt.ts, timestamp, curr_entry);
    return true;
}

void aodv_db_nt_report(char** str_out) {
    timeslot_report(nt.ts, str_out);
}

int aodv_db_nt_cleanup(struct timeval* timestamp) {
    timeslot_purgeobjects(nt.ts, timestamp);
    return true;
}
