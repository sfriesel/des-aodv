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

#include <pthread.h>
#include <dessert.h>

#include "aodv_database.h"
#include "../config.h"
#include "pdr_tracker/pdr.h"
#include "routing_table/aodv_rt.h"
#include "neighbor_table/nt.h"
#include "data_seq/ds.h"
#include "packet_buffer/packet_buffer.h"
#include "schedule_table/aodv_st.h"
#include "rreq_log/rreq_log.h"
#include "rerr_log/rerr_log.h"

pthread_rwlock_t db_rwlock = PTHREAD_RWLOCK_INITIALIZER;

void aodv_db_rlock() {
    pthread_rwlock_rdlock(&db_rwlock);
}

void aodv_db_wlock() {
    pthread_rwlock_wrlock(&db_rwlock);
}

void aodv_db_unlock() {
    pthread_rwlock_unlock(&db_rwlock);
}

int aodv_db_init() {
    int success = true;
    aodv_db_wlock();
    success &= db_nt_init();
    success &= aodv_db_pdr_nt_init();
    success &= db_ds_init();
    success &= aodv_db_rt_init();
    success &= pb_init();
    success &= aodv_db_rerrl_init();
    success &= aodv_db_rl_init();
    aodv_db_unlock();
    return success;
}

int aodv_db_cleanup(struct timeval* timestamp) {
    int success = true;
    aodv_db_wlock();
    success &= db_nt_cleanup(timestamp);
    success &= db_ds_cleanup(timestamp);
    success &= aodv_db_rt_cleanup(timestamp);
    success &= pb_cleanup(timestamp);
    success &= aodv_db_pdr_nt_cleanup(timestamp);
    aodv_db_unlock();
    return success;
}

int aodv_db_neighbor_reset(uint32_t* count_out) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_nt_neighbor_reset(count_out);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

