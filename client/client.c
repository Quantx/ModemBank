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
    conn_in.flags.enabled = 1;

    // Check if this file descriptor is a TTY and set errno
    int istty = isatty( conn_in.fd );
    // Bad file descriptor
    if ( istty ) conn_in.flags.modem = 1;
    else if ( errno == EBADF )
    {
        perror( "Failed to retrieve file descriptor" );
        return 1;
    }

    while ( conn_in.flags.enabled )
    {


        // Go to sleep for now
        usleep( 1000000 / TICK_RATE );
    }

    return 0;
}
