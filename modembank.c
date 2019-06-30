#include "modembank.h"

const speed_t baudlist[BAUDLIST_SIZE] = { B300, B1200, B4800, B9600, B19200, B38400, B57600, B115200 };
const int    baudalias[BAUDLIST_SIZE] = {  300,  1200,  4800,  9600,  19200,  38400,  57600,  115200 };

int main( int argc, char * argv[] )
{
    int servsock, connsock, clilen;
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

    // Configure serial ports for use with modems
    int mods[MAX_MODEMS];
    int mod_count = configureModems( mods );

    // Build the sentinel node
    conn headconn;
    headconn.sentinel = 1;
    headconn.prev = headconn.next = &headconn;

    conn * icn; // For iterrating through conns
    int i;
    int conn_count = 0; // Total number of conns

    printf( "Started terminal server on port: %d\n", HOST_PORT );

    while (1)
    {
        // The 1 is for the server socket
        int pfd_size = 1 + conn_count;

        struct pollfd * pfds = malloc( sizeof(struct pollfd) * pfd_size );

        // Configure server socket
        pfds[0].fd = servsock;
        pfds[0].events = POLLIN;

        i = 1;
        // Add conn sockets to array
        for ( icn = headconn.next; icn != &headconn && i < pfd_size; (icn = icn->next),i++ )
        {
            if ( icn->garbage )
            {
                printf( "Uncaught garbage conn\n" );
                continue;
            }

            pfds[i].fd = icn->sock;
            pfds[i].events = 0;

            // Can we read?
            if ( icn->buflen < BUFFER_LEN )
            {
                pfds[i].events = POLLIN;
            }
        }

        printf( "Polling %d clients\n", i - 1 );

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
                printf( "New connection\n" );

                // Create struct
                conn * newconn = malloc( sizeof(conn) );

                // Set default username
                strcpy( newconn->username, "Guest" );
                // Save socket
                newconn->sock = connsock;
                // Empty buffer
                newconn->buflen = 0;
                // Take note of current time
                newconn->first = newconn->last = time(NULL);
                // Set command line
                newconn->cmdsec = 0;
                newconn->cmdwnt = 20; // First prompt is username
                newconn->cmdlen = 0;
                newconn->cmdpos = 0;
                // Set flags
                newconn->sentinel = 0;
                newconn->admin = 0;
                newconn->garbage = 0;
                // Default terminal size
                newconn->width = 80;
                newconn->height = 24;

                // Flag this as a socket connection
                newconn->network = 1;
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
            }
        }

        i = 1;
        for ( icn=headconn.next; icn != &headconn && i < pfd_size; (icn = icn->next),i++ )
        {
            if ( pfds[i].revents & POLLIN ) // Data to be read
            {
                // Read in data
                icn->buflen += read( icn->sock, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );

                // Check for termination
                if ( !icn->buflen )
                {
                    icn->garbage = 1;
                }

                // Add end of string char
                icn->buf[icn->buflen] = '\0';

                // Up date last
                icn->last = time(NULL);
            }

            // Check if we were sent telnet options
            if ( icn->network ) telnetOptions( icn );

            // Process command line
            commandLine( icn );

            // Run the shell
            modemShell( icn );
        }

        // Conn garbage collection
        conn * garbconn;
        icn = headconn.next;
        while ( icn != &headconn )
        {
            garbconn = icn;
            icn = icn->next; // Gotta do this before we free the conn

            if ( garbconn->garbage && !garbconn->sentinel )
            {
                // Update node before us
                garbconn->prev->next = garbconn->next;
                // Update node after us
                garbconn->next->prev = garbconn->prev;

                // Ensure our socket is closed
                close( garbconn->sock );

                // Free the object
                free( garbconn );
                // Decrement count
                conn_count--;

                printf("Client disconnected\n");
            }
        }

        // We re-compute this list each time, so free it here
        free( pfds );
    }
}

