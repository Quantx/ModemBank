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
#define TERMTYPE_LEN 20
#define USERNAME_LEN 20
#define COMMAND_LEN 100

typedef struct conn
{
    int sock; // Stores the client socket fd
    char username[USERNAME_LEN + 1]; // Username of the client

    char cmdbuf[COMMAND_LEN + 1]; // Stores the current command line
    int cmdsec; // Boolean, does not echo characters typed if true
    int cmdwnt; // Number of bytes wanted, should not excede COMMAND_LEN, a value of 0 indicates that cmdbuf holds a valid string
    int cmdlen; // Length of current command
    int cmdpos; // Position of the cursor

    char buf[BUFFER_LEN + 1]; // Stores incoming data from the socket
    int buflen; // Stores number of bytes in buf

    int sentinel; // This is a sentinel node
    int admin; // Boolean, is this user an admin?
    int garbage; // Boolean, delete if true

    // Terminal width and height
    int width;
    int height;

    // Terminal type
    char termtype[TERMTYPE_LEN + 1];

    time_t first; // Unix time of initial connect
    time_t last; // Unix time of last char recived

    struct conn * next; // Next conn in the linked list
    struct conn * prev; // Prev conn in the linked list
} conn;

int telnetOptions(conn * mconn);
void commandLine(conn * mconn);
void modemShell(conn * mconn);
