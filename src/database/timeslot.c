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

#include "timeslot.h"
#include <dessert.h>
#include <stdio.h>
#include <time.h>
#include <utlist.h>
#include <stdlib.h>
#undef assert
#include <assert.h>

typedef struct timeslot_element ts_element;

struct timeslot_element {
    struct timeval purge_time;
    void          *object; //key
    ts_element    *prev;
    ts_element    *next;
};

struct timeslot {
    struct timeval   default_lifetime;
    object_purger_t *object_purger;
    void            *src_object;
    ts_element      *elements; /* sorted in DESCENDING purge_time */
};

timeslot_t* timeslot_create(struct timeval* default_lifetime, void* src_object, object_purger_t* object_purger) {
    timeslot_t* ts = malloc(sizeof(*ts));
    assert(ts);

    ts->object_purger = object_purger;
    ts->default_lifetime = *default_lifetime;
    ts->src_object = src_object;
    ts->elements = NULL;

    return ts;
}

void timeslot_destroy(timeslot_t* ts) {
    while(ts->elements) {
        ts_element *tmp = ts->elements;
        DL_DELETE(ts->elements, tmp);
        free(tmp);
    }

    free(ts);
}

void timeslot_purgeobjects(timeslot_t* ts, struct timeval const *curr_time) {
    while(ts->elements) {
        ts_element *earliest = ts->elements->prev;
        if(dessert_timevalcmp(&earliest->purge_time, curr_time) > 0) {
            break;
        }

        if(ts->object_purger) {
            ts->object_purger(&earliest->purge_time, ts->src_object, earliest->object);
        }

        DL_DELETE(ts->elements, earliest);
        free(earliest);
    }
}

int timeslot_deleteobject(timeslot_t *ts, void *object) {
    // first find element with *object pointer
    ts_element *el;
    
    DL_FOREACH(ts->elements, el) {
        if(el->object == object) {
            DL_DELETE(ts->elements, el);
            free(el);
            return true;
        }
    }

    return false;
}

int timeslot_addobject_varpurge(timeslot_t* ts, struct timeval const *timestamp, void *object, struct timeval *lifetime) {
    timeslot_deleteobject(ts, object);
    ts_element* new_el = malloc(sizeof(*new_el));
    assert(new_el);
    dessert_timevaladd2(&new_el->purge_time, timestamp, lifetime);
    new_el->object = object;

    /* new elements will usually go to the start of the list (descending order) */
    if(!ts->elements || dessert_timevalcmp(&new_el->purge_time, &ts->elements->purge_time) >= 0) {
        DL_PREPEND(ts->elements, new_el);
        return true;
    }

    assert(ts->elements);

    ts_element *a = ts->elements;
    ts_element *b = a->next;
    while(b && dessert_timevalcmp(&new_el->purge_time, &b->purge_time) < 0) {
        a = b;
        b = b->next;
    }
    assert(a);

    a->next = new_el;
    new_el->next = b;
    new_el->prev = a;
    if(b) {
        b->prev = new_el;
    }
    else {
        ts->elements->prev = new_el;
    }

    return true;
}

int timeslot_addobject(timeslot_t* ts, struct timeval const *timestamp, void* object) {
    return timeslot_addobject_varpurge(ts, timestamp, object, &ts->default_lifetime);
}

void timeslot_report(timeslot_t* ts, char** str_out) {
    int size = 128;
    int used = 0;
    int tmp;
    char *buf = malloc(size);
    assert(buf);
    buf[0] = '\0';
    *str_out = buf;

    if(!ts) {
        snprintf(buf + used, size - used, "Time Slot: NULL\n");
        return;
    }

    if(!ts->elements) {
        snprintf(buf + used, size - used, "Time Slot: EMPTY\n");
        return;
    }

    tmp = snprintf(buf + used, size - used, "---------- Time Slot  -------------\n");
    assert(tmp >= 0 && tmp < size - used);
    used += tmp;

    ts_element* el;
    DL_FOREACH(ts->elements, el) {
        goto print; //first, try to print without realloc-ing
        do {
            size *= 2;
            buf = realloc(buf, size);
            assert(buf);
            *str_out = buf;
            print:
            tmp = snprintf(buf + used, size - used, "element       : %ld.%.6ld\n", el->purge_time.tv_sec, el->purge_time.tv_usec);
            assert(tmp >= 0);
        } while(tmp > size - used);
        used += tmp;
    }
}

