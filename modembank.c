#include "modembank.h"

static volatile int isRunning = 1;

const speed_t baudlist[BAUDLIST_SIZE] = { B300, B1200, B4800, B9600, B19200, B38400, B57600, B115200 };
const int    baudalias[BAUDLIST_SIZE] = {  300,  1200,  4800,  9600,  19200,  38400,  57600,  115200 };

int main( int argc, char * argv[] )
{
    // Setup cleanup code first
    signal( SIGINT, sigHandler );

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
        perror("setsockopt(SO_REUSEADDR) failed");

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

    printf( "Started terminal server on port: %d\n", HOST_PORT );

    // Build the user sentinel node
    user headuser;
    headuser.flags = FLAG_SENT;
    headuser.prev = headuser.next = &headuser;
    // Number of users
    int user_count = 0;

    // Build the conn sentinel node
    conn headconn;
    headconn.flags = FLAG_SENT;
    headconn.prev = headconn.next = &headconn;
    // Number of conns
    int conn_count = 0;

    // Configure serial modems
    conn_count += configureModems( &headconn );

    conn * icn; // For iterrating through conns
    user * iur; // For iterrating through users

    while ( isRunning )
    {
        // The 1 is for the server's listening socket
        int pfd_size = 1 + conn_count;

        // Store the fds of all conns
        struct pollfd * pfds = malloc( sizeof(struct pollfd) * pfd_size );

        // Configure server socket
        pfds[0].fd = servsock;
        pfds[0].events = POLLIN;

        i = 1;
        // Add conn sockets to array
        for ( icn = headconn.next; icn != &headconn && i < pfd_size; (icn = icn->next),i++ )
        {
            if ( icn->flags & FLAG_GARB )
            {
                printf( "Uncaught garbage conn\n" );
                continue;
            }

            pfds[i].fd = icn->fd;
            pfds[i].events = 0;

            // Can we read?
            if ( icn->buflen < BUFFER_LEN )
            {
                pfds[i].events |= POLLIN;
            }
            else
            {
                printf( "Conn has full buffer\n" );
            }
        }

        printf( "Polling %d buffers, %d users\n", i - 1, user_count );

        poll( pfds, pfd_size, TIMEOUT * 1000 );

        if ( pfds[0].revents & POLLIN )
        {
            connsock = accept(servsock, (struct sockaddr *) &cli_addr, &clilen);

            if ( connsock < 0 )
            {
                perror( "ERROR on accept" );
            }
            else
            {
                printf( "New network connection\n" );

                // Create a new connection
                conn * newconn = malloc( sizeof(conn) );

                // Initialize conn
                newconn->flags = 0;
                newconn->fd = connsock;
                newconn->buflen = 0;

                // Transmit telnet init string
                write( connsock, "\xFF\xFD\x22\xFF\xFB\x01\xFF\xFB\x03\xFF\xFD\x1F\xFF\xFD\x18", 15 );

                // Set the position of our current node
                newconn->prev = headconn.prev;
                newconn->next = &headconn;

                // Update the last node in the list to point to newconn
                headconn.prev->next = newconn;
                // Update sentinel node to point to newconn
                headconn.prev = newconn;

                // Increment count
                conn_count++;

                // Create a new user session for this socket
                user * newuser = malloc( sizeof(newuser) );

                // Associate the new conn & user
                newuser->stdin = newconn;
                newuser->stdout = NULL;

                // Set default username
                strcpy( newuser->name, "guest" );
                // Take note of current time
                newuser->first = newuser->last = time(NULL);
                // Set command line
                newuser->cmdsec = 0;
                newuser->cmdwnt = 20; // First prompt is username
                newuser->cmdlen = 0;
                newuser->cmdpos = 0;
                // Default terminal size
                newuser->width = 80;
                newuser->height = 24;
                // Default terminal name
                strcpy( newuser->termtype, "unknown" );

                // Set the position of our current node
                newuser->prev = headuser.prev;
                newuser->next = &headuser;

                // Update the last node in the list to point to newuser
                headuser.prev->next = newuser;
                // Update sentinel node to point to newuser
                headuser.prev = newuser;

                // Increment count
                user_count++;
            }
        }

        i = 1;
        for ( icn=headconn.next; icn != &headconn && i < pfd_size; (icn = icn->next),i++ )
        {
            if ( pfds[i].revents & POLLIN ) // Data to be read
            {
                // Read in data
                icn->buflen += read( icn->fd, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );

                // Check for termination
                if ( !icn->buflen )
                {
                    icn->flags |= FLAG_GARB;
                    continue;
                }

                // Add end of string char
                icn->buf[icn->buflen] = '\0';
            }
        }

        // We re-compute this list each time, so free it now that we're done reading in
        free( pfds );

        // Handle user sessions
        for ( iur = headuser.next; iur != &headuser; iur = iur-> next )
        {
            // Check if our input buffer is dead
            if ( iur->stdin->flags & FLAG_GARB )
            {
                // This user is dead now too
                iur->flags |= FLAG_GARB;
                continue;
            }

            // If we're not a modem, decode Telnet options
            if ( !(iur->stdin->flags & FLAG_MODM) )
            {
                telnetOptions( iur );
            }

            // Check if we have a valid command
            if ( iur->cmdwnt )
            {
                commandLine( iur );
            }
            else
            {
                commandShell( iur );
            }
        }

        // User garbage collection
        // Conn garbage collection
        user * garbuser;
        iur = headuser.next;
        while ( iur != &headuser )
        {
            garbuser = iur;
            iur = iur->next; // Gotta do this before we free the user

            // Nothing to do here
            if ( !(garbuser->flags & FLAG_GARB) ) continue;

            // Make sure our input buffer gets cleaned up
            garbuser->stdin->flags |= FLAG_GARB;
            // Make suer our output buffer gets clean up if we have one
            if ( garbuser->stdout != NULL ) garbuser->stdout->flags |= FLAG_GARB;

            // Update node before us
            garbuser->prev->next = garbuser->next;
            // Update node after us
            garbuser->next->prev = garbuser->prev;

            printf("User '%s' disconnected\n", garbuser->name);

            // Free user
            free( garbuser );
            // Decriment user count
            user_count--;
        }

        // Conn garbage collection
        conn * garbconn;
        icn = headconn.next;
        while ( icn != &headconn )
        {
            garbconn = icn;
            icn = icn->next; // Gotta do this before we free the conn

            // Nothing to do here
            if ( !(garbconn->flags & FLAG_GARB) ) continue;

            if ( garbconn->flags & FLAG_SENT )
            {
                // Sentinel nodes can't be garbage
                garbconn->flags &= ~FLAG_GARB;
            }
            else if ( garbconn->flags & FLAG_MODM )
            {
                // TODO: Reset modem fully

                // Finished reseting modem, mark it as not garbage, and available
                garbconn->flags &= ~(FLAG_GARB | FLAG_CALL);
            }
            else
            {
                // Update node before us
                garbconn->prev->next = garbconn->next;
                // Update node after us
                garbconn->next->prev = garbconn->prev;

                // Shutdown socket
                shutdown( garbconn->fd, SHUT_RDWR );
                // Ensure our socket is closed
                close( garbconn->fd );

                printf("Terminated an %s disconnected\n", garbconn->flags & FLAG_OUTG ? "outgoing" : "incoming");

                // Free the object
                free( garbconn );
                // Decrement count
                conn_count--;
            }
        }
    }

    // Close everything
    for ( icn = headconn.next; icn != &headconn; icn = icn->next )
    {
        close( icn->fd );
    }
}

