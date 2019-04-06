#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <poll.h>

#include <netdb.h>
#include <netinet/in.h>

#include <string.h>

#define TIMEOUT 5
#define BUFFER_LEN 256

struct conn
{
    int sock; // Stores the client socket fd
    char username[21]; // Username of the client
    char command[100]; // Stores the current command line

    char buf[BUFFER_LEN];   // Stores incoming data from the socket
    int buflen;               // Stores number of bytes coming in

    int admin; // Boolean, is this user an admin?
    int garbage; // Boolean, delete if true

    time_t first;
    time_t last;

    struct conn * nconn; // Next conn in the linked list
};

void modemshell(struct conn * mconn);
