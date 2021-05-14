#include "common.h"

enum w2c {
    w2c_init,
    w2c_newcall,
    w2c_newcall_pickup_ack,
    w2c_dial_okay,
    w2c_dial_busy,
    w2c_dial_fail,
    w2c_hangup
};

struct message_w2c_init {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;

    uint16_t modem_count; // Number of modems controlled by this worker
    uint32_t version; // Version of modem bank being run
};

// *** New Call transaction
struct message_w2c_newcall {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;

    struct caller_id cid;
};

struct message_w2c_newcall_pickup_ack {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};
// *** Dial transaction
struct message_w2c_dial_okay {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};

struct message_w2c_dial_fail {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};

struct message_w2c_dial_busy {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};
// *** Hangup transaction
struct message_w2c_hangup {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
};
