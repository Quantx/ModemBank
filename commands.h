#define COMMAND_COUNT 1

void  (* const command_list[ COMMAND_COUNT]) (user * muser, int argc, char * argv[]);
const char *   command_alias[COMMAND_COUNT];

void cmd_help(user * muser, int argc, char * argv[]);
