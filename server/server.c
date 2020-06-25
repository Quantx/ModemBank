#include "server.h"

// Array of all modems available to the system
struct modem * modm_head = NULL, ** modm_tail = &modm_head;
// Linked list of accounts
struct account * acct_head = NULL, ** acct_tail = &acct_head;
// Linked list of sessions
struct session * sess_head = NULL, ** sess_tail = &sess_head;

// Shutdown flag
static volatile int running = 1;

int main( int argc, char ** argv )
{
    argc--; argv++; // Drop program name

    int modm_size = setup_modem( "modem.cfg", &modm_tail );

    int serv_sock = setup_network( "0.0.0.0:12345" );

    if ( serv_sock < 0 ) printf( "*** Failed to start listening server ***\r\n" );

    while ( running )
    {
        // Accept incoming modem connections
        accept_modem( modm_head, &sess_tail );
        // Accept incoming network connections
        if ( serv_sock >= 0 ) accept_network( serv_sock, &sess_tail );

        // Go to sleep for now
        usleep( 1000000 / TICK_RATE );
    }

    return 0;
}
