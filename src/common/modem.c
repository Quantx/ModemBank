#include "modem.h"

// Check if data carrier detect is enabled
int getDCD( int fd )
{
    int status;
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_CAR;
}

// Check if data terminal ready is enabled
int getDTR( int fd )
{
    int status;
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_DTR;
}

// Check if ring indicator is enabled
int getRING( int fd )
{
    int status;
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_RNG;
}

// Enable or disable data termina ready
void setDTR( int fd, int enable )
{
    int status = TIOCM_DTR;
    ioctl( fd, enable ? TIOCMBIS : TIOCMBIC, &status );
}
