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

int aodv_gossip_0(){
    return (random() < (((long double) gossip_p)*((long double) RAND_MAX)));
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
        default: {
            dessert_crit("unknown gossip type -> using GOSSIP_NONE as fallback");
            return true;
        }
    }
}
