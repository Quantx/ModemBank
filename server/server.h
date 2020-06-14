#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
	*** Struct definitions ***
*/

struct account
{
    // Username of this account
    #define ACCOUNT_USERNAME_LEN 16
    char path[ACCOUNT_USERNAME_LEN + 1];

    struct
    {
        // User is a developer (debug commands)
        unsigned int dev : 1;
        // User is a system operator (auditing commands)
        unsigned int oper : 1;
    } privs;

    // Number of sessions this account has open
    unsigned int refs;
    // Maximum number of logins for this account
    unsigned int max_refs;
};

struct session
{
    // TX pipe to client
    int pipe_send;
    // RX pipe from client
    int pipe_recv;

    // Account associated with this session, null if not logged in
    struct account * acct;

    // Pointer to a modem used to dial in
    int * modm_in;
    // Pointer to a modem used to dial out
    int * modm_out;
};

/*
	Function definitions
*/



// *** Setup all modems, returns number of modems initialized
// How long to wait for the modem to reply to AT\r (in milliseconds)
#define MODEM_INIT_TIMEOUT 100
int modem_setup( char * path, int ** list );
