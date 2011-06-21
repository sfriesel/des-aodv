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

#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include <string.h>
#include "../database/data_seq/data_seq.h"
#include <pthread.h>
#include <utlist.h>

int aodv_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {

	// create new HELLO message with hello_ext.
	dessert_msg_t* hello_msg;
	dessert_msg_new(&hello_msg);
	hello_msg->ttl = 2;

	dessert_ext_t* ext;
	dessert_msg_addext(hello_msg, &ext, HELLO_EXT_TYPE, sizeof(struct aodv_msg_hello));

	void* payload;
	uint16_t size = max(hello_size - sizeof(dessert_msg_t) - sizeof(struct ether_header) - 2, 0);

	dessert_msg_addpayload(hello_msg, &payload, size);
	memset(payload, 0xA, size);

	dessert_meshsend_fast(hello_msg, NULL);
	dessert_msg_destroy(hello_msg);
	return 0;
}

int aodv_periodic_cleanup_database(void *data, struct timeval *scheduled, struct timeval *interval) {
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        if (aodv_db_cleanup(&timestamp)) return 0;
        else return 1;
}

dessert_msg_t* aodv_create_rerr(_onlb_element_t** head, uint16_t count) {
        if (*head == NULL) return NULL;
        dessert_msg_t* msg;
        dessert_ext_t* ext;
        dessert_msg_new(&msg);

        // set ttl
        msg->ttl = 255;

        // add l25h header
        dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
        struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
        memcpy(rreq_l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
        memcpy(rreq_l25h->ether_dhost, ether_broadcast, ETH_ALEN);

        // add RERR ext
        dessert_msg_addext(msg, &ext, RERR_EXT_TYPE, sizeof(struct aodv_msg_rerr));
        struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) ext->data;
        rerr_msg->flags = AODV_FLAGS_RERR_N;

        // write addresses of all my mesh interfaces
        dessert_meshif_t* iface = dessert_meshiflist_get();
        void* ifaceaddr_pointer = rerr_msg->ifaces;
        uint8_t ifaces_count = 0;
        while (iface != NULL && ifaces_count < MAX_MESH_IFACES_COUNT) {
                memcpy(ifaceaddr_pointer, iface->hwaddr, ETH_ALEN);
                ifaceaddr_pointer += ETH_ALEN;
                iface = iface->next;
                ifaces_count++;
        }

        rerr_msg->iface_addr_count = ifaces_count;

        // write addresses of affected destinations in RERRDL_EXT
        uint8_t rerrdl_count = 0;
        uint8_t ext_el_num = 0;
        void* dl = NULL;
        uint8_t max_dl_len = DESSERT_MAXEXTDATALEN / ETH_ALEN;
        uint8_t MAX_RERRDL_COUNT = (dessert_maxlen -
                        sizeof(dessert_msg_t) - sizeof(struct ether_header) -
                        sizeof(struct aodv_msg_rerr) - sizeof(struct aodv_msg_broadcast)) / DESSERT_MAXEXTDATALEN;

        while(*head != NULL && rerrdl_count < MAX_RERRDL_COUNT) {
                if (ext_el_num == 0) {
                        dessert_msg_addext(msg, &ext, RERRDL_EXT_TYPE, ((count >= max_dl_len)? max_dl_len : count) * ETH_ALEN);
                        dl = ext->data;
                }

                _onlb_element_t* el = *head;
                memcpy(dl, el->dhost_ether, ETH_ALEN);
                dl += ETH_ALEN;
                ext_el_num++;

                if (ext_el_num == max_dl_len) {
                        rerrdl_count++;
                        ext_el_num = 0;
                }

                DL_DELETE(*head, el);
                count--;
                free(el);
        }

        // add broadcast id ext since RERR is an broadcast message
        dessert_msg_addext(msg, &ext, BROADCAST_EXT_TYPE, sizeof(struct aodv_msg_broadcast));
        struct aodv_msg_broadcast* brc_str = (struct aodv_msg_broadcast*) ext->data;
        pthread_rwlock_wrlock(&pp_rwlock);
        brc_str->id = ++broadcast_id;
        pthread_rwlock_unlock(&pp_rwlock);

        return msg;
}

int aodv_periodic_scexecute(void *data, struct timeval *scheduled, struct timeval *interval) {
        uint8_t schedule_type;
        uint64_t schedule_param;
        uint8_t ether_addr[ETH_ALEN];
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);

        if (aodv_db_popschedule(&timestamp, ether_addr, &schedule_type, &schedule_param) == FALSE) {
                return 0;
        }

        if (schedule_type == AODV_SC_SEND_OUT_PACKET) {
                //do nothing
        }
        else if (schedule_type == AODV_SC_REPEAT_RREQ) aodv_send_rreq(ether_addr, &timestamp, schedule_param);	// send out rreq
        else if (schedule_type == AODV_SC_SEND_OUT_RERR) {
                uint32_t rerr_count;
                aodv_db_getrerrcount(&timestamp, &rerr_count);
                if (rerr_count < RERR_RATELIMIT) {
                        uint16_t dest_count = 0;
                        uint8_t dhost_ether[ETH_ALEN];
                        _onlb_element_t* curr_el = NULL;
                        _onlb_element_t* head = NULL;

                        while(aodv_db_invroute(ether_addr, dhost_ether) == TRUE) {
                                dessert_debug("invalidate route to " MAC, EXPLODE_ARRAY6(dhost_ether));
                                dest_count++;
                                curr_el = malloc(sizeof(_onlb_element_t));
                                memcpy(curr_el->dhost_ether, dhost_ether, ETH_ALEN);
                                curr_el->next = curr_el->prev = NULL;
                                DL_APPEND(head, curr_el);
                        }

                        if (dest_count > 0) {
                                while(head != NULL) {
                                        dessert_msg_t* rerr_msg = aodv_create_rerr(&head, dest_count);
                                        if (rerr_msg != NULL) {
                                                dessert_debug("link to " MAC " break -> send RERR", EXPLODE_ARRAY6(dhost_ether));
                                                dessert_meshsend_fast(rerr_msg, NULL);
                                                dessert_msg_destroy(rerr_msg);
                                                aodv_db_putrerr(&timestamp);
                                        }
                                }
                        }
                }
        }
        return 0;
}
