#include "modembank.h"
#include "commands.h"

void (* const command_list[COMMAND_COUNT]) ( user * muser, int argc, char * argv[]) = {
    commandHelp,
    commandExit,
    commandClear
};

const char * command_alias[COMMAND_COUNT] = {
    "help,?",
    "exit,quit,logout",
    "clear,cls"
};

void (* const findCommand(const char * cmd)) ( user * muser, int argc, char * argv[] )
{
    if ( cmd == NULL ) return NULL;

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
    char helppath[300];
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

        sprintf( helppath, "assets/help/%s.hlp", argv[1] );

        fd = fopen( helppath, "r" );

        if ( fd == NULL )
        {
            zlog( "Unable to open help file: %s\n", helppath );
            return;
        }

        // Discard first line
        fgets( helpbuf, 256, fd );

        while ( fgets( helpbuf, 256, fd ) != NULL )
        {
            int lln = strlen( helpbuf );
            // Discard unix line endings
            if ( helpbuf[lln - 1] == '\n' ) helpbuf[lln - 1] = '\0';

            uprintf( muser, "%s\r\n", helpbuf );
        }

        // Close help file
        fclose( fd );

        return;
    }

    DIR * dr = opendir( "assets/help/" );

    if ( dr == NULL )
    {
        zlog( "Unable to open help directory: assets/help/\n" );
        return;
    }

    struct dirent * dir;

    // Read each file in directory
    while ( (dir = readdir( dr )) != NULL )
    {
        // Find location of "." in filename
        char * dotpos = strrchr( dir->d_name, '.' );

        // Not a help file
        if ( dotpos == NULL || strcmp( dotpos, ".hlp" ) ) continue;

        // Build help file path
        sprintf( helppath, "assets/help/%s", dir->d_name );

        fd = fopen( helppath, "r" );

        if ( fd == NULL )
        {
            zlog( "Unable to open help file: %s\n", helppath );
            return;
        }

        // Get first line
        fgets( helpbuf, 256, fd );

        // Remove unix line endings
        int lln = strlen( helpbuf );
        if ( helpbuf[lln - 1] == '\n' ) helpbuf[lln - 1] = '\0';

        // Send first line
        uprintf( muser, "%s\r\n", helpbuf );

        // Close help file
        fclose( fd );
    }

    // Close help directory
    closedir( dr );
}

// Logout this user
void commandExit( user * muser, int argc, char * argv[] )
{
    // Tell user they're being logged off
    uprintf( muser, "%%%s: terminated session\r\n", argv[0] );
    // Deep six their ass
    muser->flags |= FLAG_GARB;
}

void commandClear( user * muser, int argc, char * argv[] )
{
    // Transmit ANSI home & clear screen codes
    uprintf( muser, "\e[H\e[2J" );
}
