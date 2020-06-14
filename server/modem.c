#include "server.h"
#include <termios.h>
#include <fcntl.h>

speed_t baud_rates[] = { B115200,  B57600,  B38400,  B19200,  B9600,  B4800,  B2400,  B1200,  B600,  B300, B0 };
char *  baud_list[] =  { "115200", "57600", "38400", "19200", "9600", "4800", "2400", "1200", "600", "300" };
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

    int mcount = 0;
    int nlh;

    while ( (nlh = fgetc( mcf )) != EOF ) if ( (char)nlh == '\n' ) mcount++;
    fseek( mcf, 0, SEEK_SET );

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

        printf( "Configuring modem at %s... ", tok );

        // Open modem for Read/Write, make it non-blocking, do not become controlling process, and close on fork
        int mfd = open( tok, O_RDWR | O_NONBLOCK | O_NOCTTY );//| O_CLOEXEC );

        if ( mfd < 0 ) { printf( "failed, could not open file\r\n" ); continue; }
        if ( !isatty( mfd ) ) { printf( "failed, file is not a tty\r\n" ); close( mfd ); continue; }

        struct termios modm_cfg;

        modm_cfg.c_iflag = 0;
        modm_cfg.c_oflag = 0;
        modm_cfg.c_cflag = CS8 | CREAD | CRTSCTS;
        modm_cfg.c_lflag = 0;

        // The following are not used
        // (superseded by O_NONBLOCK)
        modm_cfg.c_cc[VMIN] = 0;
        modm_cfg.c_cc[VTIME] = 0;

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

            // Sleep (in microseconds)
            usleep(MODEM_INIT_TIMEOUT * 1000);

            char resp[33];
            int rlen = read( mfd, resp, 32 );
            resp[rlen] = 0;

            int k;
            for ( k = 0; k < rlen - 2; k++ )
            {
                if ( resp[k  ] == 'O'
                  && resp[k+1] == 'K'
                  && resp[k+2] == '\r' ) break;
            }
            if ( k < rlen - 2 ) break;
        }

        if ( i < 0 ) break;
        if ( baud_rates[i] == B0 ) { printf( "failed, coult not determine baudrate\r\n" ); close( mfd ); continue; }

        printf( "success, baud rate is %s\r\n", baud_list[i] );

        // Transmit the init string
        tok = strtok( NULL, "\n" );

        i = 0;
        while( tok[i++] );
        write( mfd, tok, i );
        write( mfd, "\r", 1 );
    }

    free( buf );

    return mcount;
}
