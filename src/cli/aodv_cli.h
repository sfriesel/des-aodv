#include <libcli.h>

int cli_set_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_verbose(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_multipath(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_routing_log(struct cli_def *cli, char *command, char *argv[], int argc);

int cli_print_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_print_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_print_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_print_multipath(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_send_rreq(struct cli_def* cli, char* command, char* argv[], int argc);

