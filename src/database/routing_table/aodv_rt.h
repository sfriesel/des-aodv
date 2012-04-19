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
#include "../aodv_database.h"
#include "../../config.h"
#include "../../helper.h"
#include "../neighbor_table/nt.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

int aodv_db_rt_init();

int aodv_db_rt_capt_rreq(mac_addr              destination_host,
                         mac_addr              originator_host,
                         mac_addr              originator_host_prev_hop,
                         dessert_meshif_t     *iface,
                         uint32_t              originator_sequence_number,
                         metric_t              metric,
                         uint8_t               hop_count,
                         struct timeval const *timestamp,
                         aodv_capt_result_t   *result_out);

int aodv_db_rt_capt_rrep(mac_addr              destination_host,
                         mac_addr              prev_hop_addr,
                         dessert_meshif_t     *iface,
                         uint32_t              destination_sequence_number,
                         metric_t              metric,
                         uint8_t               hop_count,
                         struct timeval const *timestamp);

int aodv_db_rt_getroute2dest(mac_addr              destination_host,
                             mac_addr             *next_hop_out,
                             dessert_meshif_t    **iface_out,
                             struct timeval const *timestamp);

int aodv_db_rt_getnexthop(mac_addr destination_host, mac_addr *next_hop_out);

int aodv_db_rt_get_destination_sequence_number(mac_addr destination_host, uint32_t* destination_sequence_number_out);

int aodv_db_rt_get_hopcount(mac_addr destination_host, uint8_t* hop_count_out);
int aodv_db_rt_get_metric(mac_addr destination_host, metric_t* last_metric_out);

int aodv_db_rt_markrouteinv(mac_addr destination_host, uint32_t destination_sequence_number);
int aodv_db_rt_inv_nexthop(nt_neighbor_t const *next_hop, struct timeval *timestamp);
int aodv_db_rt_add_precursor(mac_addr destination, mac_addr precursor_addr, dessert_meshif_t* iface);

int aodv_db_rt_cleanup(struct timeval* timestamp);
int aodv_db_rt_routing_reset(uint32_t* count_out);

int aodv_db_rt_report(char** str_out);

#endif
