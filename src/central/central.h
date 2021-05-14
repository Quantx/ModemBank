#include "../secpipe/secpipe.h"
#include "../common/common.h"
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/sctp.h>

#define DEFAULT_IP "0.0.0.0"
#define LISTEN_BACKLOG 10
#define USERNAME_LEN 32

struct modem {
    struct sctp_sndrcvinfo sri;
    uint32_t sid;
    struct caller_id cid;
};

struct worker {
    struct sctp_sndrcvinfo sri;
    unsigned int modem_count;
    struct modem * modem_list;

    struct worker * next;
};

struct internet {
    int sock;
    struct sockaddr_in addr;
};

enum connection_type {
    connection_type_none,
    connection_type_modem,
    connection_type_internet
};

struct connection {
    enum connection_type conn_type;

    union {
        struct modem * modm;
        struct internet inet;
    };
};

struct session {
    time_t born;

    struct connection in, out;

    struct session * next;
};

struct account {
    char username[USERNAME_LEN];

    struct session * sess_head, ** sess_tail;

    struct account * next;
};

int listen_server( char * ip, char * port );
