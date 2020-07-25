#include "client.h"

// Input and output connections
struct connection conn_in = {0}, conn_out = {0};

int main( int argc, char ** argv )
{
    // Skip program name
    argc--; argv++;

    // Retrive input file descriptor
    conn_in.fd = strtol( *argv, NULL, 16 );

    // Setup input file descriptor
    conn_in.enabled = 1;

    // Check if this file descriptor is a TTY and set errno
    conn_in.modem = isatty( conn_in.fd );
    // Bad file descriptor
    if ( !conn_in.modem && errno == EBADF )
    {
        perror( "Failed to retrieve file descriptor" );
        return 1;
    }

    while ( conn_in.enabled )
    {


        // Go to sleep for now
        usleep( 1000000 / TICK_RATE );
    }

    return 0;
}
