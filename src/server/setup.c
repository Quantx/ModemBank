#include "server.h"
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct {
    speed_t rate;
    char * name;
} baud_rates[] = {
    { B115200, "115200" },
    {  B57600,  "57600" },
    {  B38400,  "38400" },
    {  B19200,  "19200" },
    {   B9600,   "9600" },
    {   B4800,   "4800" },
    {   B2400,   "2400" },
    {   B1200,   "1200" },
    {    B600,    "600" },
    {    B300,    "300" },
    {      B0,     NULL }
};

int setup_modem( char * path, struct modem *** list )
{
    printf( "*** Configuring modems ***\r\n" );

    FILE * mcf = fopen( path, "r" );

    // Cannot open config file
    if ( !mcf )
    {
        printf( "Failed to load modem config file: %s\r\n", path );
        return 0;
    }

    char * buf = NULL;
    size_t len = 0;
    ssize_t nread;

    int mcount = 0;

    // Read a line from the config file
    while ( (nread = getline( &buf, &len, mcf )) >= 0 )
    {
        // Get path to modem
        char * tok = strtok( buf, "," );

        printf( "Configuring modem at %s... ", tok );

        // Open modem for Read/Write, make it non-blocking, do not become controlling process, and close on fork
        int mfd = open( tok, O_RDWR | O_NONBLOCK | O_NOCTTY );

        if ( mfd < 0 ) { printf( "failed, could not open file\r\n" ); continue; }
        if ( !isatty( mfd ) ) { printf( "failed, file is not a tty\r\n" ); close( mfd ); continue; }

        // Close file descriptor on execvp
        if ( fcntl( mfd, F_SETFD, FD_CLOEXEC ) < 0 )
        {
            printf( "failed, could not set FD_CLOEXEC\r\n" );
            close( mfd ); continue;
        }

        struct termios modm_cfg = {0};

        modm_cfg.c_cflag = CS8 | CREAD | CRTSCTS | CLOCAL | HUPCL;

        // Auto determine baudrate
        int baud;
        for ( baud = 0; baud_rates[baud].rate != B0; baud++ )
        {
            // Set baudrate
            cfsetispeed( &modm_cfg, baud_rates[baud].rate );
            cfsetospeed( &modm_cfg, baud_rates[baud].rate );

            if ( tcsetattr( mfd, TCSAFLUSH, &modm_cfg ) < 0 )
            {
                printf( "failed, could not set modem attributes\r\n" );
                baud = -1;
                close( mfd );
                break;
            }

            write( mfd, "AT\rAT\rAT\r", 9 );

            // Sleep (in microseconds)
            usleep(MODEM_INIT_TIMEOUT * 1000);

            char resp[33];
            int rlen = read( mfd, resp, 32 );

            int k;
            for ( k = 0; k < rlen - 2; k++ )
            {
                if ( resp[k  ] == 'O'
                  && resp[k+1] == 'K'
                  && resp[k+2] == '\r' ) break;
            }
            if ( k < rlen - 2 ) break;
        }

        if ( baud < 0 ) continue;
        if ( baud_rates[baud].rate == B0 ) { printf( "failed, coult not determine baud rate\r\n" ); close( mfd ); continue; }

        // Allocate memory for modem
        **list = malloc( sizeof(struct modem) );

        (**list)->next = NULL;
        (**list)->fd = mfd;

        // Get name of modem
        tok = strtok( NULL, "," );

        strncpy( (**list)->name, tok, MODEM_NAME_LEN );
        (**list)->name[MODEM_NAME_LEN + 1] = 0;

        // Set modem to idle state
        setDTR( mfd, 1 );
        (**list)->state = modem_state_idle;

        // Get init string
        tok = strtok( NULL, "\n" );

        int init_len = strlen(tok);

        // Allocate room for init string (including final \r and null terminator)
        (**list)->init = malloc( (init_len + 2) * sizeof(char) );

        // Save init string
        strncpy( (**list)->init, tok, init_len );

        // Add a final \r
        (**list)->init[init_len] = '\r';
        (**list)->init[init_len + 1] = 0;

        // Replace comma with \r
        while ( tok = strchr( (**list)->init, ',' ) ) *tok = '\r';

        // Transmit the init string
        write( mfd, (**list)->init, init_len + 1 );

        printf( "success, %s at baud rate %s\r\n", (**list)->name, baud_rates[baud].name );

        *list = &(**list)->next;
        mcount++;
    }

    printf( "*** Configured %d modems ***\r\n", mcount );

    // Free file readline buffer
    free( buf );

    return mcount;
}

int setup_network( char * addr )
{
    printf( "*** Starting listening server ***\r\n" );

    char * addr_str = strdup( addr );

    int sock = socket( AF_INET, SOCK_STREAM | O_NONBLOCK, 0 );

    if ( sock < 0 )
    {
        perror("failed to open socket");
        return -1;
    }

    // Close file descriptor on execvp
    if ( fcntl( sock, F_SETFD, FD_CLOEXEC ) < 0 )
    {
        printf( "failed to set FD_CLOEXEC\r\n" );
        close( sock );
        return -1;
    }

    int enable = 1;
    if ( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int) ) < 0 )
    {
        perror("failed to set socket option");
        close( sock );
        return -1;
    }

    struct sockaddr_in host_addr = {0};

    host_addr.sin_family = AF_INET;

    if ( inet_pton( host_addr.sin_family, strtok( addr_str, ":" ), &(host_addr.sin_addr) ) < 0 )
    {
        perror("invalid ip addres");
        free( addr_str );
        close( sock );
        return -1;
    }

    char * port_str_end;
    int port = strtol( strtok( NULL, "\n" ), &port_str_end, 10 );
    free( addr_str );

    if ( !port )
    {
        printf("invalid port number");
        close( sock );
        return -1;
    }

    host_addr.sin_port = htons(port);

    if ( bind( sock, (struct sockaddr *) &host_addr, sizeof(struct sockaddr_in) ) < 0 )
    {
        perror("failed to bind socket");
        close( sock );
        return -1;
    }

    if ( listen( sock, 10 ) < 0 )
    {
        perror("failed to listen on socket");
        close( sock );
        return -1;
    }

    printf( "*** Listening server started on port %d ***\r\n", port );

    return sock;
}
