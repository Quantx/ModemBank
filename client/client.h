#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

struct connection
{
    int fd;

    struct {
        unsigned int enabled : 0;
        unsigned int modem : 1;
    } flags;
};

// How many times per second to execute the main loop
#define TICK_RATE 10
