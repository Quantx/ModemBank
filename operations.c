#include "modembank.h"

int handleOperation( conn ** headconn, int * conn_count, user ** headuser, int * user_count, user * muser )
{
    switch ( muser->opcmd )
    {
        case req_opensock:
            (*conn_count) += operationOpensock( headconn, muser );
            break;
    }

    // We'll handle cleanup here if needed
    if ( muser->opcmd != req_idle ) muser->opcmd = req_idle;
    if ( muser->opdat != NULL )
    {
        free( muser->opdat );
        muser->opdat = NULL;
    }
}

int operationOpensock( conn ** headconn, user * muser )
{
    // Save what we need
    struct addrinfo res = *((struct addrinfo *) muser->opdat);
    // Free the rest
    freeaddrinfo( muser->opdat ); muser->opdat = NULL;

    // Get actual addresss
    struct sockaddr_in * destip = (struct sockaddr_in *)res.ai_addr;

    // Get a human readable address
    char addrstr[100];
    inet_ntop( res.ai_family, &( destip->sin_addr ), addrstr, 100 );

    // Begin connection process
    int sock = socket( res.ai_family, res.ai_socktype, 0 );

    if ( sock < 0 )
    {
        xlog( muser, "req_opensock: unable to create socket\n" );
        return 0;
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

        return 0;
    }

    if ( !connect( sock, res.ai_addr, sizeof(*(res.ai_addr)) ) )
    {
        uprintf( muser, "Failed to connect to %s (%s) on port %hu\r\n", addrstr, res.ai_canonname, htons(destip->sin_port) );
        xlog( muser, "Failed to connect to %s (%s) on port %hu\r\n", addrstr, res.ai_canonname, htons(destip->sin_port) );

        // Close and free conn
        close( sock );
        free( newconn );

        return 0;
    }

    uprintf( muser, "Connecting to %s (%s) on port %hu\r\n", addrstr, res.ai_canonname, htons(destip->sin_port) );
    xlog( muser, "Connecting to %s (%s) on port %hu\r\n", addrstr, res.ai_canonname, htons(destip->sin_port) );

    // Insert node
    newconn->next = *headconn;
    *headconn = newconn;

    // Set the stdout of the user
    muser->stdout = newconn;
    // Set the user into bridge mode
    muser->flags |= FLAG_BRDG;

    return 1;
}
