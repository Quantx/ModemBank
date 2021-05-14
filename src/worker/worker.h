#include "../secpipe/secpipe.h"
#include "../common/common.h"
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <dirent.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#define MODEM_BUF_SIZE 4096
#define HANGUP_PERIOD 4

struct baudrate {
    speed_t rate;
    char * name;
    unsigned int value;
};

enum modem_state {
    modem_state_idle,
    modem_state_ringing,
    modem_state_dialing,
    modem_state_online,
    modem_state_hangup
};

struct modem {
    // Port
    int fd;
    int mbits;

    // Config
    char * init;
    struct termios sercfg;

    // Read buffer
    char recvbuf[MODEM_BUF_SIZE];
    int recvlen;

    // State
    uint32_t sid; // Current session ID, increments once per call
    enum modem_state cst; // Current state
    time_t nstt; // Next state time
};

extern volatile int running;

extern struct pollfd * pfd_list;
extern unsigned int pfd_count;

extern struct modem * modem_list;
extern unsigned int modem_count;

extern int central;

void setDTR( int fd, int val );
int getDTR( int fd );
int getDCD( int fd );
int getRI( int fd );

int load_config( char * dname );
int connect_central( char * ip, char * port, char * iface );

void receive_messages(void);
void update_modem(int mi);