int configureModems( conn * headconn )
{
    printf( "Initializing modems...\n" );

    int i;
    int mod_count = 0;
    struct termios modopt;

    FILE * fd = fopen( "config/modems.csv", "r" );

    if ( fd == NULL )
    {
        perror( "Cannot open modem config file" );
        return -1;
    }

    char modbuf[256];
    char atbuf[256];
    char * modtok;
    int baud_alias, baud_index;

    while ( fgets( modbuf, 256, fd ) != NULL )
    {
        // Get the tty file to open
        modtok = strtok( modbuf, "," );

        // No baud rate specified, skip this one
        if ( modtok == NULL ) continue;

        // Open the serial port
        int mfd = open( modtok, O_RDWR );

        // Couldn't open port
        if ( mfd < 0 )
        {
            printf( "Failed to open modem %s\n", modtok );
            continue;
        }

        // This isn't a serial port
        if ( !isatty(mfd) )
        {
            printf( "Failed, %s is not a TTY\n", modtok );
            close( mfd );
            continue;
        }

        // Get the baud rate index
        modtok = strtok( NULL, "," );

        // Convert to integer
        baud_alias = atoi( modtok );
        baud_index = -1;

        for ( i = 0; i < BAUDLIST_SIZE; i++ )
        {
            if ( baud_alias == baudalias[i] )
            {
                baud_index = i;
                break;
            }
        }

        if ( baud_index < -1 )
        {
            printf( "Failed, invalid baud rate %d for modem %s\n", baud_alias, modbuf );
            close( mfd );
            continue;
        }

        // Get current config
        tcgetattr( mfd, &modopt );

        // Update with new parameters
        modopt.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);

        modopt.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OLCUC | OPOST);

        modopt.c_cflag &= ~(CSIZE | PARENB);
        modopt.c_cflag |= CS8;

        modopt.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

        // Set 1 second timeout
        modopt.c_cc[VTIME] = INIT_DURATION * 10;
        modopt.c_cc[VMIN] = 0;

        // Configure baud rate
        cfsetispeed( &modopt, baudlist[baud_index] );
        cfsetospeed( &modopt, baudlist[baud_index] );

        // Flash settings to the tty after flushing
        tcsetattr( mfd, TCSAFLUSH, &modopt );

        // Attempt to intialize the modem
        for ( i = 0; i < INIT_ATTEMPTS; i++ )
        {
            printf( "Sending 'AT' to modem (attempt %d)... ", i );
            fflush(stdout);

            // Send an AT command for baud rate detection
            write( mfd, "AT\r", 3 );

            // Wait for read
            int atsize = read( mfd, atbuf, 255 );

            // Check if we got any data
            if ( atsize > 0 )
            {
                // Recived an OK command
                if ( strncmp( atbuf, "\r\nOK\r\n", 6 ) == 0 )
                {
                    printf( "responded 'OK'\n" );
                    break;
                }
                else
                {
                    // Make the string printable
                    atbuf[atsize] = '\0';
                    printf( "unknown response: >>>%s<<<\n", atbuf );
                    for ( int k = 0; k < atsize; k++ ) printf( "%02x|", atbuf[k] & 0xFF );
                    printf( "\n" );
                }
            }
            else
            {
                printf( "no response\n" );
            }
        }

        // Could not reach modem
        if ( i == INIT_ATTEMPTS )
        {
             close( mfd );
             printf( "Modem %s non-responsive to AT commands\n", modbuf );
             continue;
        }

        // Send reset command to modem
        write( mfd, "ATZ\r", 4 );

        // Set blocking
        modopt.c_cc[VTIME] = 0;
        modopt.c_cc[VMIN] = 1;
        // Re-flash settings
        tcsetattr( mfd, TCSANOW, &modopt );

        printf( "Intialized modem %s for baud %d\n", modbuf, baud_alias );

        // Create a new conn
        conn * newconn = malloc( sizeof(conn) );

        // Save modem information
        newconn->flags = FLAG_MODM;
        newconn->fd = mfd;
        newconn->buflen = 0;

        // Set the position of our current node
        newconn->prev = headconn->prev;
        newconn->next = headconn;

        // Update the last node in the list to point to newconn
        headconn->prev->next = newconn;
        // Update sentinel node to point to newconn
        headconn->prev = newconn;

        // Save the modem file descriptor
        mod_count++;
    }

    // Remember to close the config file
    fclose( fd );

    printf( "Initialized %d modems\n", mod_count );

    return mod_count;
}

