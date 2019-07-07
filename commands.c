#include "modembank.h"
#include "commands.h"

void (* const command_list[COMMAND_COUNT]) ( user * muser, int argc, char * argv[]) = {
    commandHelp
};

const char * command_alias[COMMAND_COUNT] = {
    "help,?"
};

void (* const findCommand(const char * cmd)) ( user * muser, int argc, char * argv[] )
{
    char * cmdtok;
    char alias[256];
    int i;

    for ( i = 0; i < COMMAND_COUNT; i++ )
    {
        // Don't let strtok destroy the original
        strcpy( alias, command_alias[i] );
        // Check each alias
        cmdtok = strtok( alias, "," );
        while ( cmdtok != NULL )
        {
            if ( !strcmp( cmdtok, cmd ) )
            {
                return command_list[i];
            }

            cmdtok = strtok( NULL, "," );
        }
    }

    return NULL;
}

void commandHelp( user * muser, int argc, char * argv[] )
{
    conn * mconn = muser->stdin;
    char help_path[50];
    char helpbuf[256];
    int i;
    FILE * fd;

    if ( argc > 1 )
    {
        if ( findCommand( argv[1] ) == NULL )
        {
            // Print out error message
            write( mconn->fd, "Unknown command: ", 17 );
            write( mconn->fd, argv[1], strlen(argv[1]) );
            write( mconn->fd, "\r\n", 2 );
            return;
        }

        sprintf( help_path, "%s.hlp", argv[1] );

        fd = fopen( help_path, "r" );

        if ( fd == NULL )
        {
            printf( "Unable to open help file: %s\n", help_path );
            return;
        }

        while ( fgets( helpbuf, 256, fd ) != NULL )
        {
            write( mconn->fd, helpbuf, strlen(helpbuf) );
            write( mconn->fd, "\r\n", 2 );
        }

        // Close help file
        fclose( fd );

        return;
    }
}
