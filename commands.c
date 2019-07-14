#include "modembank.h"
#include "commands.h"

void (* const command_list[COMMAND_COUNT]) ( data * mdata, user * muser, int argc, char * argv[]) = {
    commandHelp,
    commandExit,
    commandClear,
    commandTelnet
};

const char * command_alias[COMMAND_COUNT] = {
    "help,?",
    "exit,quit,logout",
    "clear,cls",
    "telnet,connect"
};

void (* const findCommand(const char * cmd)) ( data * mdata, user * muser, int argc, char * argv[] )
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

void commandHelp( data * mdata, user * muser, int argc, char * argv[] )
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
void commandExit( data * mdata, user * muser, int argc, char * argv[] )
{
    // Tell user they're being logged off
    uprintf( muser, "%%%s: terminated session\r\n", argv[0] );
    // Deep six their ass
    muser->flags |= FLAG_GARB;
}

void commandClear( data * mdata, user * muser, int argc, char * argv[] )
{
    // Transmit ANSI home & clear screen codes
    uprintf( muser, "\e[H\e[2J" );
}

void commandTelnet( data * mdata, user * muser, int argc, char * argv[] )
{
    // Need to specify an address
    if ( argc <= 1 )
    {
        uprintf( muser, "%%%s: please specify a destination address", argv[0] );
        return;
    }

    struct addrinfo hints, * res;
    char addrstr[100];
    char * port = "telnet";

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; //AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;// | AI_NUMERICSERV;

    // Did the user specify a port?
    if ( argc > 2 ) port = argv[2];

    if ( getaddrinfo( argv[1], port, &hints, &res ) )
    {
        uprintf( muser, "%%%s: invalid service/port or address: %s %s\r\n", argv[0], argv[1], port );
        return;
    }

    if ( !res )
    {
        uprintf( muser, "%%%s: unable to resolve address: %s\r\n", argv[0], argv[1] );
        return;
    }

    // Get actual addresss
    struct sockaddr_in * destip = (struct sockaddr_in *)res->ai_addr;

    // Get a human readable address
    inet_ntop( res->ai_family, &( destip->sin_addr ), addrstr, 100 );

    // Begin connection process
    int sock = socket( res->ai_family, res->ai_socktype, 0 );

    if ( sock < 0 )
    {
        xlog( muser, "req_opensock: unable to create socket\n" );
        return;
    }

    // Generate new connection
    conn * newconn = malloc( sizeof(conn) );

    // Record time
    time( &(newconn->first) );
    time( &(newconn->last ) );

    // Initialize conn
    newconn->flags = FLAG_OUTG;
    newconn->fd = sock;
    newconn->buflen = 0;
    newconn->org.addr = *(destip);

    // Print out the port and string
    sprintf( newconn->name, "%s:%hu", addrstr, newconn->org.addr.sin_port );

    // Set non-blocking
    if ( setBlocking( newconn, 0 ) < 0 )
    {
        xlog( muser, "req_opensock: failed to set non-blocking\n" );

        close( sock );
        free( newconn );

        return;
    }

    if ( connect( sock, res->ai_addr, sizeof( *(res->ai_addr) ) ) < 0 )
    {
        if ( errno != EINPROGRESS )
        {
            uprintf( muser, "Failed to connect to %s (%s) on port %hu\r\n", addrstr, res->ai_canonname, htons(destip->sin_port) );
            xlog( muser, "Failed to connect to %s (%s) on port %hu\r\n", addrstr, res->ai_canonname, htons(destip->sin_port) );

            // Close and free conn
            close( sock );
            free( newconn );

            return;
        }
    }

    uprintf( muser, "Trying %s (%s) on port %hu\r\n", addrstr, res->ai_canonname, htons(destip->sin_port) );
    xlog( muser, "Trying %s (%s) on port %hu\r\n", addrstr, res->ai_canonname, htons(destip->sin_port) );

    // Set the stdout of the user
    muser->stdout = newconn;
    // Set the user into bridge mode
    muser->flags |= FLAG_BRDG;

     // Insert node
    newconn->next = mdata->headconn;
    mdata->headconn = newconn;

    mdata->conn_count++;

    return;
}
