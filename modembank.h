#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <poll.h>

#include <netdb.h>
#include <netinet/in.h>

#include <termios.h>
#include <fcntl.h>

#include <string.h>

#define MAX_MODEMS 8
#define BAUDLIST_SIZE 8

#define TIMEOUT 5
#define HOST_PORT 5001
#define BUFFER_LEN 256
#define TERMTYPE_LEN 20
#define USERNAME_LEN 20
#define COMMAND_LEN 100

const speed_t baudlist[BAUDLIST_SIZE];

typedef struct conn
{
    // Networking
    int sock; // Stores the client socket fd
    int network; // Boolean, true for socket, false for tty
    char buf[BUFFER_LEN + 1]; // Stores incoming data from the socket
    int buflen; // Stores number of bytes in buf
    int garbage; // Boolean, delete if true

    time_t first; // Unix time of initial connect
    time_t last; // Unix time of last char recived

    // Account
    char username[USERNAME_LEN + 1]; // Username of the client
    char usertemp[USERNAME_LEN + 1]; // Stores the login prompt username
    char passtemp[65]; // Stores the login prompt password
    int admin; // Boolean, is this user an admin?

    // Command Line
    char cmdbuf[COMMAND_LEN + 1]; // Stores the current command line
    int cmdsec; // Boolean, does not echo characters typed if true
    int cmdwnt; // Number of bytes wanted, should not excede COMMAND_LEN, a value of 0 indicates that cmdbuf holds a valid string
    int cmdlen; // Length of current command
    int cmdpos; // Position of the cursor

    // Terminal attributes
    int width;
    int height;
    char termtype[TERMTYPE_LEN + 1]; // Terminal type

    // Linked list
    int sentinel; // This is a sentinel node
    struct conn * next; // Next conn in the linked list
    struct conn * prev; // Prev conn in the linked list
} conn;

int telnetOptions(conn * mconn);
void commandLine(conn * mconn);
void modemShell(conn * mconn);
int configureModems(int * mods);
