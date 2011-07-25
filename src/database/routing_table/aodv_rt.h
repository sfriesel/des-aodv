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

#ifndef AODV_RREQ_T
#define AODV_RREQ_T

#include <dessert.h>
#include <utlist.h>
#include <uthash.h>
#include "../../pipeline/aodv_pipeline.h"
#include "../timeslot.h"
#include "../../config.h"
#include "../../helper.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

typedef struct aodv_rt_srclist_entry {
    uint8_t				originator_host[ETH_ALEN]; // ID
    uint8_t				originator_host_prev_hop[ETH_ALEN];
    dessert_meshif_t*		output_iface;
    uint32_t			originator_sequence_number;
    uint32_t            hop_count;
    UT_hash_handle			hh;
} aodv_rt_srclist_entry_t;

typedef struct aodv_rt_entry {
    uint8_t				destination_host[ETH_ALEN]; // ID
    uint8_t				destination_host_next_hop[ETH_ALEN];
    dessert_meshif_t*		output_iface;
    uint32_t			destination_sequence_number;
    uint8_t				hop_count;
    /**
     * flags format: 0 0 0 0 0 0 U I
     * I - Invalid flag; route is invalid due of link breakage
     * U - next hop Unknown flag;
     */
    uint8_t				flags;
    aodv_rt_srclist_entry_t*	src_list;
    UT_hash_handle			hh;
} aodv_rt_entry_t;


typedef struct aodv_rt {
    aodv_rt_entry_t*		entrys;
    timeslot_t*			ts;
} aodv_rt_t;

/**
 * Mapping next_hop -> destination list
 */
typedef struct nht_destlist_entry {
    uint8_t				destination_host[ETH_ALEN];
    aodv_rt_entry_t*		rt_entry;
    UT_hash_handle			hh;
} nht_destlist_entry_t;

typedef struct nht_entry {
    uint8_t				destination_host_next_hop[ETH_ALEN];
    nht_destlist_entry_t*		dest_list;
    UT_hash_handle			hh;
} nht_entry_t;

int aodv_db_rt_init();

int aodv_db_rt_capt_rreq(uint8_t destination_host[ETH_ALEN],
                         uint8_t originator_host[ETH_ALEN],
                         uint8_t originator_host_prev_hop[ETH_ALEN],
                         dessert_meshif_t* output_iface,
                         uint32_t originator_sequence_number,
                         uint8_t hop_count,
                         struct timeval* timestamp);

int aodv_db_rt_capt_rrep(uint8_t destination_host[ETH_ALEN],
                         uint8_t destination_host_next_hop[ETH_ALEN],
                         dessert_meshif_t* output_iface,
                         uint32_t destination_sequence_number,
                         uint8_t hop_count,
                         struct timeval* timestamp);

int aodv_db_rt_getroute2dest(uint8_t destination_host[ETH_ALEN], uint8_t destination_host_next_hop_out[ETH_ALEN],
                             dessert_meshif_t** output_iface_out, struct timeval* timestamp);

int aodv_db_rt_getnexthop(uint8_t destination_host[ETH_ALEN], uint8_t destination_host_next_hop_out[ETH_ALEN]);

int aodv_db_rt_getprevhop(uint8_t destination_host[ETH_ALEN], uint8_t originator_host[ETH_ALEN],
                          uint8_t originator_host_prev_hop_out[ETH_ALEN], dessert_meshif_t** output_iface_out);

int aodv_db_rt_get_destination_sequence_number(uint8_t destination_host[ETH_ALEN], uint32_t* destination_sequence_number_out);

int aodv_db_rt_get_originator_sequence_number(uint8_t destination_host[ETH_ALEN], uint8_t originator_host[ETH_ALEN], uint32_t* originator_sequence_number_out);

int aodv_db_rt_get_orginator_hop_count(uint8_t destination_host[ETH_ALEN], uint8_t originator_host[ETH_ALEN], uint8_t* last_hop_count_orginator_out);

int aodv_db_rt_get_hop_count(uint8_t dhost_ether[ETH_ALEN], uint8_t* hop_count_out);

int aodv_db_rt_markrouteinv(uint8_t destination_host[ETH_ALEN]);

int aodv_db_rt_inv_route(uint8_t destination_host_next_hop[ETH_ALEN], uint8_t destination_host_out[ETH_ALEN]);

int aodv_db_rt_cleanup(struct timeval* timestamp);

int aodv_db_rt_report(char** str_out);

#endif
