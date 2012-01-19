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
#include <string.h>
#include <utlist.h>
#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include "../helper.h"

uint32_t seq_num_global = 0;
pthread_rwlock_t pp_rwlock = PTHREAD_RWLOCK_INITIALIZER;

// ---------------------------- help functions ---------------------------------------

dessert_msg_t* _create_rreq(uint8_t dhost_ether[ETH_ALEN], uint8_t ttl, metric_t initial_metric) {
    dessert_msg_t* msg;
    dessert_ext_t* ext;
    dessert_msg_new(&msg);

    msg->ttl = ttl;
    msg->u8 = 0; /*hop count */

    // add l25h header
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
    memcpy(rreq_l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
    memcpy(rreq_l25h->ether_dhost, dhost_ether, ETH_ALEN);

    // add RREQ ext
    dessert_msg_addext(msg, &ext, RREQ_EXT_TYPE, sizeof(struct aodv_msg_rreq));
    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) ext->data;
    msg->u16 = initial_metric;
    rreq_msg->flags = 0;
    pthread_rwlock_wrlock(&pp_rwlock);
    rreq_msg->originator_sequence_number = ++seq_num_global;
    pthread_rwlock_unlock(&pp_rwlock);

    //this is for local repair, we know that the latest rrep we saw was last_destination_sequence_number
    uint32_t last_destination_sequence_number;

    if(aodv_db_get_destination_sequence_number(dhost_ether, &last_destination_sequence_number) != true) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_U;
    }

    if(dest_only) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_D;
    }

    rreq_msg->destination_sequence_number = last_destination_sequence_number;

    int d = aodv_db_get_warn_status(dhost_ether);

    if(d == true) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_D;
    }

    dessert_debug("create rreq to " MAC ": o=%" PRIu32 " d=%" PRIu32 "", EXPLODE_ARRAY6(dhost_ether), rreq_msg->originator_sequence_number, rreq_msg->destination_sequence_number);

    dessert_msg_dummy_payload(msg, rreq_size);

    return msg;
}

dessert_msg_t* _create_rrep(uint8_t route_dest[ETH_ALEN], uint8_t route_source[ETH_ALEN], uint8_t rrep_next_hop[ETH_ALEN], uint32_t destination_sequence_number, uint8_t flags, metric_t initial_metric) {
    dessert_msg_t* msg;
    dessert_ext_t* ext;
    dessert_msg_new(&msg);

    msg->ttl = TTL_MAX;
    msg->u8 = 0; /*hop count */

    // add l25h header
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
    memcpy(rreq_l25h->ether_shost, route_dest, ETH_ALEN);
    memcpy(rreq_l25h->ether_dhost, route_source, ETH_ALEN);

    // set next hop
    memcpy(msg->l2h.ether_dhost, rrep_next_hop, ETH_ALEN);

    // and add RREP ext
    dessert_msg_addext(msg, &ext, RREP_EXT_TYPE, sizeof(struct aodv_msg_rrep));
    struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) ext->data;
    rrep_msg->flags = flags;
    msg->u16 = initial_metric;
    rrep_msg->lifetime = 0;
    rrep_msg->destination_sequence_number = destination_sequence_number;
    return msg;
}

