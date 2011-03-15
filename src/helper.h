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

#ifndef HELPER
#define HELPER

#ifdef ANDROID
#include <sys/time.h>
#endif

#include <linux/if_ether.h>
#include <stdlib.h>

/**
 * Compares two integers
 * returns 0 if i = j
 * return 1 if i > j (cirlce diff < (MAX_INT / 2))
 * return -1 if i < j (circle diff > (MAX_INT / 2))
 */
int hf_seq_comp_i_j(u_int32_t i, u_int32_t j);

/**
 * Compares two timevals.
 * Returns 0 if tv1 = tv2
 * Return 1 if  tv1 > tv2
 * Return -1 if tv1 < tv2
 */
int hf_compare_tv(struct timeval* tv1, struct timeval* tv2);

/**
 * Return summ of two timevals
 */
int hf_add_tv(struct timeval* tv1, struct timeval* tv2, struct timeval* sum);

#endif
