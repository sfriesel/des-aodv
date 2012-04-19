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

#ifndef AODV_NT
#define AODV_NT

#include <dessert.h>

typedef struct neighbor nt_neighbor_t;

int aodv_db_nt_init();
int aodv_db_nt_reset(uint32_t* count_out);
int aodv_db_nt_cleanup(struct timeval* timestamp);
void aodv_db_nt_report(char** str_out);
/**
 * Take a record that the given neighbor seems to be bidirectional neighbor
 */
int aodv_db_nt_capt_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_seq, dessert_meshif_t* iface, struct timeval const *timestamp);

nt_neighbor_t const *aodv_db_nt_lookup(mac_addr neighbor_addr, dessert_meshif_t* iface, struct timeval const *timestamp);
const uint8_t *aodv_db_nt_get_addr(nt_neighbor_t const *ngbr);
dessert_meshif_t *aodv_db_nt_get_iface(nt_neighbor_t const *ngbr);

#endif
