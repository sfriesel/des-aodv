#include <string.h>
#include <printf.h>
#include <dessert-extra.h>

#include "config.h"
#include "cli/aodv_cli.h"
#include "pipeline/aodv_pipeline.h"
#include "database/aodv_database.h"

int 	multipath 			= MULTIPATH;
int 	be_verbose 			= BE_VERBOSE;
char* 	routing_log_file 	= NULL;
int 	hello_size 			= HELLO_SIZE;

int print_macaddress_arginfo(const struct printf_info *info, size_t n, int *argtypes) {
    if (n > 0) argtypes[0] = PA_POINTER;  // We always take exactly one argument and this is a pointer to the structure..
    return 1;
}

int print_macaddress(FILE *stream, const struct printf_info *info, const void * const *args) {
    const uint8_t *address;
    int len;
    address = *(uint8_t **) (args[0]);
    len = fprintf(stream, "%02x:%02x:%02x:%02x:%02x:%02x", address[0], address[1], address[2], address[3], address[4], address[5]);
    if (len == -1) return -1;
    return len;
}

int main(int argc, char** argv) {
    register_printf_function("M", print_macaddress, print_macaddress_arginfo);

    /* initialize daemon with correct parameters */
    FILE *cfg = NULL;
	if (argc == 1) {
		dessert_info("starting DES-AODV in daemonize mode, auto config file");
		dessert_init("aodv", 0x03, DESSERT_OPT_DAEMONIZE);
		char cfg_file_name[] = "/etc/des-aodv.conf";
		cfg = fopen(cfg_file_name, "r");
		if (!cfg) {
			printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		if (argc == 2) {
			if (strcmp(argv[1], "-nondaemonize") == 0) {
				dessert_info("starting DES-AODV in nondaemonize mode, auto config file");
				dessert_init("aodv", 0x03, DESSERT_OPT_NODAEMONIZE);
				char cfg_file_name[] = "/etc/des-aodv.conf";
				cfg = fopen(cfg_file_name, "r");
				if (!cfg) {
					printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
					return EXIT_FAILURE;
				}
			} else {
				dessert_info("starting DES-AODV in daemonize mode, manual config file");
				dessert_init("aodv", 0x03, DESSERT_OPT_DAEMONIZE);
				cfg = dessert_cli_get_cfg(argc, argv);
			}
		} else {
			if (argc == 3) {
				dessert_info("starting DES-AODV in nondaemonize mode, manual config file");
				dessert_init("aodv", 0x03, DESSERT_OPT_NODAEMONIZE);
				cfg = dessert_cli_get_cfg(argc, argv);
			} else {
				dessert_info("USAGE: des-aodv [config file] [-nondaemonize]");
				return EXIT_FAILURE;
			}
		}
	}

	/* routing table initialization */
	aodv_db_init();

    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

	/* cli initialization */
	struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");
	cli_register_command(dessert_cli, cli_cfg_set, "verbose", cli_beverbose, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "be more verbose");
	cli_register_command(dessert_cli, cli_cfg_set, "routinglog", cli_setrouting_log, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set path to routing logging file");
	cli_register_command(dessert_cli, NULL, "rreq", aodv_cli_sendrreq, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "send RREQ to destination");
	cli_register_command(dessert_cli, NULL, "multipath", cli_multipath, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "activate/deactivate multipath");
	cli_register_command(dessert_cli, NULL, "hello_size", cli_sethellosize, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "set size of hello packet");

	struct cli_command* cli_command_print = cli_register_command(dessert_cli, NULL, "print", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print table");
	cli_register_command(dessert_cli, cli_command_print, "rt", aodv_cli_print_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print routing table");
	cli_register_command(dessert_cli, cli_command_print, "multipath", cli_showmultipath, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print whether multipath is set");
	cli_register_command(dessert_cli, cli_command_print, "hello_size", cli_showhellosize, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print hello packet size");

	/* registering callbacks */
	dessert_meshrxcb_add(dessert_msg_check_cb, 10); 		// check message
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20); 	// set lflags
	dessert_meshrxcb_add(aodv_drop_errors, 30);
	dessert_meshrxcb_add(aodv_handle_hello, 40);
	dessert_meshrxcb_add(aodv_handle_rreq, 50);
	dessert_meshrxcb_add(aodv_handle_rerr, 60);
	dessert_meshrxcb_add(aodv_handle_rrep, 70);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 75);
	dessert_meshrxcb_add(aodv_fwd2dest, 80);
	dessert_meshrxcb_add(rp2sys, 100);

	dessert_sysrxcb_add(aodv_sys2rp, 10);

	/* registering periodic tasks */
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

	/* running cli & daemon */
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	dessert_cli_run();
	dessert_run();

	return EXIT_SUCCESS;
}
