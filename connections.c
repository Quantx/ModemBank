#include "modembank.h"

const speed_t baud_list[ BAUDLIST_SIZE] = { B300, B1200, B4800, B9600, B19200, B38400, B57600, B115200 };
const int     baud_alias[BAUDLIST_SIZE] = {  300,  1200,  4800,  9600,  19200,  38400,  57600,  115200 };

int loadModemConfig( conn ** headconn )
{
    char modbuf[256];
    char * path, * magic;
    int baud, mod_count = 0;

    FILE * fd = fopen( "config/modems.csv", "r" );

    if ( fd == NULL )
    {
        zlog( "Cannot open modem config file: \"config/modem.csv\"\n" );
        return 0;
    }

    zlog( "Initializing modems...\n" );

    while ( fgets( modbuf, 256, fd ) != NULL )
    {
        // Get the tty file to open
        path = strtok( modbuf, "," );

        // Get the baud rate index
        magic = strtok( NULL, "," );

        // No baud rate specified, skip this one
        if ( magic == NULL )
        {
            zlog( "Bad line, no baud rate specified for %s\n", path );
            continue;
        }

        // Convert to integer
        baud = atoi( magic );

        // Get init string
        magic = strtok( NULL, "\n" );

        mod_count += configureModem( headconn, path, baud, magic );
    }

    // Remember to close the config file
    fclose( fd );

    zlog( "Initialized %d modems\n", mod_count );

    return mod_count;
}


int configureModem( conn ** headconn, const char * path, int baud, const char * magic )
{
    struct termios modopt;
    char atbuf[256];
    int i, mfd, atsize;

    // Open the serial port
    mfd = open( path, O_RDWR );

    // Couldn't open port
    if ( mfd < 0 )
    {
        zlog( "Failed to open modem %s\n", path );
        return 0;
    }

    // This isn't a serial port
    if ( !isatty(mfd) )
    {
        zlog( "Failed, %s is not a TTY\n", path );
        close( mfd );
        return 0;
    }

    // DO NOT CHANGE THE VALUE OF "I" BEFORE SETTING BAUD RATE
    for ( i = 0; i < BAUDLIST_SIZE; i++ )
    {
        if ( baud == baud_alias[i] ) break;
    }

    if ( i == BAUDLIST_SIZE )
    {
        zlog( "Failed, invalid baud rate %d for modem %s\n", baud, path );
        close( mfd );
        return 0;
    }

    // Get current config
    tcgetattr( mfd, &modopt );

    // Un-set iFlag params
    modopt.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    // Un-set oFlag params
    modopt.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OLCUC | OPOST);
    // Un-set cFlag params
    modopt.c_cflag &= ~(CSIZE | PARENB | CLOCAL);
    // Set cFlag params
    modopt.c_cflag |=  (CS8 | CRTSCTS | CREAD);
    // Un-set lFlag params
    modopt.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    // Set 1 second timeout
    modopt.c_cc[VTIME] = INIT_DURATION * 10;
    modopt.c_cc[VMIN] = 0;

    // Configure baud rate
    cfsetispeed( &modopt, baud_list[i] );
    cfsetospeed( &modopt, baud_list[i] );

    // Flash settings to the tty after flushing
    tcsetattr( mfd, TCSAFLUSH, &modopt );

    zlog( "Attempting to contact modem %s\n", path );

    // Attempt to intialize the modem
    for ( i = 0; i < INIT_ATTEMPTS; i++ )
    {
        printf( "Sending 'AT' to modem (attempt %d)... ", i );
        fflush(stdout);

        // Send an AT command for baud rate detection
        write( mfd, "AT\r", 3 );

        // Wait for read
        atsize = read( mfd, atbuf, 255 );

        // Check if we got any data
        if ( atsize > 0 )
        {
            // Recived an OK command
            if ( strncmp( atbuf, "\r\nOK\r\n", 6 ) == 0 )
            {
                printf( "responded 'OK'\n" );
                break;
            }
            else
            {
                // Make the string printable
                atbuf[atsize] = '\0';
                printf( "unknown response: >>>%s<<<\n", atbuf );
                for ( int k = 0; k < atsize; k++ ) printf( "%02x|", atbuf[k] & 0xFF );
                printf( "\n" );
            }
        }
        else
        {
            printf( "no response\n" );
        }
    }

    // Could not reach modem
    if ( i == INIT_ATTEMPTS )
    {
        close( mfd );
        zlog( "Modem %s non-responsive to AT commands\n", path );
        return 0;
    }

    // Send reset command to modem
    write( mfd, "ATZ\r", 4 );

    // No baud rate specified, skip this one
    if ( magic != NULL )
    {
        // Transmit string
        write( mfd, magic, strlen(magic) );
        // Terminate string
        write( mfd, "\r", 1 );
    }

    // Set blocking
    modopt.c_cc[VTIME] = 0;
    modopt.c_cc[VMIN] = 1;
    // Re-flash settings
    tcsetattr( mfd, TCSANOW, &modopt );

    // Create a new conn
    conn * newconn = malloc( sizeof(conn) );

    // Save modem information
    newconn->flags = FLAG_MODM;
    newconn->fd = mfd;
    newconn->buflen = 0;
    // Store the name of this modem
    strcpy( newconn->name, path );

    // Insert node
    newconn->next = *headconn;
    *headconn = newconn;

    ylog( newconn, "Intialized at %d baud\n", baud );

    return 1;
}

