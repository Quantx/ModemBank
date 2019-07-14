#define COMMAND_COUNT 4

void  (* const command_list[ COMMAND_COUNT]) (data * mdata, user * muser, int argc, char * argv[]);
const char *   command_alias[COMMAND_COUNT];

void (* const findCommand( const char * cmd )) (data * mdata, user * muser, int argc, char * argv[]);

void commandHelp  (data * mdata, user * muser, int argc, char * argv[]);
void commandExit  (data * mdata, user * muser, int argc, char * argv[]);
void commandClear (data * mdata, user * muser, int argc, char * argv[]);
void commandTelnet(data * mdata, user * muser, int argc, char * argv[]);

