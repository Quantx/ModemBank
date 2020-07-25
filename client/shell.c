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

}

int cmd_exit( int argc, char ** argv )
{

}

int cmd_dial( int argc, char ** argv )
{

}

int cmd_connect( int arvc, char ** argv )
{

}
