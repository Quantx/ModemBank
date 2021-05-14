#include "worker.h"

volatile int running = 1;

int main( int argc, char ** argv )
{
    if ( argc < 2 ) { fprintf( stderr, "usage: ./%s <central server IP>\n", argv[0] ); return 1; }

    if ( !load_config( "modems/" ) ) return 2;

    if ( connect_central(            argv[1],
                          argc > 3 ? argv[2] : MODEMBANK_PORT_STR,
                          argc > 4 ? argv[3] : NULL ) < 0 ) return 3;

    while ( running )
    {
        if ( poll( pfd_list, pfd_count, 50 ) > 0 )
        {
            // Recieve messages from central
            if ( pfd_list->revents & POLLIN )
            {
                receive_messages();
            }

            // Read data from modems
            int i;
            for ( i = 1; i < pfd_count; i++ )
            {
                if ( ~pfd_list[i].revents & POLLIN ) continue;

                struct modem * m = modem_list + i - 1;

                if ( MODEM_BUF_SIZE - m->recvlen > 0 )
                {
                    m->recvlen += read( m->fd, m->recvbuf + m->recvlen, MODEM_BUF_SIZE - m->recvlen );
                }
            }
        }

        for ( int i = 0; i < modem_count; i++ ) update_modem(i);
    }

    return 0;
}
