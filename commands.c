#include "modembank.h"
#include "commands.h"

void (* const command_list[COMMAND_COUNT]) ( user * muser, int argc, char * argv[]) = {
    commandHelp,
    commandExit
};

const char * command_alias[COMMAND_COUNT] = {
    "help,?",
    "exit,quit,logout"
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
    char help_path[50];
    char helpbuf[256];
    int i;
    FILE * fd;

    if ( argc > 1 )
    {
        if ( findCommand( argv[1] ) == NULL )
        {
            // Print out error message
            uprintf( muser, "%%help: unknown command: %s\r\n", argv[1] );
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
            uprintf( muser, "%s\r\n", helpbuf );
        }

        // Close help file
        fclose( fd );

        return;
    }
}

// Logout this user
void commandExit( user * muser, int argc, char * argv[] )
{
    // Tell user they're being logged off
    uprintf( muser, "%%%s: terminated session\r\n", argv[0] );
    // Deep six their ass
    muser->flags |= FLAG_GARB;
}
