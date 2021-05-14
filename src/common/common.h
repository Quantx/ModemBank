#ifndef COMMON_H
#define COMMON_H
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <stdint.h>

#define MODEMBANK_VERSION 0

#define MODEMBANK_PPID 1234
#define MODEMBANK_PORT_NUM 2600
#define MODEMBANK_PORT_STR "2600"

#define MODEMBANK_DIAL_STR_LEN 64

#define CALLER_ID_NAME_LEN 32
#define CALLER_ID_NUMBER_LEN 32

struct caller_id {
    char name[CALLER_ID_NAME_LEN + 1];
    char number[CALLER_ID_NUMBER_LEN + 1];
};

struct message {
    uint8_t op;
    uint16_t modem;
    uint32_t sid;
    uint8_t data[];
};

#include "c2w.h"
#include "w2c.h"
#endif
