#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

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
#define CONN_TIMEOUT 5

// Modem & Network constatns
#define MASTER_TIMEOUT 5
#define BUFFER_LEN 256
#define CONNNAME_LEN 20

// User constatns
#define TERMTYPE_LEN 20
#define USERNAME_LEN 20
#define COMMAND_LEN 100
#define PROMPT_LEN 10

// Linked-list flag constants
#define FLAG_GARB (1 << 0) // Garbage node
// User flag constants
#define FLAG_DEVL (1 << 1) // User is developer (oper + dev = admin)
#define FLAG_OPER (1 << 2) // User is sysop
#define FLAG_BRDG (1 << 3) // User is in bridge mode (stdin in and stdout are inter-connected)
// Conn flag constants
#define FLAG_MODM (1 << 1) // Conn is a modem (else: socket)
#define FLAG_OUTG (1 << 2) // Conn is outgoing
#define FLAG_CALL (1 << 3) // Conn is currently connected

// ModemBank operation list
enum operations { req_idle, req_opensock, };

typedef struct conn
{
    // Status flags
    int flags;

    // Networking
    int fd; // Stores the client socket fd
    char buf[BUFFER_LEN + 1]; // Stores incoming data from the socket
    int buflen; // Stores number of bytes in buf

    time_t first; // Unix time of first connect
    time_t last; // Unix time of last data

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
    char cmdppt[PROMPT_LEN + 1]; // Store the prompt string to print
    int cmdsec; // Boolean, does not echo characters typed if true
    int cmdwnt; // Number of bytes wanted, should not excede COMMAND_LEN, a value of 0 indicates that cmdbuf holds a valid string
    int cmdlen; // Length of current command
    int cmdpos; // Position of the cursor

    // Terminal attributes
    int width;
    int height;
    char termtype[TERMTYPE_LEN + 1]; // Terminal type

    // Operation to be preformed by ModemBank
    enum operations opcmd;
    // Data associated with the operation, will be freed on completion
    void * opdat;

    // Linked list
    struct user * next; // Next user in the linked list
} user;

typedef struct data
{
    struct conn * headconn;
    int conn_count;

    struct user * headuser;
    int user_count;
} data;

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
