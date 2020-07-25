#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

struct connection
{
    int fd;

    struct {
        unsigned int enabled : 1;
        unsigned int modem : 1;
    } flags;
};

// How many times per second to execute the main loop
#define TICK_RATE 10
