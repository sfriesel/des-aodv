#include <libcli.h>

int cli_multipath(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_beverbose(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_sethellosize(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showmultipath(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showhellosize(struct cli_def* cli, char* command, char* argv[], int argc);
int aodv_cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int aodv_cli_sendrreq(struct cli_def* cli, char* command, char* argv[], int argc);
int aodv_cli_sendping(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_cfgsysif(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_addmeshif(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_setport(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_setrouting_log(struct cli_def *cli, char *command, char *argv[], int argc);

