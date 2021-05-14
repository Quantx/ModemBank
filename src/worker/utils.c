#include "worker.h"

void setDTR( int fd, int val )
{
    ioctl( fd, val ? TIOCMBIS : TIOCMBIC, TIOCM_DTR );
}

int getDTR( int fd )
{
    int val;
    ioctl( fd, TIOCMGET, &val );
    return val & TIOCM_DTR;
}

int getDCD( int fd )
{
    int val;
    ioctl( fd, TIOCMGET, &val );
    return val & TIOCM_CAR;
}

int getRI( int fd )
{
    int val;
    ioctl( fd, TIOCMGET, &val );
    return val & TIOCM_RI;
}
