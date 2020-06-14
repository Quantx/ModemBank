#include "common.h"

struct session {
    #define ACCOUNT_LEN 16
    char account[ACCOUNT_LEN + 1];

    // User -> stdin -> stdout -> Remote
    struct conn stdin;
    struct conn stdout;

    
};
