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

#include <string.h>
#include "cli/aodv_cli.h"
#include "pipeline/aodv_pipeline.h"
#include "database/aodv_database.h"
#include "config.h"
#include <dessert-extra.h>

int be_verbose = BE_VERBOSE;
int cli_port = 1023;
char* routing_log_file = NULL;

int main(int argc, char** argv) {
	FILE *cfg = NULL;
	if ((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
		dessert_info("starting AODV in non daemonize mode");
		dessert_init("AODV", 0x03, DESSERT_OPT_NODAEMONIZE);
		char cfg_file_name[] = "/etc/des-aodv.conf";
		cfg = fopen(cfg_file_name, "r");
		if (cfg == NULL) {
			printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		dessert_init("AODV", 0x03, DESSERT_OPT_DAEMONIZE);
		cfg = dessert_cli_get_cfg(argc, argv);
	}
	// routing table initialisation
	aodv_db_init();

    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

	// cli initialization
	struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
	cli_register_command(dessert_cli, cli_cfg_set, "port", cli_setport, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "configure TCP port the daemon is listening on");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");
	cli_register_command(dessert_cli, cli_cfg_set, "verbose", cli_beverbose, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "be more verbose");
	cli_register_command(dessert_cli, cli_cfg_set, "routinglog", cli_setrouting_log, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set path to routing logging file");
	cli_register_command(dessert_cli, NULL, "rreq", aodv_cli_sendrreq, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "send RREQ to destination");
	struct cli_command* cli_command_print =
			cli_register_command(dessert_cli, NULL, "print", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print table");
	cli_register_command(dessert_cli, cli_command_print, "rt", aodv_cli_print_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print routing table");

	// registering callbacks
	dessert_meshrxcb_add(dessert_msg_check_cb, 10); // check message
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20); // set lflags,
	dessert_meshrxcb_add(aodv_drop_errors, 30);
	dessert_meshrxcb_add(aodv_handle_hello, 40);
	dessert_meshrxcb_add(aodv_handle_rreq, 50);
	dessert_meshrxcb_add(aodv_handle_rerr, 60);
	dessert_meshrxcb_add(aodv_handle_rrep, 70);
	//dessert_meshrxcb_add(dessert_rx_trace, 75);
	dessert_meshrxcb_add(aodv_fwd2dest, 80);
	dessert_meshrxcb_add(rp2sys, 100);

	dessert_sysrxcb_add(aodv_sys2rp, 10);

	// registering periodic tasks
	struct timeval hello_interval;
	hello_interval.tv_sec = HELLO_INTERVAL / 1000;
	hello_interval.tv_usec = (HELLO_INTERVAL % 1000) * 1000;
	dessert_periodic_add(aodv_periodic_send_hello, NULL, NULL, &hello_interval);

	struct timeval cleanup_interval;
	cleanup_interval.tv_sec = DB_CLEANUP_INTERVAL / 1000;
	cleanup_interval.tv_usec = (DB_CLEANUP_INTERVAL % 1000) * 1000;
	dessert_periodic_add(aodv_periodic_cleanup_database, NULL, NULL, &cleanup_interval);

	struct timeval schedule_chec_interval;
	schedule_chec_interval.tv_sec = SCHEDULE_CHECK_INTERVAL / 1000;
	schedule_chec_interval.tv_usec = (SCHEDULE_CHECK_INTERVAL % 1000) * 1000;
	dessert_periodic_add(aodv_periodic_scexecute, NULL, NULL, &schedule_chec_interval);

	// configure
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);

	// DES-SERT and CLI Run!
	dessert_cli_run();
	dessert_run();

	return EXIT_SUCCESS;
}
