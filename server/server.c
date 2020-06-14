#include "server.h"

// Array of all modems available to the system
struct modem * modm_head = NULL, ** modm_tail = &modm_head;
// Linked list of accounts
struct account * acct_head = NULL, ** acct_tail = &acct_head;
// Linked list of connections
struct connection * conn_head = NULL, ** conn_tail = &conn_head;

int main( int argc, char ** argv )
{
    modem_setup( "modem.cfg", &modm_tail );

    return 0;
}
