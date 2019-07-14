#include "modembank.h"

static volatile int isRunning = 2;

int main( int argc, char * argv[] )
{
    // Setup cleanup code first
    signal( SIGINT, sigHandler );

    // Seed our RNG
    srand( time(NULL) );

    int servsock, connsock, clilen, i;
    struct sockaddr_in serv_addr, cli_addr;

    // Create socket
    servsock = socket(AF_INET, SOCK_STREAM, 0);

    if ( servsock < 0 )
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // Reuse sockets
    int enable = 1;
    if (setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(HOST_PORT);

    // Bind socket to port
    if (bind(servsock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    listen( servsock, 5 );
    clilen = sizeof(cli_addr);

    zlog( "Started terminal server on port: %d\n", HOST_PORT );

    data globdata;

    globdata.headuser = NULL;
    globdata.user_count = 0;

    globdata.headconn = NULL;
    globdata.conn_count = 0;

    // Configure serial modems
    globdata.conn_count += loadModemConfig( &(globdata.headconn) );

    conn * icn; // For iterrating through conns
    user * iur; // For iterrating through users

    // Indicates that the program is about to enter the loop
    isRunning = 1;
    while ( isRunning )
    {
        // Check for new modem connections
        for ( icn = globdata.headconn; icn != NULL; icn = icn->next )
        {
            // Skip, not a modem
            if ( !(icn->flags & FLAG_MODM) ) continue;

            // Skip, modem reseting
            if ( !(icn->flags & FLAG_GARB) ) continue;

            // Check for remote hangup
            if ( icn->flags & FLAG_CALL )
            {
                // Skip, call in-progress
                if ( getDCD( icn ) ) continue;

                // Reset modem
                icn->flags |= FLAG_GARB;
            }
            // Check for incoming call
            else
            {
                // Flush the modem, keep the buffer empty
                tcflush( icn->fd, TCIOFLUSH );

                // Skip, no call
                if ( !getDCD( icn ) ) continue;

                // We need to start a new session
                globdata.user_count += createSession( &(globdata.headuser), icn );

                // Mark modem as active
                icn->flags |= FLAG_CALL;
            }
        }

        // The 1 is for the server's listening socket
        int pfd_size = 1 + globdata.conn_count;

        // Store the fds of all conns
        struct pollfd * pfds = malloc( sizeof(struct pollfd) * pfd_size );

        // Configure server socket
        pfds[0].fd = servsock;
        pfds[0].events = POLLIN;

        i = 1;
        // Add conn sockets to array
        for ( icn = globdata.headconn; icn != NULL && i < pfd_size; (icn = icn->next), i++ )
        {
            pfds[i].fd = icn->fd;
            pfds[i].events = 0;

            // Skip garbage connection
            if ( icn->flags & FLAG_GARB ) continue;

            // Skip unconnected modems
            if ( !(icn->flags & FLAG_CALL) && icn->flags & FLAG_MODM ) continue;

            // Can we read?
            if ( icn->buflen < BUFFER_LEN )
            {
                pfds[i].events |= POLLIN;
            }
            else
            {
                ylog( icn, "Full buffer alarm!\n" );
            }
        }

        zlog( "Polling %d buffers, %d users\n", i, globdata.user_count );

        poll( pfds, pfd_size, MASTER_TIMEOUT * 1000 );

        i = 1;
        for ( icn = globdata.headconn; icn != NULL && i < pfd_size; (icn = icn->next),i++ )
        {
            // Check pending sockets
            if ( !(icn->flags & FLAG_CALL) && !(icn->flags & FLAG_MODM) )
            {
                // Check for socket timeouts
                if ( time(NULL) > icn->first + CONN_TIMEOUT )
                {
                    ylog( icn, "Connection timed out\n" );
                    icn->flags |= FLAG_GARB;
                }
            }

            if ( pfds[i].revents & POLLERR ) // Socket error
            {
                // Modem error
                if ( icn->flags & FLAG_MODM )
                {
                    icn->flags |= FLAG_GARB;
                }
                // Socket error
                else
                {
                    int error = 0;
                    socklen_t errlen = sizeof(error);
                    getsockopt( icn->fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);

                    // Handle "still connecting" errors
                    if ( error == EINPROGRESS && icn->flags & FLAG_OUTG )
                    {
                        // Did we exceede timeout?
                        if ( time(NULL) > icn->first + CONN_TIMEOUT )
                        {
                            ylog( icn, "Connection timed out (EINPROGRESS)\n" );
                            icn->flags |= FLAG_GARB;
                        }
                    }
                    // Unknown error, abort!
                    else
                    {
                        ylog( icn, "Conn error\n" );
                        icn->flags |= FLAG_GARB;
                    }
                }
            }
            else if ( pfds[i].revents & POLLHUP ) // Socket hang-up, close it
            {
                icn->flags |= FLAG_GARB;
                ylog( icn, "Conn hangup signal\n" );
            }
            else if ( pfds[i].revents & POLLIN ) // Data to be read
            {
                // Re-enable blocking on outgoing sockets
                if ( icn->flags & FLAG_OUTG
                && !(icn->flags & FLAG_MODM)
                && !(icn->flags & FLAG_CALL) )
                {
                    setBlocking( icn, 1 );
                    // Mark this socket as an active call
                    icn->flags |= FLAG_CALL;

                    write( icn->fd, "Connected to ", 28 );
                    write( icn->fd, icn->name, strlen( icn->name ) );
                    write( icn->fd, "\r\n", 2 );

                    continue;
                }

                // Read in data
                int ret = read( icn->fd, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );

                // Check for termination
                if ( ret <= 0 )
                {
                    // Flag as garbage
                    icn->flags |= FLAG_GARB;
                    ylog( icn, "Conn read error\n" );
                }
                else
                {
                    // Record last time
                    time( &(icn->last) );

                    // Update buffer length
                    icn->buflen += ret;

                    // Add end of string char
                    icn->buf[icn->buflen] = '\0';
                }
            }
        }

        // Accept new connections last
        if ( pfds[0].revents & POLLIN )
        {
            connsock = accept( servsock, (struct sockaddr *) &cli_addr, &clilen );

            if ( connsock < 0 )
            {
                perror( "ERROR on accept" );
            }
            else
            {
                // Create a new connection
                conn * newconn = malloc( sizeof(conn) );

                // Initialize conn
                newconn->flags = FLAG_CALL;
                newconn->fd = connsock;
                newconn->buflen = 0;
                newconn->org.addr = cli_addr;

                // Record time
                time( &(newconn->first) );
                time( &(newconn->last ) );

                char ipstr[40];
                // Convert the IP to a string
                inet_ntop( AF_INET, &(newconn->org.addr.sin_addr), ipstr, 40 );
                // Print out the port and string
                sprintf( newconn->name, "%s:%hu", ipstr, newconn->org.addr.sin_port );

                // Transmit telnet init string
                write( newconn->fd, "\xFF\xFD\x22\xFF\xFB\x01\xFF\xFB\x03\xFF\xFD\x1F\xFF\xFD\x18", 15 );

                // Insert node
                newconn->next = globdata.headconn;
                globdata.headconn = newconn;

                // Increment count
                globdata.conn_count++;

                ylog( newconn, "Accepted incoming connection\n" );

                // Increment count
                globdata.user_count += createSession( &(globdata.headuser), newconn );
            }
        }

        // We re-compute this list each time, so free it now that we're done reading in
        free( pfds );

        // Handle user sessions
        for ( iur = globdata.headuser; iur != NULL; iur = iur->next )
        {
            // Check if our input buffer is dead
            if ( iur->stdin == NULL || iur->stdin->flags & FLAG_GARB )
            {
                // This user is dead now too
                iur->flags |= FLAG_GARB;
                continue;
            }

            // Break out of bridge mode if needed
            if ( iur->flags & FLAG_BRDG && ( iur->stdout == NULL || iur->stdout->flags & FLAG_GARB ) )
            {
                if ( iur->stdout != NULL )
                {
                    if ( iur->stdout->flags & FLAG_CALL ) uprintf( iur, "\r\n" );
                    uprintf( iur, "Connection %s\r\n", iur->stdout->flags & FLAG_CALL ? "terminated" : "timed out" );
                }

                iur->flags &= ~FLAG_BRDG;
            }

            // Update AFK timer
            if ( iur->stdin->buflen > 0 ) time( &(iur->last) );

            // Operate in bridge mode
            if ( iur->flags & FLAG_BRDG )
            {
                terminalBridge( iur );
            }
            else
            {
                // If we're not a modem, decode Telnet options
                if ( !(iur->stdin->flags & FLAG_MODM) )
                {
                    telnetOptions( iur );
                }

                if ( iur->cmdwnt < 0 )
                {
                    // Get raw input
                    terminalRaw( iur );
                }
                else if ( iur->cmdwnt > 0 )
                {
                    // Assemble a command string
                    terminalLine( iur );
                }

                // Make this seperate so we can finish on same cycle
                if ( !iur->cmdwnt )
                {
                    // Nuke the backlog
                    iur->stdin->buflen = 0;
                    // Process the command
                    terminalShell( &globdata, iur );
                }
            }
        }

        // User garbage collection
        user * prevuser = NULL;
        user * nextuser = NULL;
        for ( iur = globdata.headuser; iur != NULL; iur = nextuser )
        {
            // Keep a copy of this pointer
            nextuser = iur->next;

            // Drop dead I/O devices
            if ( iur->stdin  != NULL && iur->stdin->flags  & FLAG_GARB ) iur->stdin  = NULL;
            if ( iur->stdout != NULL && iur->stdout->flags & FLAG_GARB ) iur->stdout = NULL;

            // Make sure this user is garbage
            if ( !(iur->flags & FLAG_GARB) )
            {
                prevuser = iur;
                continue;
            }

            // Make sure our input buffer gets cleaned up
            iur->stdin->flags |= FLAG_GARB;
            // Make sure our output buffer gets clean up if we have one
            if ( iur->stdout != NULL ) iur->stdout->flags |= FLAG_GARB;

            xlog( iur, "User disconnected\n" );

            // Check if we're head
            if ( prevuser == NULL )
            {
                globdata.headuser = iur->next;
            }
            else
            {
                prevuser->next = iur->next;
            }

            // Free user
            free( iur );
            // Decriment user count
            globdata.user_count--;
        }

        // Cleanup dead conns
        globdata.conn_count -= connGarbage( &(globdata.headconn) );
    }

    // Close server listen socket
    close( servsock );

    i = 1;
    // Close everything else
    for ( icn = globdata.headconn; icn != NULL; (icn = icn->next), i++ )
    {
        close( icn->fd );
    }

    zlog( "Closed %d connections\n", i );

    // Must be last line
    zlog( "ModemBank terminated successfully\n" );
}

int createSession( user ** headuser, conn * newconn )
{
    // Create a new user session for this socket
    user * newuser = malloc( sizeof(user) );

    // Zero out flags
    newuser->flags = 0;

    // Define initial request
    newuser->opcmd = req_idle;
    newuser->opdat = NULL;

    // Associate the new conn & user
    newuser->stdin = newconn;
    newuser->stdout = NULL;

    // Take note of current time
    time( &(newuser->first) );
    time( &(newuser->last ) );

    // Set default username
    sprintf( newuser->name, "guest-%d", rand() );
    // Set command line
    newuser->cmdsec = 0;
    newuser->cmdwnt = 20; // First prompt is username
    newuser->cmdlen = 0; // Negative means that we need to print a prompt
    newuser->cmdpos = 0;
    // Set prompt
    strcpy( newuser->cmdppt, "@" );
    // Default terminal size
    newuser->width = 80;
    newuser->height = 24;

    // Default terminal name
    strcpy( newuser->termtype, "unknown" );

    // Insert node
    newuser->next = *headuser;
    *headuser = newuser;

    xlog( newuser, "New user connected\n" );

    return 1;
}

void sigHandler(int sig)
{
    // Hide the CTRL+C
    printf("\b\b  \b\b");
    // Print log message
    zlog("*** Interrupt Signaled ***\n");

    // This is the second abort attempt, we need to hard exit
    if ( isRunning == 0 )
    {
        zlog( "Unable to gracefully stop ModemBank: hard exit\n" );
        exit(1);
    }

    // Only soft exit if we're in the loop
    if ( isRunning == 1 ) isRunning = 0;

    // We need to hard exit
    if ( isRunning > 1 ) exit(0);
}


int uprintf( user * muser, const char * format, ... )
{
    if ( muser == NULL
    || muser->flags & FLAG_GARB
    || muser->stdin == NULL
    || muser->stdin->flags & FLAG_GARB ) return -1;

    va_list args;
    va_start( args, format );

    // Save a reference to stdout
    int fdout = dup(1);
    // Overwrite stdout with the I/O device
    dup2( muser->stdin->fd, 1 );
    // Print to the I/O device
    int out = vprintf( format, args );
    // Ensure string is sent to the device
    fflush( stdout );
    // Reset stdout to it's default value
    dup2( fdout, 1 );

    va_end( args );

    return out;
};

void xlog( user * muser, const char * format, ... )
{
    printf( "[" );

    va_list args;
    va_start( args, format );
    vxlog( muser, format, args );
    va_end( args );
}

void vxlog( user * muser, const char * format, va_list args )
{
    char perms[6];

    strcpy( perms, "USER" );

    if ( muser->flags & FLAG_DEVL && muser->flags & FLAG_OPER )
    {
        strcpy( perms, "ADMIN" );
    }
    else if ( muser->flags & FLAG_DEVL )
    {
        strcpy( perms, "DEV" );
    }
    else if ( muser->flags & FLAG_OPER )
    {
        strcpy( perms, "SYSOP" );
    }

    printf( "%s %s|", muser->name, perms );

    vylog( muser->stdin, NULL, args );
    vylog( muser->stdout, format, args );
}

void ylog( conn * mconn, const char * format, ... )
{
    printf( "[" );

    va_list args;
    va_start( args, format );
    vylog( mconn, format, args );
    va_end( args );
}

void vylog( conn * mconn, const char * format, va_list args )
{
    if ( mconn != NULL )
    {
        printf( "%s %s|", mconn->name, mconn->flags & FLAG_OUTG ? "OUT" : "IN" );
    }

    vzlog( format, args );
}

void zlog( const char * format, ... )
{
    printf( "[" );

    va_list args;
    va_start( args, format );
    vzlog( format, args );
    va_end( args );
}

void vzlog( const char * format, va_list args )
{
    // Nothing was passed to us
    if ( format == NULL ) return;

    time_t rightNow;
    time( &rightNow );

    char timebuf[80];
    strftime( timebuf, 80, "%x %X", localtime( &rightNow ) );

    // Print out our debug info
    printf( "%s]", timebuf );

    // Print out user message
    vprintf( format, args );
}
