#include "../../config.h"
#include "../timeslot.h"

timeslot_t* rreq_log_ts 	= NULL;
u_int32_t rreq_count 		= 0;
void* rreq_pseudo_pointer 	= 0;

void rreq_decrement_counter(struct timeval* timestamp, void* src_object, void* object) {
	rreq_count--;
}

int aodv_db_rl_init() {
	struct timeval timeout;
	timeout.tv_sec = 1;			// 1 sek timeout since we are interested for number of sent RREQ in last 1 sek
	timeout.tv_usec = 0;
	return timeslot_create(&rreq_log_ts, &timeout, NULL, rreq_decrement_counter);
}

void aodv_db_rl_putrreq(struct timeval* timestamp) {
	if (timeslot_addobject(rreq_log_ts, timestamp, rreq_pseudo_pointer++) == TRUE) rreq_count++;
}

void aodv_db_rl_getrreqcount(struct timeval* timestamp, u_int32_t* count_out) {
	timeslot_purgeobjects(rreq_log_ts, timestamp);
	*count_out = rreq_count;
}