int telnetOptions( user * muser )
{
    // Get our input buffer
    conn * mconn = muser->stdin;

    if ( mconn->buflen <= 0 ) return 0;

    if ( mconn->buf[0] != '\xFF' ) return 0; // All options start with 0xFF

    int i;
    char * ctok = mconn->buf;

    printf( "Telnet options string:\n" );

    for ( i = 0; i < mconn->buflen; i++ )
    {
        printf( "%02x", mconn->buf[i] & 0xFF );
        if ( i < mconn->buflen - 1 ) printf("|");
    }

    printf( "\n" );

    while ( ctok < mconn->buf + mconn->buflen )
    {
        if ( ctok[1] == '\xFA' ) // Sub negotiate params
        {
            switch ( ctok[2] ) // What type of option?
            {
                case '\x1F': // Negotiate About Window Size (NAWS)
                    muser->width  = (ctok[3] << 8) + ctok[4];
                    muser->height = (ctok[5] << 8) + ctok[6];
                    printf( "Set terminal size to: %d by %d\n", muser->width, muser->height );
                    break;
                case '\x18': // Terminal type
                    for ( i = 0; i < TERMTYPE_LEN && i + 4 + ctok < mconn->buf + mconn->buflen; i++ )
                    {
                        // End of negotiation
                        if ( ctok[i + 4] == '\xFF' && ctok[i + 5] == '\xF0' ) break;
                        // Copy char
                        muser->termtype[i] = ctok[i + 4];
                    }
                    muser->termtype[i] = '\0';
                    printf( "Set terminal type to: '%s'\n", muser->termtype );
                    break;
            }
        }
        else if ( ctok[1] == '\xFB' ) // WILL
        {
            switch ( ctok[2] ) // What option?
            {
                case '\x18': // Terminal type
                    // Request terminal name
                    write( mconn->fd, "\xFF\xFA\x18\x01\xFF\xF0", 6 );
                    break;
            }
        }

        for ( ctok++; ctok < mconn->buf + mconn->buflen && ctok[0] != '\xFF'; ctok++ );
    }

    // Empty the buffer
    mconn->buflen = 0;

    return 1;
}

