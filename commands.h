#define COMMAND_COUNT 2

void  (* const command_list[ COMMAND_COUNT]) (user * muser, int argc, char * argv[]);
const char *   command_alias[COMMAND_COUNT];

void (* const findCommand( const char * cmd )) ( user * muser, int argc, char * argv[] );

void commandHelp(user * muser, int argc, char * argv[]);
void commandExit(user * muser, int argc, char * argv[]);