int configureModems( int * mods )
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

        printf("1\n");

        tcgetattr( mfd, &modopt );

        printf("2\n");

        modopt.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);

        modopt.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OLCUC | OPOST);

        modopt.c_cflag &= ~(CSIZE | PARENB);
        modopt.c_cflag |= CS8;

        modopt.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

        modopt.c_cc[VTIME] = 10;
        modopt.c_cc[VMIN] = 0;

        printf("3\n");

        // Configure baud rate
        cfsetispeed( &modopt, baudlist[baud_index] );
        cfsetospeed( &modopt, baudlist[baud_index] );

        printf("4\n");

        // Flash settings to the tty after flushing
        tcsetattr( mfd, TCSAFLUSH, &modopt );

        printf("5\n");

        // Attempt to intialize the modem
        for ( i = 0; i < MAX_INIT_ATTEMPT; i++ )
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
                if ( strncmp( atbuf, "OK\r", 3 ) == 0 )
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
        if ( i == MAX_INIT_ATTEMPT )
        {
             close( mfd );
             printf( "Modem %s non-responsive to AT commands\n", modbuf );
             continue;
        }

        // Send reset command to modem
        write( mfd, "ATZ\r", 4 );

        // Set blocking
        modopt.c_cc[VMIN] = 64;
        // Re-flash settings
        tcsetattr( mfd, TCSANOW, &modopt );

        printf( "Intializing modem %s for baud %d\n", modbuf, baud_alias );

        // Save the modem file descriptor
        mods[mod_count] = mfd;
        mod_count++;
    }

    // Remember to close the config file
    fclose( fd );

    printf( "Initialized %d modems\n", mod_count );

    return mod_count;
}

int telnetOptions( conn * mconn )
{
    if ( mconn->buflen <= 0 ) return 0;

    if ( mconn->buf[0] != '\xFF' ) return 0; // All options start with 0xFF

    int i;
    char * ctok = mconn->buf;

    printf( "Telnet options string:\n" );

    for ( i = 0; i < mconn->buflen; i++ ) printf( "%02x|", mconn->buf[i] & 0xFF );

    printf( "\n" );

    while ( ctok < mconn->buf + mconn->buflen )
    {
        if ( ctok[1] == '\xFA' ) // Sub negotiate params
        {
            switch ( ctok[2] ) // What type of option?
            {
                case '\x1F': // Negotiate About Window Size (NAWS)
                    mconn->width  = (ctok[3] << 8) + ctok[4];
                    mconn->height = (ctok[5] << 8) + ctok[6];
                    printf( "Set terminal size to: %d by %d\n", mconn->width, mconn->height );
                    break;
                case '\x18': // Terminal type
                    for ( i = 0; i < TERMTYPE_LEN && i + 4 + ctok < mconn->buf + mconn->buflen; i++ )
                    {
                        // End of negotiation
                        if ( ctok[i + 4] == '\xFF' && ctok[i + 5] == '\xF0' ) break;
                        // Copy char
                        mconn->termtype[i] = ctok[i + 4];
                    }
                    mconn->termtype[i] = '\0';
                    printf( "Set terminal type to: '%s'\n", mconn->termtype );
                    break;
            }
        }
        else if ( ctok[1] == '\xFB' ) // WILL
        {
            switch ( ctok[2] ) // What option?
            {
                case '\x18': // Terminal type
                    // Request terminal name
                    write( mconn->sock, "\xFF\xFA\x18\x01\xFF\xF0", 6 );
                    break;
            }
        }

        for ( ctok++; ctok < mconn->buf + mconn->buflen && ctok[0] != '\xFF'; ctok++ );
    }

    // Empty the buffer
    mconn->buflen = 0;

    return 1;
}