void aodv_send_rreq(mac_addr dhost_ether, struct timeval* ts, struct aodv_retry_rreq* retry, metric_t initial_metric) {
    // reschedule if we sent more than RREQ_LIMIT RREQ messages in the last second
    uint32_t rreq_count;
    aodv_db_getrreqcount(ts, &rreq_count);

    if(rreq_count > RREQ_RATELIMIT) {
        dessert_trace("we have reached RREQ_RATELIMIT");
        aodv_db_addschedule(ts, dhost_ether, AODV_SC_REPEAT_RREQ, retry);
        return;
    }

    dessert_msg_t* msg;
    if(!retry) {
        if(aodv_db_schedule_exists(dhost_ether, AODV_SC_REPEAT_RREQ)) {
            dessert_trace("there is a rreq_schedule to this dest we dont start a new series");
            return;
        }

        uint8_t ttl = ring_search ? TTL_START : NET_DIAMETER;
        msg = _create_rreq(dhost_ether, ttl, initial_metric);
    }

    if(ring_search && msg->ttl > TTL_THRESHOLD) {
        dessert_debug("RREQ to " MAC ": TTL_THRESHOLD is reached - send RREQ with NET_DIAMETER=%" PRIu8 "", EXPLODE_ARRAY6(dhost_ether), NET_DIAMETER);
        msg->ttl = NET_DIAMETER;
    }

    dessert_ext_t* ext;
    dessert_msg_getext(msg, &ext, RREQ_EXT_TYPE, 0);
    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) ext->data;
    pthread_rwlock_wrlock(&pp_rwlock);
    rreq_msg->originator_sequence_number = ++seq_num_global;
    pthread_rwlock_unlock(&pp_rwlock);

    dessert_debug("RREQ send for " MAC " ttl=%" PRIu8 " id=%" PRIu8 "", EXPLODE_ARRAY6(dhost_ether), msg->ttl, rreq_msg->originator_sequence_number);
    dessert_meshsend(msg, NULL);
    aodv_db_putrreq(ts);

    if(!retry) {
        retry = malloc(sizeof(*retry));
        retry->msg = msg;
        retry->count = 0;
    }
    retry->count++;

    if(retry->count > RREQ_RETRIES) {
        /* RREQ has been tried for the max. number of times -- give up */
        dessert_msg_destroy(retry->msg);
        free(retry);
        return;
    }

    /* repeat_time corresponds to NET_TRAVERSAL_TIME if ring_search is off */
    uint32_t repeat_time = 2 * NODE_TRAVERSAL_TIME * msg->ttl;
    dessert_trace("add task to repeat RREQ");
    if(ring_search) {
        msg->ttl += TTL_INCREMENT;
        if(msg->ttl > TTL_THRESHOLD) {
            msg->ttl = NET_DIAMETER;
        }
    }

    struct timeval rreq_repeat_time = { repeat_time / 1000, (repeat_time % 1000) * 1000 };
    hf_add_tv(ts, &rreq_repeat_time, &rreq_repeat_time);

    aodv_db_addschedule(&rreq_repeat_time, dhost_ether, AODV_SC_REPEAT_RREQ, retry);
}

// ---------------------------- pipeline callbacks ---------------------------------------------

int aodv_drop_errors(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    // drop packets sent by myself.
    if(proc->lflags & DESSERT_RX_FLAG_L2_SRC) {
        return DESSERT_MSG_DROP;
    }

    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
        return DESSERT_MSG_DROP;
    }

    /**
     * First check neighborhood, since it is possible that one
     * RREQ from one unidirectional neighbor can be added to broadcast id table
     * and then dropped as a message from unidirectional neighbor!
     */
    dessert_ext_t* ext;
    struct timeval ts;
    gettimeofday(&ts, NULL);

    // check whether control messages were sent over bidirectional links, otherwise DROP
    // Hint: RERR must be resent in both directions.
    if((dessert_msg_getext(msg, &ext, RREQ_EXT_TYPE, 0) != 0) || (dessert_msg_getext(msg, &ext, RREP_EXT_TYPE, 0) != 0)) {
        if(aodv_db_check2Dneigh(msg->l2h.ether_shost, iface, &ts) != true) {
            dessert_debug("DROP RREQ/RREP from " MAC " metric=%" AODV_PRI_METRIC " hop_count=%" PRIu8 " ttl=%" PRIu8 "-> neighbor is unidirectional!", EXPLODE_ARRAY6(msg->l2h.ether_shost), msg->u16, msg->u8, msg->ttl);
            return DESSERT_MSG_DROP;
        }
    }

    return DESSERT_MSG_KEEP;
}

