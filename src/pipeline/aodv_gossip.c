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

#include "../config.h"
#include "../helper.h"
#include "../database/packet_buffer/packet_buffer.h"
#include "../database/routing_table/aodv_rt.h"
#include "aodv_pipeline.h"

fifo_list_t* rreq_list;

int aodv_gossip_0(){
    return (random() < (((long double) gossip_p)*((long double) RAND_MAX)));
}

void aodv_gossip_3(){
	dessert_msg_t* msg = fl_pop_packet(rreq_list);
	if (msg == NULL) {
		dessert_crit("rreq_list is null - but there was a schedule");
//		return 0;
	}
    dessert_ext_t* rreq_ext;
    dessert_msg_getext(msg, &rreq_ext, RREQ_EXT_TYPE, 0);
    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) rreq_ext->data;
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	uint32_t quantity;
	if (aodv_db_rt_get_quantity(l25h->ether_dhost, l25h->ether_shost, &quantity)) {
		if (quantity < 2) {
			 dessert_debug("incoming RREQ from " MAC " over " MAC " to " MAC " seq=%ju ttl=%ju | %s", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), (uintmax_t)rreq_msg->originator_sequence_number, (uintmax_t)msg->ttl, "send again (GOSSIP_3)");
			dessert_meshsend(msg, NULL);
		}
	}
//	return 1;
}

int aodv_gossip(dessert_msg_t* msg){
    switch(gossip_type) {
        case GOSSIP_NONE:
            return true;
        case GOSSIP_0:
            return aodv_gossip_0();
        case GOSSIP_1:
        	/* u8 hop count */
            return (msg->u8 <= 1) ? true : aodv_gossip_0();
        case GOSSIP_3:
        	/* init */
        	if (rreq_list == NULL) {
        		rreq_list = malloc(sizeof(fifo_list_t));
        		rreq_list->head = NULL;
        		rreq_list->tail = NULL;
        		rreq_list->size = 0;
        	}

        	if (!aodv_gossip_0()) {

				fl_push_packet(rreq_list, msg);
				/* look once in some usec */
				struct timeval schedule;
				gettimeofday(&schedule, NULL);
				dessert_timevaladd(&schedule, 0, 3 * NODE_TRAVERSAL_TIME);
				dessert_periodic_add(aodv_gossip_3, NULL, &schedule, NULL);
				dessert_debug("ok");
				return false;
        	}
        	return true;

        default: {
            dessert_crit("unknown gossip type -> using GOSSIP_NONE as fallback");
            return true;
        }
    }
}
