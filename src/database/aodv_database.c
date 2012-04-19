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
    success &= aodv_db_nt_init();
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
    success &= aodv_db_nt_cleanup(timestamp);
    success &= db_ds_cleanup(timestamp);
    success &= aodv_db_rt_cleanup(timestamp);
    success &= pb_cleanup(timestamp);
    success &= aodv_db_pdr_nt_cleanup(timestamp);
    aodv_db_unlock();
    return success;
}

int aodv_db_neighbor_reset(uint32_t* count_out) {
    pthread_rwlock_wrlock(&db_rwlock);
    int result = aodv_db_nt_reset(count_out);
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

int aodv_db_pdr_cap_hello(mac_addr ether_neighbor_addr, uint16_t hello_seq, uint16_t hello_interv, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_cap_hello(ether_neighbor_addr, hello_seq, hello_interv, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_cap_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_interv, uint8_t hello_count, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_cap_hellorsp(ether_neighbor_addr, hello_interv, hello_count, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_pdr(mac_addr ether_neighbor_addr, uint16_t* pdr_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_pdr(ether_neighbor_addr, pdr_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_etx_mul(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_etx_mul(ether_neighbor_addr, etx_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_etx_add(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_etx_add(ether_neighbor_addr, etx_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_pdr_get_rcvdhellocount(mac_addr ether_neighbor_addr, uint8_t* count_out, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_pdr_nt_get_rcvdhellocount(ether_neighbor_addr, count_out, timestamp);
    aodv_db_unlock();
    return result;
}

void aodv_db_push_packet(mac_addr dhost_ether, dessert_msg_t* msg, struct timeval* timestamp) {
    aodv_db_wlock();
    pb_push_packet(dhost_ether, msg, timestamp);
    aodv_db_unlock();
}

dessert_msg_t* aodv_db_pop_packet(mac_addr dhost_ether) {
    aodv_db_wlock();
    dessert_msg_t* result = pb_pop_packet(dhost_ether);
    aodv_db_unlock();
    return result;
}

/**
 * Captures seq_num of the source. Also add to source list for
 * this destination. All messages to source (example: RREP) must be sent
 * over shost_prev_hop (nodes output interface: output_iface).
 */
int aodv_db_capt_rreq(mac_addr destination_host, mac_addr originator_host, mac_addr prev_hop, dessert_meshif_t *iface, uint32_t originator_sequence_number, metric_t metric, uint8_t hop_count, struct timeval const *timestamp, aodv_capt_result_t *result_out) {
    aodv_db_wlock();
    int result = aodv_db_rt_capt_rreq(destination_host, originator_host, prev_hop, iface, originator_sequence_number, metric, hop_count, timestamp, result_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_capt_rrep(mac_addr destination_host, mac_addr prev_hop, dessert_meshif_t* output_iface, uint32_t destination_sequence_number, metric_t metric, uint8_t hop_count, struct timeval const *timestamp) {
    aodv_db_wlock();
    int result =  aodv_db_rt_capt_rrep(destination_host, prev_hop, output_iface, destination_sequence_number, metric, hop_count, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_route2dest(mac_addr dhost_ether, mac_addr *next_hop_out, dessert_meshif_t** output_iface_out, struct timeval const *timestamp) {
    aodv_db_wlock();
    int result =  aodv_db_rt_getroute2dest(dhost_ether, next_hop_out, output_iface_out, timestamp);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_nexthop(mac_addr dhost_ether, mac_addr *next_hop_out) {
    aodv_db_rlock();
    int result =  aodv_db_rt_getnexthop(dhost_ether, next_hop_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_dest_seq_num(mac_addr dhost_ether, uint32_t* destination_sequence_number_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_destination_sequence_number(dhost_ether, destination_sequence_number_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_hopcount(mac_addr dhost_ether, uint8_t* hop_count_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_hopcount(dhost_ether, hop_count_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_get_metric(mac_addr dhost_ether, metric_t* last_metric_out) {
    aodv_db_rlock();
    int result = aodv_db_rt_get_metric(dhost_ether, last_metric_out);
    aodv_db_unlock();
    return result;
}

int aodv_db_markrouteinv(mac_addr dhost_ether, uint32_t destination_sequence_number) {
    aodv_db_wlock();
    int result =  aodv_db_rt_markrouteinv(dhost_ether, destination_sequence_number);
    aodv_db_unlock();
    return result;
}

int aodv_db_add_precursor(mac_addr destination, mac_addr precursor_addr, dessert_meshif_t* iface) {
    aodv_db_wlock();
    int result =  aodv_db_rt_add_precursor(destination, precursor_addr, iface);
    aodv_db_unlock();
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
int aodv_db_capt_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_seq, dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = aodv_db_nt_capt_hellorsp(ether_neighbor_addr, hello_seq, iface, timestamp);
    aodv_db_unlock();
    return result;
}

/**
 * Check whether given neighbor is 1 hop bidirectional neighbor
 */
int aodv_db_is_neighbor(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp) {
    aodv_db_wlock();
    int result = (aodv_db_nt_lookup(ether_neighbor_addr, iface, timestamp) != NULL);
    aodv_db_unlock();
    return result;
}

int aodv_db_addschedule(struct timeval* execute_ts, mac_addr ether_addr, uint8_t type, void* param) {
    aodv_db_wlock();
    int result =  aodv_db_sc_addschedule(execute_ts, ether_addr, type, param);
    aodv_db_unlock();
    return result;
}

int aodv_db_popschedule(struct timeval* timestamp, mac_addr *ether_addr_out, uint8_t* type, void* param) {
    aodv_db_wlock();
    int result =  aodv_db_sc_popschedule(timestamp, ether_addr_out, type, param);
    aodv_db_unlock();
    return result;
}

int aodv_db_dropschedule(mac_addr ether_addr, uint8_t type) {
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

int aodv_db_capt_data_seq(mac_addr src_addr, uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp) {
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
    aodv_db_nt_report(str_out);
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