void commandLine( conn * mconn )
{
    // Nothing to do here
    if ( mconn->buflen <= 0 || mconn->cmdwnt <= 0 ) return;

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
            if ( mconn->cmdlen < mconn->cmdwnt && mconn->cmdlen < COMMAND_LEN )
            {
                // Make room for character
                for ( i = mconn->cmdlen; i > mconn->cmdpos; i-- )
                {
                    mconn->cmdbuf[i] = mconn->cmdbuf[i - 1];
                }

                // Insert character
                mconn->cmdbuf[i] = mconn->buf[0];
                // Increase string length
                mconn->cmdlen++;

                if ( !mconn->cmdsec )
                {
                    // Reprint string
                    write( mconn->sock, mconn->cmdbuf + mconn->cmdpos, mconn->cmdlen - mconn->cmdpos );
                    // Move cursor back to correct spot
                    for ( i = mconn->cmdlen; i > mconn->cmdpos + 1; i-- ) write( mconn->sock, "\b", 1 );
                }
                // Advance cursor
                mconn->cmdpos++;
            }
            else
            {
                // No room, ring their bell to let them know
                write( mconn->sock, "\a", 1 );
            }
        }
        else if ( mconn->buf[0] == '\x0D' ) // Return character sent
        {
            // Add null byte
            mconn->cmdbuf[mconn->cmdlen] = '\0';

            // Reset internal vars
            mconn->cmdlen = 0;
            mconn->cmdpos = 0;

            // Indicate ready
            mconn->cmdwnt = 0;

            // Send newline
            write( mconn->sock, "\r\n", 2 );

            // Log command
            if ( mconn->cmdsec )
            {
                printf( "New secure command\n" );
            }
            else
            {
                printf( "New command: '%s'\n", mconn->cmdbuf );
            }
        }
        else if ( mconn->buf[0] == '\b' || mconn->buf[0] == '\x7F' ) // Backspace character sent
        {
            if ( mconn->cmdpos > 0 ) // Anything to delete?
            {
                // Shift all characters left by one
                for ( i = mconn->cmdpos; i < mconn->cmdlen; i++ )
                {
                    mconn->cmdbuf[i - 1] = mconn->cmdbuf[i];
                }

                // Decrease length of string
                mconn->cmdlen--;

                // Move cursor
                mconn->cmdpos--;

                if ( !mconn->cmdsec )
                {
                    write( mconn->sock, "\b", 1 );
                    // Reprint string
                    write( mconn->sock, mconn->cmdbuf + mconn->cmdpos, mconn->cmdlen - mconn->cmdpos );
                    // Add a space to cover up deleted char
                    write( mconn->sock, " \b", 2 );
                    // Move cursor back to correct spot
                    for ( i = mconn->cmdlen; i > mconn->cmdpos; i-- ) write( mconn->sock, "\b", 1 );
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
                if ( mconn->cmdpos < mconn->cmdlen )
                {
                    // Move cursor to the right
                    mconn->cmdpos++;
                    if ( !mconn->cmdsec ) write( mconn->sock, "\e[C", 3 );
                }
                break;
            case 'D': // Left arrow
                if ( mconn->cmdpos > 0 )
                {
                    // Move cursor to the left
                    mconn->cmdpos--;
                    if ( !mconn->cmdsec ) write( mconn->sock, "\b", 1 );
                }
                break;
            case 'H': // Home
                if ( mconn->cmdpos > 0 )
                {
                    mconn->cmdpos = 0;
                    write( mconn->sock, "\r", 1 );
                }
                break;
            case 'F': // End
                if ( mconn->cmdpos > 0 )
                {
                    if ( !mconn->cmdsec )
                    {
                        // Compute ANSI CSI string
                        sprintf( acms, "\e[%dC", mconn->cmdlen - mconn->cmdpos );
                        write( mconn->sock, acms, strlen( acms ) );
                    }
                    // Update cursor position
                    mconn->cmdpos = mconn->cmdlen;
                }
                break;
            case '3': // Delete
                if ( mconn->cmdpos < mconn->cmdlen )
                {
                    // Shift all characters left by one
                    for ( i = mconn->cmdpos; i < mconn->cmdlen - 1; i++ )
                    {
                        mconn->cmdbuf[i] = mconn->cmdbuf[i + 1];
                    }

                    // Decrease length of string
                    mconn->cmdlen--;

                    if ( !mconn->cmdsec )
                    {
                        // Reprint string
                        write( mconn->sock, mconn->cmdbuf + mconn->cmdpos, mconn->cmdlen - mconn->cmdpos );
                        // Add a space to cover up deleted char
                        write( mconn->sock, " \b", 2 );
                        // Move cursor back to correct spot
                        for ( i = mconn->cmdlen; i > mconn->cmdpos; i-- ) write( mconn->sock, "\b", 1 );
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

void modemShell( conn * mconn )
{

}
