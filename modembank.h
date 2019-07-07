#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>

#include <time.h>
#include <poll.h>
#include <signal.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <string.h>

#define _POSIX_SOURCE 1

// Modem constatns
#define INIT_ATTEMPTS 5
#define INIT_DURATION 1

// Network constants
#define HOST_PORT 5001

// Modem & Network constatns
#define TIMEOUT 5
#define BUFFER_LEN 256
#define CONNNAME_LEN 20

// User constatns
#define TERMTYPE_LEN 20
#define USERNAME_LEN 20
#define COMMAND_LEN 100

// Linked-list flag constants
#define FLAG_GARB (1 << 0) // Garbage node
// User flag constants
#define FLAG_ADMN (1 << 1) // User is admin
#define FLAG_OPER (1 << 2) // User is oper
// Conn flag constants
#define FLAG_MODM (1 << 1) // Conn is a modem (else: socket)
#define FLAG_OUTG (1 << 2) // Conn is outgoing
#define FLAG_CALL (1 << 3) // Modem is currently connected

typedef struct conn
{
    // Status flags
    int flags;

    // Networking
    int fd; // Stores the client socket fd
    char buf[BUFFER_LEN + 1]; // Stores incoming data from the socket
    int buflen; // Stores number of bytes in buf

    char name[CONNNAME_LEN + 1]; // A printable identifier for the conn

    // Origin of the conn
    union {
        char path[15]; // Path to the modem
        struct sockaddr_in addr; // IP & Port of the socket
    } org;

    // Linked list
    struct conn * next; // Next conn in the linked list
} conn;

typedef struct user
{
    // Status flags
    int flags;

    struct conn * stdin; // Input conn
    struct conn * stdout; // Output conn

    time_t first; // Unix time of initial connect
    time_t last; // Unix time of last char recived

    // Account
    char name[USERNAME_LEN + 1]; // Username of the client

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
    struct user * next; // Next user in the linked list
} user;

void  xlog(user * muser, const char * format, ...);
void vxlog(user * muser, const char * format, va_list args);
void  ylog(conn * mconn, const char * format, ...);
void vylog(conn * mconn, const char * format, va_list args);
void  zlog(const char * format, ...);
void vzlog(const char * format, va_list args);

int createSession(user ** headuser, conn * newconn);
int uprintf(user * muser, const char * format, ...);

void sigHandler(int sig);

#include "connections.h"
#include "shell.h"
#include "commands.h"
