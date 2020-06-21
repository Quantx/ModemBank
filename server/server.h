#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

/*
	*** Struct definitions ***
*/

struct {

} config;

struct modem
{
    // Human readable name of the modem
    #define MODEM_NAME_LEN 32
    char name[MODEM_NAME_LEN + 1];

    // Init string to send to the modem
    char * init;

    int fd;

    struct modem * next;
};

struct account
{
    // Username of this account
    #define ACCOUNT_USERNAME_LEN 16
    char path[ACCOUNT_USERNAME_LEN + 1];

    struct {
        // User is a developer (debug commands)
        unsigned int dev : 1;
        // User is a system operator (auditing commands)
        unsigned int oper : 1;
    } privs;

    // Number of sessions this account has open (account is unloaded if 0)
    unsigned int refs;
    // Maximum number of logins for this account
    unsigned int max_refs;

    struct account * next;
};

struct caller_id
{
    char date[4];
    char time[4];
    #define CID_NAME_LEN 32
    char name[CID_NAME_LEN];
    #define CID_PNUM_LEN 24
    char pnum[CID_PNUM_LEN];
};

struct session
{
    // Process ID of sesssion
    int pid;

    // TX pipe to client
    int pipe_send;
    // RX pipe from client
    int pipe_recv;
    // RX error pipe from client
    int pipe_error;

    // Log info of user connecting to ModemBank
    union {
        struct caller_id in_cid;
        struct sockaddr_in in_addr;
    };

    // Log info of user connecting out of ModemBank
    union {
        #define SESSION_DIALSTR_LEN 256
        char out_dialstr[SESSION_DIALSTR_LEN];
        struct sockaddr_in out_addr;
    };

    // Time when the session was created
    time_t born;

    // Account associated with this session, null if not logged in
    struct account * acct;

    // Pointer to a modem used to dial in
    struct modem * modm_in;
    // Pointer to a modem used to dial out
    struct modem * modm_out;

    struct session * next;
};

/*
	Function definitions
*/


// *** Setup all modems, returns number of modems initialized

// How long to wait for the modem to reply to AT\r (in milliseconds)
#define MODEM_INIT_TIMEOUT 100

// Returns: number of modems initialized
int modem_setup( char * path, struct modem *** list );

// *** Start the listening server
int network_setup( char * addr );

// *** Accept a new modem connection
// Returns: number modem of sessions created
int accept_modem( struct modem * mhead, struct session *** ctail );

// *** Accept a new network connection
// Returns: number network of sessions created
int accept_network( int serv_sock, struct session *** ctail );

// *** Spawn child process and setup IPC
// Returns: 1 if client created, 0 if client not created
int spawn_client( int cli_fd, struct session * cli_sess );
