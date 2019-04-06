#include "modembank.h"

int main( int argc, char * argv[] )
{
    int servsock, connsock, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int pid;

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
    portno = 5001;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind socket to port
    if (bind(servsock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    listen( servsock, 5 );
    clilen = sizeof(cli_addr);

    // Build the sentinel node
    conn headconn;
    headconn.sentinel = 1;
    headconn.prev = headconn.next = &headconn;

    conn * icn; // For iterrating through conns
    int i;
    int conn_count = 0; // Total number of conns

    printf( "Started terminal server on port: %d\n", portno );

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

                // Save socket
                newconn->sock = connsock;
                // Empty buffer
                newconn->buflen = 0;
                // Take note of current time
                newconn->first = newconn->last = time(NULL);
                // Set flags
                newconn->sentinel = 0;
                newconn->admin = 0;
                newconn->garbage = 0;
                // Default terminal size
                newconn->width = 80;
                newconn->height = 24;

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
                icn->buflen = read( icn->sock, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );

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
            if ( !telnetOptions( icn ) )
            {
                // Run the shell
                modemShell( icn );
            }
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

    mconn->buflen = 0;
}

void modemShell( conn * mconn )
{
    if ( mconn->buflen != 1 ) return;

    char echoback[BUFFER_LEN + 32];

    sprintf( echoback, "Got the following string: '%s'\r\n", mconn->buf );

    write( mconn->sock, echoback, strlen( echoback ) );

    mconn->buflen = 0;
}