int aodv_db_pdr_neighbor_reset(uint32_t* count_out) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_neighbor_reset(count_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_upd_expected(uint16_t new_interval) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_upd_expected(new_interval);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_cap_hello(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t hello_seq, uint16_t hello_interv, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_cap_hello(ether_neighbor_addr, hello_seq, hello_interv, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_cap_hellorsp(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t hello_interv, uint8_t hello_count, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_cap_hellorsp(ether_neighbor_addr, hello_interv, hello_count, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_pdr(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t* pdr_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_pdr(ether_neighbor_addr, pdr_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_etx_mul(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t* etx_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_etx_mul(ether_neighbor_addr, etx_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_etx_add(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t* etx_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_etx_add(ether_neighbor_addr, etx_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_rcvdhellocount(uint8_t ether_neighbor_addr[ETH_ALEN], uint8_t* count_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_rcvdhellocount(ether_neighbor_addr, count_out, timestamp);
    aodv_db_unlock();
    return result;
}

void aodv_db_push_packet(uint8_t dhost_ether[ETH_ALEN], dessert_msg_t* msg, struct timeval* timestamp) {
    aodv_db_wlock();
    pb_push_packet(dhost_ether, msg, timestamp);
    aodv_db_unlock();
}

dessert_msg_t* aodv_db_pop_packet(uint8_t dhost_ether[ETH_ALEN]) {
    aodv_db_wlock();
    dessert_msg_t* result = pb_pop_packet(dhost_ether);
    aodv_db_unlock();
    return result;
}

/**
 * Captures seq_num of the source. Also add to source list for
 * this destination. All messages to source (example: RREP) must be send
 * over shost_prev_hop (nodes output interface: output_iface).
 */
int aodv_db_capt_rreq(uint8_t destination_host[ETH_ALEN], uint8_t originator_host[ETH_ALEN], uint8_t originator_host_prev_hop[ETH_ALEN], dessert_meshif_t* output_iface, uint32_t originator_sequence_number, metric_t metric, uint8_t hop_count, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_rt_capt_rreq(destination_host, originator_host, originator_host_prev_hop, output_iface, originator_sequence_number, metric, hop_count, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_capt_rrep(uint8_t destination_host[ETH_ALEN], uint8_t destination_host_next_hop[ETH_ALEN], dessert_meshif_t* output_iface, uint32_t destination_sequence_number, metric_t metric, uint8_t hop_count, struct timeval* timestamp) {
    aodv_db_wlock();
    int result =  aodv_db_rt_capt_rrep(destination_host, destination_host_next_hop, output_iface, destination_sequence_number, metric, hop_count, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_getroute2dest(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN], dessert_meshif_t** output_iface_out, struct timeval* timestamp, uint8_t flags) {
    aodv_db_wlock();
    int result =  aodv_db_rt_getroute2dest(dhost_ether, dhost_next_hop_out, output_iface_out, timestamp, flags);
    aodv_db_unlock();
    return result;
}

int aodv_db_getnexthop(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN]) {
    aodv_db_rlock();
    int result =  aodv_db_rt_getnexthop(dhost_ether, dhost_next_hop_out);
    aodv_db_unlock();
    return result;
}

/**
 * gets prev_hop address and output_iface towards source with shost_ether address
 * that has produces an RREQ to destination with dhost_ether address
 * (DB - read)
 */
int aodv_db_getprevhop(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN], uint8_t shost_next_hop_out[ETH_ALEN], dessert_meshif_t** output_iface_out) {
    aodv_db_rlock();
    int result =  aodv_db_rt_getprevhop(dhost_ether, shost_ether, shost_next_hop_out, output_iface_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_destination_sequence_number(uint8_t dhost_ether[ETH_ALEN], uint32_t* destination_sequence_number_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_destination_sequence_number(dhost_ether, destination_sequence_number_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_originator_sequence_number(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN], uint32_t* originator_sequence_number_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_originator_sequence_number(dhost_ether, shost_ether, originator_sequence_number_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_orginator_metric(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN], metric_t* last_metric_orginator_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_orginator_metric(dhost_ether, shost_ether, last_metric_orginator_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_markrouteinv(uint8_t dhost_ether[ETH_ALEN], uint32_t destination_sequence_number) {
    aodv_db_wlock();
    int result =  aodv_db_rt_markrouteinv(dhost_ether, destination_sequence_number);
    aodv_db_unlock();
    return result;
}

int aodv_db_remove_nexthop(uint8_t next_hop[ETH_ALEN]) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result =  aodv_db_rt_remove_nexthop(next_hop);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

int aodv_db_inv_over_nexthop(uint8_t next_hop[ETH_ALEN]) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_rt_inv_over_nexthop(next_hop);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

int aodv_db_get_destlist(uint8_t dhost_next_hop[ETH_ALEN], aodv_link_break_element_t** destlist) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_rt_get_destlist(dhost_next_hop, destlist);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

int aodv_db_get_warn_endpoints_from_neighbor_and_set_warn(uint8_t neighbor[ETH_ALEN], aodv_link_break_element_t** head) {
    aodv_db_wlock();
    int result = aodv_db_rt_get_warn_endpoints_from_neighbor_and_set_warn(neighbor, head);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_warn_status(uint8_t dhost_ether[ETH_ALEN]) {
    aodv_db_wlock();
    int result = aodv_db_rt_get_warn_status(dhost_ether);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_active_routes(aodv_link_break_element_t** head) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_rt_get_active_routes(head);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

int aodv_db_routing_reset(uint32_t* count_out) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_rt_routing_reset(count_out);
    pthread_rwlock_unlock(&db_rwlock);
    return result;
}

/**
 * Take a record that the given neighbor seems to be
 * the 1 hop bidirectional neighbor
 */
int aodv_db_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], uint16_t hello_seq, dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = db_nt_cap2Dneigh(ether_neighbor_addr, hello_seq, iface, timestamp);
    aodv_db_unlock();
    return result;
}

/**
 * Check whether given neighbor is 1 hop bidirectional neighbor
 */
int aodv_db_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result =  db_nt_check2Dneigh(ether_neighbor_addr, iface, timestamp);
    aodv_db_unlock();
    return result;
}

#ifndef ANDROID
int aodv_db_reset_rssi(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = db_nt_reset_rssi(ether_neighbor_addr, iface, timestamp);
    aodv_db_unlock();
    return result;
}

int8_t aodv_db_update_rssi(uint8_t ether_neighbor[ETH_ALEN], dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = db_nt_update_rssi(ether_neighbor, iface, timestamp);
    aodv_db_unlock();
    return result;
}
#endif

int aodv_db_addschedule(struct timeval* execute_ts, uint8_t ether_addr[ETH_ALEN], uint8_t type, void* param) {
    aodv_db_wlock();
    int result =  aodv_db_sc_addschedule(execute_ts, ether_addr, type, param);
    aodv_db_unlock();
    return result;
}

int aodv_db_popschedule(struct timeval* timestamp, uint8_t ether_addr_out[ETH_ALEN], uint8_t* type, void* param) {
    aodv_db_wlock();
    int result =  aodv_db_sc_popschedule(timestamp, ether_addr_out, type, param);
    aodv_db_unlock();
    return result;
}

int aodv_db_schedule_exists(uint8_t ether_addr[ETH_ALEN], uint8_t type) {
    aodv_db_wlock();
    int result =  aodv_db_sc_schedule_exists(ether_addr, type);
    aodv_db_unlock();
    return result;
}

int aodv_db_dropschedule(uint8_t ether_addr[ETH_ALEN], uint8_t type) {
    aodv_db_wlock();
    int result =  aodv_db_sc_dropschedule(ether_addr, type);
    aodv_db_unlock();
    return result;
}

void aodv_db_putrreq(struct timeval* timestamp) {
    aodv_db_wlock();
    aodv_db_rl_putrreq(timestamp);
    aodv_db_unlock();
}

void aodv_db_getrreqcount(struct timeval* timestamp, uint32_t* count_out) {
    aodv_db_wlock();
    aodv_db_rl_getrreqcount(timestamp, count_out);
    aodv_db_unlock();
}

void aodv_db_putrerr(struct timeval* timestamp) {
    aodv_db_wlock();
    aodv_db_rl_putrerr(timestamp);
    aodv_db_unlock();
}

void aodv_db_getrerrcount(struct timeval* timestamp, uint32_t* count_out) {
    aodv_db_wlock();
    aodv_db_rl_getrerrcount(timestamp, count_out);
    aodv_db_unlock();
}

int aodv_db_capt_data_seq(uint8_t src_addr[ETH_ALEN], uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_ds_capt_data_seq(src_addr, data_seq_num, hop_count, timestamp);
    aodv_db_unlock();
    return result;
}

// --------------------------------------- reporting ---------------------------------------------------------------

int aodv_db_view_routing_table(char** str_out) {
    aodv_db_rlock();
    int result =  aodv_db_rt_report(str_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_view_pdr_nt(char** str_out) {
    aodv_db_rlock();
    int result = aodv_db_pdr_nt_report(str_out);
    aodv_db_unlock();
    return result;
}

void aodv_db_neighbor_timeslot_report(char** str_out) {
    aodv_db_rlock();
    nt_report(str_out);
    aodv_db_unlock();
}

void aodv_db_packet_buffer_timeslot_report(char** str_out) {
    aodv_db_rlock();
    pb_report(str_out);
    aodv_db_unlock();
}

void aodv_db_data_seq_timeslot_report(char** str_out) {
    aodv_db_rlock();
    ds_report(str_out);
    aodv_db_unlock();
}
