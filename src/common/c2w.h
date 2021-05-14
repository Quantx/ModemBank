#include "common.h"

enum c2w {
    c2w_init,
    c2w_newcall_pickup,
    c2w_newcall_hangup,
    c2w_dial,
    c2w_hangup
};

enum c2w_init_status {
    c2w_init_status_okay,
    c2w_init_status_nostreams,
    c2w_init_status_version
};

struct message_c2w_init {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;

    uint16_t status;
};

// *** New call transaction
struct message_c2w_newcall_pickup {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};

struct message_c2w_newcall_hangup {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};
// *** Dial transaction
struct message_c2w_dial {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;

    uint8_t dialstr[MODEMBANK_DIAL_STR_LEN + 1];
};
// *** Hangup transaction
struct message_c2w_hangup {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};
