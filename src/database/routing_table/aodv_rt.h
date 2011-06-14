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

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

int aodv_db_rt_init();

int aodv_db_rt_capt_rreq (uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN],
		uint8_t shost_prev_hop[ETH_ALEN], const dessert_meshif_t* output_iface,
		uint32_t shost_seq_num, struct timeval* timestamp);

int aodv_db_rt_capt_rrep (uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop[ETH_ALEN],
		const dessert_meshif_t* output_iface, uint32_t dhost_seq_num,
		uint8_t hop_count, struct timeval* timestamp);

int aodv_db_rt_getroute2dest(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN],
		const dessert_meshif_t** output_iface_out, struct timeval* timestamp);

int aodv_db_rt_getnexthop(uint8_t dhost_ether[ETH_ALEN], uint8_t dhost_next_hop_out[ETH_ALEN]);

int aodv_db_rt_getprevhop(uint8_t dhost_ether[ETH_ALEN], uint8_t shost_ether[ETH_ALEN],
		uint8_t shost_next_hop_out[ETH_ALEN], const dessert_meshif_t** output_iface_out);

int aodv_db_rt_getrouteseqnum(uint8_t dhost_ether[ETH_ALEN], uint32_t* dhost_seq_num_out);

int aodv_db_rt_getlastrreqseq(uint8_t dhost_ether[ETH_ALEN],
		uint8_t shost_ether[ETH_ALEN], uint32_t* shost_seq_num_out);

int aodv_db_rt_markrouteinv(uint8_t dhost_ether[ETH_ALEN]);

int aodv_db_rt_inv_route(uint8_t dhost_next_hop[ETH_ALEN], uint8_t dhost_ether_out[ETH_ALEN]);

int aodv_db_rt_cleanup (struct timeval* timestamp);

int aodv_db_rt_report(char** str_out);

#endif
