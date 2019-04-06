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

    struct conn * headconn = NULL; // First conn in the list
    struct conn * icn; // For iterrating through conns
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
        for ( icn = headconn; icn != NULL && i < pfd_size; icn = icn->next )
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

            // Increment i
            i++;
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
                struct conn * newconn = (struct conn *) malloc( sizeof(struct conn) );

                // Save socket
                newconn->sock = connsock;
                // Empty buffer
                newconn->buflen = 0;
                // Take note of current time
                newconn->first = newconn->last = time(NULL);
                // Set flags
                newconn->admin = 0;
                newconn->garbage = 0;
                // Init linked list
                newconn->prev = newconn->next = NULL;
                // Default terminal size
                newconn->width = 80;
                newconn->height = 24;

                // Transmit telnet init string
                write( connsock, "\xFF\xFD\x22\xFF\xFB\x01\xFF\xFB\x03\xFF\xFD\x1F\xFF\xFD\x18", 15 );

                // ICN should still point to end of list, set next pointer
                // to the new conn struct
                if ( headconn == NULL )
                {
                    // List is empty
                    headconn = newconn;
                }
                else
                {
                    // Get last element in list
                    for ( icn = headconn; icn->next != NULL; icn = icn->next );
                    // Insert at end
                    icn->next = newconn;
                    newconn->prev = icn;
                }

                // Increment count
                conn_count++;
            }
        }

        i = 1;
        for ( icn = headconn; icn != NULL && i < pfd_size; icn = icn->next )
        {
            if ( pfds[i].revents & POLLHUP ) // Socket closed
            {
                printf( "Client disconnected\n" );
                // Ensure socket is closed
                close( icn->sock );
                // Mark for garbage collection
                icn->garbage = 1;
                // Increment i
                i++;
                // Skip the rest
                continue;
            }

            if ( pfds[i].revents & POLLIN ) // Data to be read
            {
                // Read in data
                icn->buflen = read( icn->sock, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );

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

            // Increment i
            i++;
        }

        // Conn garbage collection
        struct conn * garbconn;
        icn = headconn;
        while ( icn != NULL )
        {
            garbconn = icn;
            icn = icn->next; // Gotta do this before we free the conn

            if ( garbconn->garbage )
            {
                if ( garbconn == headconn ) // We're the first node
                {
                    // Update node after us to be new head
                    headconn = garbconn->next;
                    // Check if last node
                    if ( headconn != NULL ) headconn->prev = NULL;
                }
                else // We're elsewhere
                {
                    garbconn->prev->next = garbconn->next; // Update node before us

                    // Check if we're the last node
                    if ( garbconn->next != NULL )
                    {
                        garbconn->next->prev = garbconn->prev; // Update node after us
                    }
                }

                conn_count--;
                free( garbconn );

                printf("Cleaned up an account\n");
            }
        }

        // We re-compute this list each time, so free it here
        free( pfds );
    }
}

int telnetOptions( struct conn * mconn )
{
    if ( mconn->buflen <= 0 ) return 0;

    if ( mconn->buf[0] != '\xFF' ) return 0; // All options start with 0xFF

    printf("Telnet options string:");

    int curOpt = -1;
    int optPos = 0;

    for ( int i = 0; i < mconn->buflen; i++ )
    {
        unsigned char cbyt = mconn->buf[i] & 0xFF;

        if ( cbyt == 0xFF ) // Start of new command
        {
            curOpt = -1;
            optPos = 0;
            printf("\n");
        }
        else if ( curOpt < 0 && optPos == 2 ) // Command type
        {
            curOpt = cbyt;
        }
        else
        {
            switch ( curOpt ) // Handle command
            {
                case 0x1F: // Negotiate About Windows Size (NAWS)
                    switch ( optPos )
                    {
                        case 3: mconn->width = cbyt << 8; break;
                        case 4: mconn->width += cbyt; break;
                        case 5: mconn->height = cbyt << 8; break;
                        case 6:
                            mconn->height += cbyt;
                            printf( "New terminal size: %d by %d\n", mconn->width, mconn->height );
                            break;
                    }
                    break;
                case 0x18: // Terminal Type
                    if ( optPos <= 3 )
                    {
                        memset( mconn->termtype, '\0', 21 );
                    }
                    else if ( optPos < 23 )
                    {
                        mconn->termtype[optPos - 4] = cbyt;
                    }
                    break;
            }
        }

        optPos++;

        printf("%02x|", cbyt );
    }

    printf( "\n" );

    mconn->buflen = 0;
}

void modemShell( struct conn * mconn )
{
    if ( mconn->buflen != 1 ) return;

    char echoback[BUFFER_LEN + 32];

    sprintf( echoback, "Got the following string: '%s'\r\n", mconn->buf );

    write( mconn->sock, echoback, strlen( echoback ) );

    mconn->buflen = 0;
}