void commandLine( user * muser )
{
    // Get our input buffer
    conn * mconn = muser->stdin;

    // Nothing to do here
    if ( mconn->buflen <= 0 || muser->cmdwnt <= 0 ) return;

    char acms[9]; // For sending ANSI CSI strings
    int i;

    // Fix a weird bug
    if ( mconn->buf[0] == '\x0D' && mconn->buf[1] == '\0' ) mconn->buflen = 1;

    if ( mconn->buflen == 1 ) // Standard ascii
    {
        // Check if this is printable ascii
        if ( mconn->buf[0] >= 32 && mconn->buf[0] < 127 )
        {
            // Check if we have room in the command string
            if ( muser->cmdlen < muser->cmdwnt && muser->cmdlen < COMMAND_LEN )
            {
                // Make room for character
                for ( i = muser->cmdlen; i > muser->cmdpos; i-- )
                {
                    muser->cmdbuf[i] = muser->cmdbuf[i - 1];
                }

                // Insert character
                muser->cmdbuf[i] = mconn->buf[0];
                // Increase string length
                muser->cmdlen++;

                if ( !muser->cmdsec )
                {
                    // Reprint string
                    write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                    // Move cursor back to correct spot
                    for ( i = muser->cmdlen; i > muser->cmdpos + 1; i-- ) write( mconn->fd, "\b", 1 );
                }
                // Advance cursor
                muser->cmdpos++;
            }
            else
            {
                // No room, ring their bell to let them know
                write( mconn->fd, "\a", 1 );
            }
        }
        else if ( mconn->buf[0] == '\x0D' ) // Return character sent
        {
            // Add null byte
            muser->cmdbuf[muser->cmdlen] = '\0';

            // Reset internal vars
            muser->cmdlen = 0;
            muser->cmdpos = 0;

            // Indicate ready
            muser->cmdwnt = 0;

            // Send newline
            write( mconn->fd, "\r\n", 2 );

            // Log command
            if ( muser->cmdsec )
            {
                printf( "New secure command\n" );
            }
            else
            {
                printf( "New command: '%s'\n", muser->cmdbuf );
            }
        }
        else if ( mconn->buf[0] == '\b' || mconn->buf[0] == '\x7F' ) // Backspace character sent
        {
            if ( muser->cmdpos > 0 ) // Anything to delete?
            {
                // Shift all characters left by one
                for ( i = muser->cmdpos; i < muser->cmdlen; i++ )
                {
                    muser->cmdbuf[i - 1] = muser->cmdbuf[i];
                }

                // Decrease length of string
                muser->cmdlen--;

                // Move cursor
                muser->cmdpos--;

                if ( !muser->cmdsec )
                {
                    write( mconn->fd, "\b", 1 );
                    // Reprint string
                    write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                    // Add a space to cover up deleted char
                    write( mconn->fd, " \b", 2 );
                    // Move cursor back to correct spot
                    for ( i = muser->cmdlen; i > muser->cmdpos; i-- ) write( mconn->fd, "\b", 1 );
                }
            }
        }
        else
        {
            printf( "Got unknown ascii: %02x\n", mconn->buf[0] & 0xFF );
        }
    }
    else if ( mconn->buf[0] == '\e' && mconn->buf[1] == '[' )// Extended control string (Arrow key, Home, etc.)
    {
        switch ( mconn->buf[2] ) // Check for arrow keys
        {
            case 'A': // Up arrow
                break;
            case 'B': // Down arrow
                break;
            case 'C': // Right arrow
                if ( muser->cmdpos < muser->cmdlen )
                {
                    // Move cursor to the right
                    muser->cmdpos++;
                    if ( !muser->cmdsec ) write( mconn->fd, "\e[C", 3 );
                }
                break;
            case 'D': // Left arrow
                if ( muser->cmdpos > 0 )
                {
                    // Move cursor to the left
                    muser->cmdpos--;
                    if ( !muser->cmdsec ) write( mconn->fd, "\b", 1 );
                }
                break;
            case 'H': // Home
                if ( muser->cmdpos > 0 )
                {
                    muser->cmdpos = 0;
                    write( mconn->fd, "\r", 1 );
                }
                break;
            case 'F': // End
                if ( muser->cmdpos > 0 )
                {
                    if ( !muser->cmdsec )
                    {
                        // Compute ANSI CSI string
                        sprintf( acms, "\e[%dC", muser->cmdlen - muser->cmdpos );
                        write( mconn->fd, acms, strlen( acms ) );
                    }
                    // Update cursor position
                    muser->cmdpos = muser->cmdlen;
                }
                break;
            case '3': // Delete
                if ( muser->cmdpos < muser->cmdlen )
                {
                    // Shift all characters left by one
                    for ( i = muser->cmdpos; i < muser->cmdlen - 1; i++ )
                    {
                        muser->cmdbuf[i] = muser->cmdbuf[i + 1];
                    }

                    // Decrease length of string
                    muser->cmdlen--;

                    if ( !muser->cmdsec )
                    {
                        // Reprint string
                        write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                        // Add a space to cover up deleted char
                        write( mconn->fd, " \b", 2 );
                        // Move cursor back to correct spot
                        for ( i = muser->cmdlen; i > muser->cmdpos; i-- ) write( mconn->fd, "\b", 1 );
                    }
                }
                break;
        }
    }
    else
    {
        printf( "Random string:\n" );
        for ( i = 0; i < mconn->buflen; i++ )printf( "%02x|", mconn->buf[i] & 0xFF );
        printf( "\n" );
    }

    // Empty the buffer
    mconn->buflen = 0;
}

void commandShell( user * muser )
{

}

void sigHandler(int sig)
{
    isRunning = 0;
    // Hide the CTRL+C
    printf("\b\b  \b\b[Interrupt Signaled]\n");
}