int telnetOptions( user * muser )
{
    // Get our input buffer
    conn * mconn = muser->stdin;

    if ( mconn->buflen <= 0 ) return 0;

    if ( mconn->buf[0] != '\xFF' ) return 0; // All options start with 0xFF

    int i;
    char * ctok = mconn->buf;

    /*
    printf( "Telnet options string:\n" );

    for ( i = 0; i < mconn->buflen; i++ )
    {
        printf( "%02x", mconn->buf[i] & 0xFF );
        if ( i < mconn->buflen - 1 ) printf("|");
    }

    printf( "\n" );
    */

    while ( ctok < mconn->buf + mconn->buflen )
    {
        if ( ctok[1] == '\xFA' ) // Sub negotiate params
        {
            switch ( ctok[2] ) // What type of option?
            {
                case '\x1F': // Negotiate About Window Size (NAWS)
                    muser->width  = (ctok[3] << 8) + ctok[4];
                    muser->height = (ctok[5] << 8) + ctok[6];
                    xlog( muser, "Set terminal size to: %d by %d\n", muser->width, muser->height );
                    break;
                case '\x18': // Terminal type
                    for ( i = 0; i < TERMTYPE_LEN && i + 4 + ctok < mconn->buf + mconn->buflen; i++ )
                    {
                        // End of negotiation
                        if ( ctok[i + 4] == '\xFF' && ctok[i + 5] == '\xF0' ) break;
                        // Copy char
                        muser->termtype[i] = ctok[i + 4];
                    }
                    muser->termtype[i] = '\0';
                    xlog( muser, "Set terminal type to: '%s'\n", muser->termtype );
                    break;
            }
        }
        else if ( ctok[1] == '\xFB' ) // WILL
        {
            switch ( ctok[2] ) // What option?
            {
                case '\x18': // Terminal type
                    // Request terminal name
                    write( mconn->fd, "\xFF\xFA\x18\x01\xFF\xF0", 6 );
                    break;
            }
        }

        for ( ctok++; ctok < mconn->buf + mconn->buflen && ctok[0] != '\xFF'; ctok++ );
    }

    // Empty the buffer
    mconn->buflen = 0;

    return 1;
}

int getDCD( conn * mconn )
{
    // Not a modem
    if ( !(mconn->flags & FLAG_MODM) ) return -1;

    int status;

    // Get status pins
    ioctl( mconn->fd, TIOCMGET, &status );

    // Return status of Data Carrier Detect
    return status & TIOCM_CAR;
}

int setDTR( conn * mconn, int set )
{
    // Not a modem
    if ( !(mconn->flags & FLAG_MODM) ) return -1;

    int status;

    // Get status pins
    ioctl( mconn->fd, TIOCMGET, &status );

    int result = status & TIOCM_DTR;

    // Flip the bit
    if ( set )
    {
        status |= TIOCM_DTR;
    }
    else
    {
        status &= ~TIOCM_DTR;
    }

    // Set status pins
    ioctl( mconn->fd, TIOCMSET, &status );

    // Return true if changed
    return result != set;
}

int connGarbage( conn ** headconn )
{
    conn * icn, * nextconn, * prevconn = NULL;
    int conn_count = 0;

    for ( icn = *headconn; icn != NULL; icn = nextconn )
    {
        // Save this for later
        nextconn = icn->next;

        // Nothing to do here
        if ( !(icn->flags & FLAG_GARB) )
        {
            /*** Intentionally left empty ***/
        }
        // Handle modems
        else if ( icn->flags & FLAG_MODM )
        {
            // Do we need to hangup?
            if ( getDCD( icn ) )
            {
                // Drop the Data Terminal Ready line to hangup
                setDTR( icn, 0 );
            }
            else
            {
                ylog( icn, "Reset an %s modem\n", icn->flags & FLAG_OUTG ? "outgoing" : "incoming");

                // Tell the modem we're ready now
                setDTR( icn, 1 );

                // Finished reseting modem, mark it as not garbage, and available
                icn->flags &= ~(FLAG_GARB | FLAG_CALL);
            }
        }
        else
        {
            // Shutdown socket
            shutdown( icn->fd, SHUT_RDWR );
            // Close socket
            close( icn->fd );

            ylog( icn, "Terminated an %s connection\n", icn->flags & FLAG_OUTG ? "outgoing" : "incoming");

            // Check if we're the head
            if ( prevconn == NULL )
            {
                *headconn = icn->next;
            }
            else
            {
                prevconn->next = icn->next;
            }

            // Free the node
            free( icn );

            // Decrement count
            conn_count++;

            // Don't update prevconn
            continue;
        }

        prevconn = icn;
    }

    return conn_count;
}
