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

    struct conn * headconn;
    struct conn * icn;
    int i;
    int conn_count = 0;

    while (1)
    {
        // The "1" is for the server socket
        int pfd_size = 1 + conn_count;

        struct pollfd * pfds = malloc( sizeof(struct pollfd) * pfd_size );

        // Configure server socket
        pfds[0].fd = servsock;
        pfds[0].events = POLLIN;

        // Add conn sockets to array
        for ( icn = headconn; icn->nconn != NULL; icn = icn->nconn )
        {
            pfds[0]->fd = icn->sock;
            pfds[0]->events = 0;

            // Can we read?
            if ( icn->lenin < BUFFER_LEN_IN ) pfds[0].events &= POLLIN;

            // Do we need to write?
            if ( icn->lenout > 0 ) pfds[0].events &= POLLOUT;
        }

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
                // Create struct
                struct conn * newconn = malloc( sizeof(struct conn) );

                // Init struct
                newconn.sock = connsock;
                newconn.buflen = 0;
                newconn.first = newconn.last = time(NULL);
                newconn.admin = 0;
                newconn.garbage = 0;

                // Telnet init string
                write( newconn, "\xFF\xFD\x22\xFF\xFB\x01\xFF\xFB\x03\xFF\xFD\x1F", 12 );

                // ICN should still point to end of list, set next pointer
                // to the new conn struct
                icn->nconn = newconn;
                // Increment count
                conn_count++;
            }
        }

        i = 1;
        for ( icn = headconn; icn->nconn != NULL; icn = icn->nconn )
        {
            if ( pfds[i].revents & POLLHUP ) // Socket closed
            {
                // Ensure socket is closed
                close( icn->sock );
                // Mark for garbage collection
                icn->garbage = 1;
            }
            else if ( pfds[i].revents & POLLIN ) // Data to be read
            {
                icn->buflen = read( icn->sock, icn->buf + icn->buflen, BUFFER_LEN - icn->buflen );
            }

            i++;
        }

        // Conn garbage collection
        for ( icn = headconn; icn->nconn != NULL; icn = icn->nconn )
        {
            
        }

        free( pfds );
    }
}

void modemshell( struct conn * mconn )
{

}
