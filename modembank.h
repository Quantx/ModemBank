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

typedef struct conn
{
    int sock; // Stores the client socket fd
    char username[21]; // Username of the client
    char command[100]; // Stores the current command line

    char buf[BUFFER_LEN + 1]; // Stores incoming data from the socket
    int buflen; // Stores number of bytes coming in

    int sentinel; // This is a sentinel node
    int admin; // Boolean, is this user an admin?
    int garbage; // Boolean, delete if true

    // Terminal width and height
    int width;
    int height;

    // Terminal type
    char termtype[21];

    time_t first; // Unix time of initial connect
    time_t last; // Unix time of last char recived

    struct conn * next; // Next conn in the linked list
    struct conn * prev; // Prev conn in the linked list
} conn;

int telnetOptions(struct conn * mconn);
void modemShell(struct conn * mconn);
