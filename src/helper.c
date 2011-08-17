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

#include "helper.h"
#include "config.h"

/******************************************************************************/

int hf_comp_u8(uint8_t i, uint8_t j) {
    if(i == j) {
        return 0;
    }

    uint32_t diff = i - j;

    if(diff < (UINT8_MAX >> 1)) {
        return 1;
    }

    return -1;
}

int hf_comp_u16(uint16_t i, uint16_t j) {
    if(i == j) {
        return 0;
    }

    uint16_t diff = i - j;

    if(diff < (UINT16_MAX >> 1)) {
        return 1;
    }

    return -1;
}


int hf_comp_u32(uint32_t i, uint32_t j) {
    if(i == j) {
        return 0;
    }

    uint32_t diff = i - j;

    if(diff < (UINT32_MAX >> 1)) {
        return 1;
    }

    return -1;
}

/******************************************************************************/

int hf_comp_metric(metric_t i, metric_t j) {
    if(i == j) {
        return 0;
    }

    metric_t diff = i - j;

    if(diff < (AODV_MAX_METRIC >> 1)) {
        return 1;
    }

    return -1;
}

/******************************************************************************/

int hf_compare_tv(struct timeval* tv1, struct timeval* tv2) {
    if((tv1->tv_sec == tv2->tv_sec) && (tv1->tv_usec == tv2->tv_usec)) {
        return 0;
    }

    if(tv1->tv_sec > tv2->tv_sec) {
        return 1;
    }

    if(tv2->tv_sec > tv1->tv_sec) {
        return -1;
    }

    if(tv1->tv_usec > tv2->tv_usec) {
        return 1;
    }
    else {
        return -1;
    }
}

int hf_add_tv(struct timeval* tv1, struct timeval* tv2, struct timeval* sum) {
    sum->tv_sec = tv1->tv_sec + tv2->tv_sec;
    __suseconds_t usec_sum = tv1->tv_usec + tv2->tv_usec;

    if(usec_sum >= 1000000) {
        sum->tv_sec += 1;
        sum->tv_usec = usec_sum - 1000000;
    }
    else {
        sum->tv_usec = usec_sum;
    }

    return true;
}

/******************************************************************************/

/* rssi is typicaly in [-128, 0] */
uint8_t hf_rssi2interval(int8_t rssi) {

    if(rssi == 0) {
        return 8;
    }

    if(rssi > -40) {
        return 1;
    }

    if(rssi > -60) {
        return 2;
    }

    if(rssi > -70) {
        return 4;
    }

    return 8;
}
