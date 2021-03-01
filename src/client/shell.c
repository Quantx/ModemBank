#include "shell.h"

struct {
    char * name;
    int (*cmd)(int argc, char ** argv);
} commands[] = {
    { "help", cmd_help },
    { "exit", cmd_exit },
    { "dial", cmd_dial },
    { "connect", cmd_connect }
};

int cmd_help( int argc, char ** argv )
{
    return 0;
}

int cmd_exit( int argc, char ** argv )
{
    return 0;
}

int cmd_dial( int argc, char ** argv )
{
    return 0;
}

int cmd_connect( int arvc, char ** argv )
{
    return 0;
}
