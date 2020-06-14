#include "server.h"
#include <termios.h>
#include <fcntl.h>

speed_t baud_rates[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B600, B300, B0 };

int modem_setup( char * path, int ** list )
{
    printf( "*** Configuring modems ***\r\n" );

    FILE * mcf = fopen( path, "r" );

    // Cannot open config file
    if ( !mcf )
    {
        printf( "Failed to load modem config file: %s\r\n", path );
        return -1;
    }

    int mcount = 1;
    int nlh;

    while ( (nlh = fgetc( mcf )) != EOF ) if ( (char)nlh == '\n' ) mcount++;

    // Allocate array for modems (default size of 16)
    *list = malloc( mcount * sizeof(int) );

    char * buf = NULL;
    size_t len = 0;
    ssize_t nread;

    mcount = 0;

    // Read a line from the config file
    while ( (nread = getline( &buf, &len, mcf )) >= 0 )
    {
        // Get path to modem
        char * tok = strtok( buf, "," );

        printf( "Configuring modem '%s' ... ", tok );

        // Open modem for Read/Write, make it non-blocking, do not become controlling process, and close on fork
        int mfd = open( tok, O_RDWR | O_NONBLOCK | O_NOCTTY | O_CLOEXEC );

        if ( mfd < 0 ) { printf( "failed, could not open file\r\n" ); continue; }
        if ( !isatty( mfd ) ) { printf( "failed, file is not a tty\r\n" ); close( mfd ); continue; }

        struct termios modm_cfg = {0};

        modm_cfg.c_cflag = CS8 | CREAD | CRTSCTS;

        // Use timeout mode
        modm_cfg.c_cc[VMIN] = 0;
        modm_cfg.c_cc[VTIME] = MODEM_INIT_TIMEOUT * 10;

        // Auto determine baudrate
        int i;
        for ( i = 0; baud_rates[i] != B0; i++ )
        {
            // Set baudrate
            cfsetispeed( &modm_cfg, baud_rates[i] );
            cfsetospeed( &modm_cfg, baud_rates[i] );

            if ( tcsetattr( mfd, TCSAFLUSH, &modm_cfg ) < 0 )
            {
                printf( "failed, could not set modem attributes\r\n" );
                i = -1;
                close( mfd );
                break;
            }

            write( mfd, "AT\rAT\rAT\r", 9 );

            char resp[3];
            int rlen = read( mfd, resp, 3 );

            if ( rlen && resp[0] == 'O' && resp[1] == 'K' && resp[2] == '\n' ) break;
        }

        if ( i < 0 ) break;
        if ( baud_rates[i] == B0 ) { printf( "failed, coult not determine baudrate\r\n" ); close( mfd ); continue; }

        // Switch to polling mode
        modm_cfg.c_cc[VTIME] = 0;

        tcsetattr( mfd, TCSANOW, &modm_cfg );

        // Get init string
        tok = strtok( NULL, "\n" );
    }

    free( buf );

    return mcount;
}
