#include "modem.h"

// Check if data carrier detect is enabled
inline int getDCD( int fd )
{
    int status;
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_CAR;
}

// Check if data terminal ready is enabled
inline int getDTR( int fd )
{
    int status;
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_DTR;
}

// Check if ring indicator is enabled
inline int getRING( int fd )
{
    int status
    ioctl( fd, TIOCMGET, &status );
    return status & TIOCM_RNG;
}

// Enable or disable data termina ready
inline void setDTR( int fd, int enable )
{
    int status = TIOCM_DTR;
    ioctl( fd, enable ? TIOCMBIS : TIOCMBIC, &status );
}
