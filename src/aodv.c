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
#ifndef ANDROID
#include <printf.h>
#endif
#include <dessert-extra.h>

#include "config.h"
#include "cli/aodv_cli.h"
#include "pipeline/aodv_pipeline.h"
#include "database/aodv_database.h"

int     hello_size	        = HELLO_SIZE;
int     hello_interval      = HELLO_INTERVAL;
int     rreq_size	        = RREQ_SIZE;

dessert_periodic_t* periodic_send_hello;

int main(int argc, char** argv) {
    /* initialize daemon with correct parameters */
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
            dessert_info("starting AODV in daemonize mode");
            cfg = dessert_cli_get_cfg(argc, argv);
            dessert_init("AODV", 0x03, DESSERT_OPT_DAEMONIZE);
    }

    /* routing table initialization */
    aodv_db_init();

    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

    /* cli initialization */
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

    struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
    cli_register_command(dessert_cli, cli_cfg_set, "hello_size", cli_set_hello_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet size");
    cli_register_command(dessert_cli, cli_cfg_set, "hello_interval", cli_set_hello_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet interval");
    cli_register_command(dessert_cli, cli_cfg_set, "rreq_size", cli_set_rreq_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set RREQ packet size");

    cli_register_command(dessert_cli, dessert_cli_show, "hello_size", cli_show_hello_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "hello_interval", cli_show_hello_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet interval");
    cli_register_command(dessert_cli, dessert_cli_show, "rreq_size", cli_show_rreq_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show RREQ packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "rt", cli_show_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing table");

    cli_register_command(dessert_cli, NULL, "send_rreq", cli_send_rreq, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "send RREQ to destination");

    /* registering callbacks */
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
    dessert_meshrxcb_add(aodv_drop_errors, 30);
    dessert_meshrxcb_add(aodv_handle_hello, 40);
    dessert_meshrxcb_add(aodv_handle_rreq, 50);
    dessert_meshrxcb_add(aodv_handle_rerr, 60);
    dessert_meshrxcb_add(aodv_handle_rrep, 70);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 75);
    dessert_meshrxcb_add(aodv_forward_broadcast, 80);
    dessert_meshrxcb_add(aodv_forward_multicast, 81);
    dessert_meshrxcb_add(aodv_forward, 90);
    dessert_meshrxcb_add(rp2sys, 100);

    dessert_sysrxcb_add(aodv_sys2rp, 10);

    /* registering periodic tasks */
    struct timeval hello_interval_t;
    hello_interval_t.tv_sec = hello_interval / 1000;
    hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
    periodic_send_hello = dessert_periodic_add(aodv_periodic_send_hello, NULL, NULL, &hello_interval_t);

    struct timeval cleanup_interval;
    cleanup_interval.tv_sec = DB_CLEANUP_INTERVAL / 1000;
    cleanup_interval.tv_usec = (DB_CLEANUP_INTERVAL % 1000) * 1000;
    dessert_periodic_add(aodv_periodic_cleanup_database, NULL, NULL, &cleanup_interval);

    struct timeval schedule_chec_interval;
    schedule_chec_interval.tv_sec = SCHEDULE_CHECK_INTERVAL / 1000;
    schedule_chec_interval.tv_usec = (SCHEDULE_CHECK_INTERVAL % 1000) * 1000;
    dessert_periodic_add(aodv_periodic_scexecute, NULL, NULL, &schedule_chec_interval);

    /* running cli & daemon */
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_cli_run();
    dessert_run();

    return EXIT_SUCCESS;
}
