#include "worker.h"

void update_modem( int mi )
{
    struct modem * m = modem_list + mi;

    // Get status bits
    ioctl( m->fd, TIOCMGET, &m->mbits );

    // State machine
    switch ( m->cst )
    {
        case modem_state_idle:
            // Incoming call
            if ( m->mbits & TIOCM_RI )
            {
                m->cst = modem_state_ringing;
            }
            break;
        case modem_state_ringing:
            break;
        case modem_state_dialing:
            break;
        case modem_state_online:
            // Transmit pending data in modem's buffer
            if ( m->recvlen )
            {
                sctp_sendmsg( central,
                              m->recvbuf, m->recvlen,
                              NULL, 0,
                              MODEMBANK_PPID, 0,
                              mi, 0, 0 );
                m->recvlen = 0;
            }

            // DCD or DTR dropped
            if ( !( m->mbits & TIOCM_CAR && m->mbits & TIOCM_DTR ) )
            {
                // Drop DTR if needed
                if ( m->mbits & TIOCM_DTR ) setDTR( m->fd, 0 );

                // TODO: transmit hangup message

                // Set hangup timeout
                m->nstt = time(NULL) + HANGUP_PERIOD;
                m->cst = modem_state_hangup;
            }
            break;
        case modem_state_hangup:
            m->recvlen = 0; // Drop data while hanging up

            if ( time(NULL) > m->nstt )
            {
                // Set DTR
                setDTR( m->fd, 1 );

                // Increment session ID before returning to idle state
                m->sid++;
                m->cst = modem_state_idle;
            }
            break;
    }
}
