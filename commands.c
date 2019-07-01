#include "modembank.h"
#include "commands.h"

void (* const command_list[COMMAND_COUNT]) ( user * muser, int argc, char * argv[]) = {
    cmd_help
};

const char * command_alias[COMMAND_COUNT] = {
    "help"
};

void cmd_help( user * muser, int argc, char * argv[] )
{
    conn * mconn = muser->stdin;
    char help_path[50];
    char helpbuf[256];
    int i;
    FILE * fd;

    if ( argc > 1 )
    {
        for ( i = 0; i < COMMAND_COUNT; i++ )
        {
            if ( !strcmp( argv[1], command_alias[i] ) ) break;
        }

        if ( i == COMMAND_COUNT )
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