int aodv_handle_hello(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* hallo_ext;

    if(dessert_msg_getext(msg, &hallo_ext, HELLO_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    struct aodv_msg_hello* hello_msg = (struct aodv_msg_hello*) hallo_ext->data;

    struct timeval ts;
    gettimeofday(&ts, NULL);

    msg->ttl--;

    if(msg->ttl >= 1) {
        // hello req
        uint8_t rcvd_hellos = 0;
        aodv_db_pdr_cap_hello(msg->l2h.ether_shost, msg->u16, hello_msg->hello_interval, &ts);
        if(aodv_db_pdr_get_rcvdhellocount(msg->l2h.ether_shost, &rcvd_hellos, &ts) == true) {
            hello_msg->hello_rcvd_count = rcvd_hellos;
        }
        hello_msg->hello_interval = hello_interval; 
        memcpy(msg->l2h.ether_dhost, msg->l2h.ether_shost, ETH_ALEN);
        dessert_meshsend(msg, iface);
        // dessert_trace("got hello-req from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_shost));
    }
    else {
        //hello rep
        if(memcmp(iface->hwaddr, msg->l2h.ether_dhost, ETH_ALEN) == 0) {
            aodv_db_pdr_cap_hellorsp(msg->l2h.ether_shost, hello_msg->hello_interval, hello_msg->hello_rcvd_count, &ts);
            // dessert_trace("got hello-rep from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_dhost));
            aodv_db_cap2Dneigh(msg->l2h.ether_shost, msg->u16, iface, &ts);
        }
    }

    return DESSERT_MSG_DROP;
}

int aodv_handle_rreq(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rreq_ext;

    if(dessert_msg_getext(msg, &rreq_ext, RREQ_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) rreq_ext->data;

    dessert_debug("incoming RREQ from " MAC " over " MAC " to " MAC " seq=%i ttl=%d", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost),  EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->originator_sequence_number, msg->ttl);

    if(gossip_type == GOSSIP_NONE && msg->ttl == 0) {
        return DESSERT_MSG_DROP;
    }

    msg->ttl--;
    msg->u8++; /* hop count */

    struct timeval ts;

    gettimeofday(&ts, NULL);

    /********** METRIC *************/
    aodv_metric_do(&(msg->u16), msg->l2h.ether_shost, iface, &ts);
    /********** METRIC *************/

    int x = aodv_db_capt_rreq(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, iface, rreq_msg->originator_sequence_number, msg->u16, msg->u8, &ts);

    if(x == false) {
        dessert_debug("got RREQ for " MAC " from " MAC " seq=%" PRIu32 " hop=%" PRIu8 " ttl=%" PRIu8 " -> drop it: it is OLD", EXPLODE_ARRAY6(l25h->ether_dhost), EXPLODE_ARRAY6(l25h->ether_shost), rreq_msg->originator_sequence_number, msg->u16, msg->ttl);
        return DESSERT_MSG_DROP;
    }

    if(memcmp(dessert_l25_defsrc, l25h->ether_dhost, ETH_ALEN) != 0) {  // RREQ not for me

        dessert_trace("incoming RREQ from " MAC " to " MAC " originator_sequence_number=%" PRIu32 "", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->originator_sequence_number);

        int d = (rreq_msg->flags & AODV_FLAGS_RREQ_D);
        int u = (rreq_msg->flags & AODV_FLAGS_RREQ_U);
        uint32_t last_destination_sequence_number;
        int s = aodv_db_get_destination_sequence_number(l25h->ether_dhost, &last_destination_sequence_number);
        int hs = hf_comp_u32(rreq_msg->destination_sequence_number, last_destination_sequence_number);

        if(!d && !u && s && hs < 0) {
            /*
             * - RREQ is not destination_only
             * - rreq_src has valid sequence number for dest
             * - we have a valid sequence number for dest (we have a route)
             * - our sequence number is newer (our route is newer)
             */
            metric_t last_metric_orginator;
            aodv_db_get_orginator_metric(l25h->ether_dhost, l25h->ether_shost, &last_metric_orginator);
            dessert_msg_t* rrep_msg = _create_rrep(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, last_destination_sequence_number /*this is what we know*/ , AODV_FLAGS_RREP_A, last_metric_orginator);
            dessert_debug("repair link to " MAC, EXPLODE_ARRAY6(l25h->ether_dhost));

            dessert_meshsend(rrep_msg, iface);
            dessert_msg_destroy(rrep_msg);
        }
        else {
            if(gossip_type == GOSSIP_NONE || aodv_gossip(msg)) {
                dessert_debug("route to " MAC " originator_sequence_number=%" PRIu32 " is unknown for me OR we can't repair -> rebroadcast RREQ", EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->originator_sequence_number);
                dessert_meshsend(msg, NULL);
            }
            else {
                dessert_debug("stop RREQ to " MAC " id=%" PRIu32 "", EXPLODE_ARRAY6(l25h->ether_dhost), rreq_msg->originator_sequence_number);
            }
        }
    }
    else {   // RREQ for me
        pthread_rwlock_wrlock(&pp_rwlock);
        uint32_t destination_sequence_number_copy = ++seq_num_global;
        pthread_rwlock_unlock(&pp_rwlock);

        dessert_debug("incoming RREQ from " MAC " over " MAC " for me originator_sequence_number=%" PRIu32 " -> answer with RREP destination_sequence_number_copy=%" PRIu32 "",
                      EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), rreq_msg->originator_sequence_number, destination_sequence_number_copy);

        dessert_msg_t* rrep_msg = _create_rrep(dessert_l25_defsrc, l25h->ether_shost, msg->l2h.ether_shost, destination_sequence_number_copy, AODV_FLAGS_RREP_A, metric_startvalue);
        dessert_meshsend(rrep_msg, iface);
        dessert_msg_destroy(rrep_msg);

    }
    /* RREQ gives route to his source. Process RREQ also as RREP */
    int y = aodv_db_capt_rrep(l25h->ether_shost, msg->l2h.ether_shost, iface, 0 /* force */, msg->u16, msg->u8, &ts);
    if(y == true) {
        dessert_debug("new route to: " MAC, EXPLODE_ARRAY6(l25h->ether_shost));
        // no need to search for next hop. Next hop is RREQ.msg->l2h.ether_shost
        aodv_send_packets_from_buffer(l25h->ether_shost, msg->l2h.ether_shost, iface);
    }
    else {
        dessert_debug("we know a better route: " MAC, EXPLODE_ARRAY6(l25h->ether_shost));
    }
    return DESSERT_MSG_DROP;
}

int aodv_handle_rerr(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rerr_ext;

    if(dessert_msg_getext(msg, &rerr_ext, RERR_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) rerr_ext->data;

    dessert_debug("got RERR: flags=%" PRIu8 "",  rerr_msg->flags);

    int rerrdl_num = 0;

    int rebroadcast_rerr = false;

    dessert_ext_t* rerrdl_ext;

    while(dessert_msg_getext(msg, &rerrdl_ext, RERRDL_EXT_TYPE, rerrdl_num++) > 0) {
        struct aodv_mac_seq* iter;

        for(iter = (struct aodv_mac_seq*) rerrdl_ext->data;
            iter < (struct aodv_mac_seq*) rerrdl_ext->data + rerrdl_ext->len;
            ++iter) {

            uint8_t dhost_ether[ETH_ALEN];
            memcpy(dhost_ether, iter->host, ETH_ALEN);
            uint32_t destination_sequence_number = iter->sequence_number;

            uint8_t dhost_next_hop[ETH_ALEN];

            if(!aodv_db_getnexthop(dhost_ether, dhost_next_hop)) {
                continue;
            }

            // if found, compare with entrys in interface-list this RRER.
            // If equals then this this route is affected and must be invalidated!
            int iface_num;

            for(iface_num = 0; iface_num < rerr_msg->iface_addr_count; iface_num++) {
                if(memcmp(rerr_msg->ifaces + iface_num * ETH_ALEN, dhost_next_hop, ETH_ALEN) == 0) {
                    rebroadcast_rerr |= aodv_db_markrouteinv(dhost_ether, destination_sequence_number);
                }
            }
        }
    }

    if(rebroadcast_rerr) {
        // write addresses of all my mesh interfaces
        dessert_meshif_t* iface = dessert_meshiflist_get();
        rerr_msg->iface_addr_count = 0;
        void* iface_addr_pointer = rerr_msg->ifaces;

        while(iface != NULL && rerr_msg->iface_addr_count < MAX_MESH_IFACES_COUNT) {
            memcpy(iface_addr_pointer, iface->hwaddr, ETH_ALEN);
            iface_addr_pointer += ETH_ALEN;
            iface = iface->next;
            rerr_msg->iface_addr_count++;
        }

        dessert_meshsend(msg, NULL);
    }

    return DESSERT_MSG_DROP;
}

int aodv_handle_rrep(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rrep_ext;

    if(dessert_msg_getext(msg, &rrep_ext, RREP_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    if(memcmp(msg->l2h.ether_dhost, iface->hwaddr, ETH_ALEN) != 0) {
        return DESSERT_MSG_DROP;
    }

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    dessert_debug("incoming RREP from " MAC " over " MAC " to " MAC " ttl=%d", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), msg->ttl);


    if(msg->ttl <= 0) {
        dessert_debug("got RREP from " MAC " but TTL is <= 0", EXPLODE_ARRAY6(l25h->ether_dhost));
        return DESSERT_MSG_DROP;
    }

    msg->ttl--;
    msg->u8++; /* hop count */

    struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) rrep_ext->data;

    struct timeval ts;

    gettimeofday(&ts, NULL);

    /********** METRIC *************/
    aodv_metric_do(&(msg->u16), msg->l2h.ether_shost, iface, &ts);
    /********** METRIC *************/

    int x = aodv_db_capt_rrep(l25h->ether_shost, msg->l2h.ether_shost, iface, rrep_msg->destination_sequence_number, msg->u16, msg->u8, &ts);

    if(x != true) {
        // capture and re-send only if route is unknown OR
        // sequence number is greater then that in database OR
        // if seq_nums are equals and known metric is greater than that in RREP -- METRIC
        return DESSERT_MSG_DROP;
    }

    if(memcmp(dessert_l25_defsrc, l25h->ether_dhost, ETH_ALEN) != 0) {
        // RREP is not for me -> send RREP to RREQ originator
        uint8_t next_hop[ETH_ALEN];
        dessert_meshif_t* output_iface;

        int a = aodv_db_getprevhop(l25h->ether_shost, l25h->ether_dhost, next_hop, &output_iface);

        if(a == true) {
            dessert_debug("re-send RREP to " MAC, EXPLODE_ARRAY6(l25h->ether_dhost));
            memcpy(msg->l2h.ether_dhost, next_hop, ETH_ALEN);
            dessert_meshsend(msg, output_iface);
        }
        else {
            dessert_debug("drop RREP to " MAC " we don't know the route back?!?!", EXPLODE_ARRAY6(l25h->ether_dhost));
        }
    }
    else {
        // this RREP is for me! -> pop all packets from FIFO buffer and send to destination
        dessert_debug("got RREP from " MAC " destination_sequence_number=%" PRIu32 " -> aodv_send_packets_from_buffer", EXPLODE_ARRAY6(l25h->ether_shost), rrep_msg->destination_sequence_number);
        /* no need to search for next hop. Next hop is RREP.prev_hop */
        aodv_send_packets_from_buffer(l25h->ether_shost, msg->l2h.ether_shost, iface);
    }

    return DESSERT_MSG_DROP;
}

