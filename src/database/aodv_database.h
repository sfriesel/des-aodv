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

#ifndef AODV_DATABASE
#define AODV_DATABASE

#include <dessert.h>

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

#include "../config.h"

/** initialize all tables of routing database */
int aodv_db_init();

int aodv_db_neighbor_reset(uint32_t* count_out);

/** cleanup (purge) old entrys from all database tables */
int aodv_db_cleanup(struct timeval* timestamp);

void aodv_db_push_packet(uint8_t dhost_ether[ETH_ALEN], dessert_msg_t* msg, struct timeval* timestamp);

dessert_msg_t* aodv_db_pop_packet(uint8_t dhost_ether[ETH_ALEN]);

/**
 * Captures seq_num of the source. Also add to source list for
 * this destination. All messages to source (example: RREP) must be send
 * over shost_prev_hop (nodes output interface: output_iface).
 */
int aodv_db_capt_rreq(uint8_t destination_host[ETH_ALEN],
                      uint8_t originator_host[ETH_ALEN],
                      uint8_t originator_host_prev_hop[ETH_ALEN],
                      dessert_meshif_t* output_iface,
                      uint32_t originator_sequence_number,
                      metric_t metric,
                      uint8_t hop_count,
                      struct timeval* timestamp);

int aodv_db_capt_rrep(uint8_t destination_host[ETH_ALEN],
                      uint8_t destination_host_next_hop[ETH_ALEN],
                      dessert_meshif_t* output_iface,
                      uint32_t destination_sequence_number,
                      metric_t metric,
                      uint8_t hop_count,
                      struct timeval* timestamp);
/**
 * gets prev_hop adress and output_iface towards source with shost_ether address
 * that has produces an RREQ to destination with dhost_ether address
 */
int aodv_db_getroute2dest(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN],
                          dessert_meshif_t** output_iface_out, struct timeval* timestamp, uint8_t flags);

int aodv_db_getnexthop(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN]);

int aodv_db_getprevhop(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN],
                       uint8_t shost_next_hop_out[ETH_ALEN], dessert_meshif_t** output_iface_out);

int aodv_db_get_destination_sequence_number(uint8_t dhost_ether[ETH_ALEN], uint32_t* destination_sequence_number_out);

int aodv_db_get_originator_sequence_number(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN], uint32_t* originator_sequence_number_out);

int aodv_db_get_orginator_metric(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN], metric_t* last_metric_orginator_out);

int aodv_db_markrouteinv(uint8_t dhost_ether[ETH_ALEN], uint32_t destination_sequence_number);
int aodv_db_remove_nexthop(uint8_t next_hop[ETH_ALEN]);
int aodv_db_inv_over_nexthop(uint8_t next_hop[ETH_ALEN]);
int aodv_db_get_destlist(uint8_t dhost_next_hop[ETH_ALEN], aodv_link_break_element_t** destlist);

int aodv_db_get_warn_endpoints_from_neighbor_and_set_warn(uint8_t neighbor[ETH_ALEN], aodv_link_break_element_t** head);
int aodv_db_get_warn_status(uint8_t dhost_ether[ETH_ALEN]);

int aodv_db_get_active_routes(aodv_link_break_element_t** head);

int aodv_db_routing_reset(uint32_t* count_out);

/**
 * Take a record that the given neighbor seems to be
 * the 1 hop bidirectional neighbor
 */
int aodv_db_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t hello_seq, dessert_meshif_t* iface, struct timeval* timestamp);

/**
 * Check whether given neighbor is 1 hop bidirectional neighbor
 */
int aodv_db_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp);

int aodv_db_reset_rssi(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp);

int8_t aodv_db_update_rssi(uint8_t ether_neighbor[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp);

int aodv_db_addschedule(struct timeval* execute_ts, uint8_t ether_addr[ETH_ALEN], uint8_t type, void* param);

int aodv_db_popschedule(struct timeval* timestamp, uint8_t ether_addr_out[ETH_ALEN], uint8_t* type, void* param);

int aodv_db_schedule_exists(uint8_t ether_addr[ETH_ALEN], uint8_t type);

int aodv_db_dropschedule(uint8_t ether_addr[ETH_ALEN], uint8_t type);

void aodv_db_putrreq(struct timeval* timestamp);

void aodv_db_getrreqcount(struct timeval* timestamp, uint32_t* count_out);

void aodv_db_putrerr(struct timeval* timestamp);

void aodv_db_getrerrcount(struct timeval* timestamp, uint32_t* count_out);

int aodv_db_capt_data_seq(uint8_t src_addr[ETH_ALEN], uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp);

// ----------------------------------- reporiting -------------------------------------------------------------------------

int aodv_db_view_routing_table(char** str_out);
void aodv_db_neighbor_timeslot_report(char** str_out);
void aodv_db_packet_buffer_timeslot_report(char** str_out);
void aodv_db_data_seq_timeslot_report(char** str_out);

#endif
