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

#include <dessert.h>
#include <string.h>
#include <time.h>
#include "aodv_cli.h"
#include "../database/aodv_database.h"
#include "../config.h"
#include "../pipeline/aodv_pipeline.h"

// -------------------- config ------------------------------------------------------------

int cli_beverbose(struct cli_def* cli, char* command, char* argv[], int argc) {
	u_int32_t mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1|| (mode != 0 && mode != 1)) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	if (mode == 1) {
		dessert_info("be verbose = TRUE");
		be_verbose = TRUE;
	} else {
		dessert_info("be verbose = FALSE");
		be_verbose = FALSE;
	}
	return CLI_OK;
}


// -------------------- Testing ------------------------------------------------------------

int aodv_cli_sendrreq(struct cli_def* cli, char* command, char* argv[], int argc) {
	u_int8_t dhost_hwaddr[ETHER_ADDR_LEN];
	if (argc < 1 || argc > 2 || sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			&dhost_hwaddr[0], &dhost_hwaddr[1], &dhost_hwaddr[2],
			&dhost_hwaddr[3], &dhost_hwaddr[4], &dhost_hwaddr[5]) != 6) { // args are not correct
		cli_print(cli, "usage of %s command [hardware address as XX:XX:XX:XX:XX:XX]\n", command);
		return CLI_ERROR_ARG;
	} else {
		// args are correct -> send rreq
		struct timeval ts;
		gettimeofday(&ts, NULL);
		aodv_send_rreq(dhost_hwaddr, &ts, TTL_START);
		return CLI_OK;
	}
}

int aodv_cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc){
	char* rt_report;
	aodv_db_view_routing_table(&rt_report);
	cli_print(cli, "\n%s\n", rt_report);
	free(rt_report);
	return CLI_OK;
}
// -------------------- common cli functions ----------------------------------------------

int cli_setrouting_log(struct cli_def *cli, char *command, char *argv[], int argc) {
	routing_log_file = malloc(strlen(argv[0]));
	strcpy(routing_log_file, argv[0]);
	FILE* f = fopen(routing_log_file, "a+");
	time_t lt;
	lt = time(NULL);
	fprintf(f, "\n--- %s\n", ctime(&lt));
	fclose(f);
	dessert_info("logging routing data at file %s", routing_log_file);
	return CLI_OK;
}
