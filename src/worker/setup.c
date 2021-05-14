#include "worker.h"

struct baudrate baud_table[] = {
    { B230400, "230400", 230400 },
    { B115200, "115200", 155200 },
    {  B57600,  "57600",  57600 },
    {  B38400,  "38400",  38400 },
    {  B19200,  "19200",  19200 },
    {   B9600,   "9600",   9600 },
    {   B4800,   "4800",   4800 },
    {   B2400,   "2400",   2400 },
    {   B1800,   "1800",   1800 },
    {   B1200,   "1200",   1200 },
    {    B600,    "600",    600 },
    {    B300,    "300",    300 },
    {      B0,      "0",      0 }
};

struct pollfd * pfd_list;
unsigned int pfd_count = 1;

struct modem * modem_list;
unsigned int modem_count;

int central;

int connect_central( char * ip, char * port, char * iface )
{
    // No modems so nothing to do
    if ( !modem_count ) return -1;

    int sock = socket( AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP );
    if ( sock < 0 ) { perror( "sctp socket" ); return -1; }

    // Bind to specific interface
    if ( iface ) setsockopt( sock, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) );

    // Request needed number of streams
    struct sctp_initmsg init = { modem_count + 1, modem_count + 1 };
    setsockopt( sock, SOL_SCTP, SCTP_INITMSG, &init, sizeof(struct sctp_initmsg) );

    struct addrinfo * addr, hint = {0};
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_SEQPACKET;
    hint.ai_protocol = IPPROTO_SCTP;
    getaddrinfo( ip, port, &hint, &addr );

    struct message_w2c_init msg = {
        w2c_init,
        modem_count,
        MODEMBANK_VERSION
    };

    // Send init message to configure the socket
    sctp_sendmsg( sock, &msg, sizeof(struct message_w2c_init),
        addr->ai_addr, addr->ai_addrlen,
        MODEMBANK_PPID, 0,
        0, 0,
        0 );

    freeaddrinfo(addr);

    pfd_list->fd = central = sock;
    pfd_list->events = POLLIN;

    return sock;
}

int load_config( char * dname )
{
    DIR * dcfg = opendir(dname);

    if ( dcfg == NULL ) return 0;

    int dname_len = strlen(dname);

    struct dirent * d;
    while ( (d = readdir(dcfg)) != NULL )
    {
        char * dsfx = strrchr( d->d_name, '.' );
        if ( dsfx == NULL || strcmp( dsfx + 1, "cfg" ) ) continue;

        modem_list = realloc( modem_list, (modem_count + 1) * sizeof(struct modem) );
        struct modem * mcfg = modem_list + modem_count;

        pfd_list = realloc( pfd_list, (pfd_count + 1) * sizeof(struct pollfd) );
        struct pollfd * pfd = pfd_list + pfd_count;

        mcfg->recvlen = 0;
        mcfg->sid = 0;

        // Build path
        char * cfgpath = malloc(dname_len + strlen(d->d_name) + 2);
        strcpy( cfgpath, dname );
        cfgpath[dname_len] = '/';
        cfgpath[dname_len + 1] = '\0';
        strcat( cfgpath, d->d_name );

        // Try and open the config file
        FILE * mcf = fopen( cfgpath, "r" );
        free(cfgpath);
        if ( mcf == NULL ) continue;

        size_t len = 0;
        char * devpath = NULL;
        getline( &devpath, &len, mcf );

        // Try and open the modem pointed to in this config file
        mcfg->fd = open( devpath, O_RDWR | O_NOCTTY );

        if ( mcfg->fd < 0 || !isatty(mcfg->fd) )
        {
            printf( "Failed to open %s as a serial port\n", devpath );
            fclose(mcf);
            free(devpath);
            continue;
        }

        // Hangup
        if ( getDTR( mcfg->fd ) )
        {
            setDTR( mcfg->fd, 0 );
            sleep(3);
        }

        // Set inital state
        setDTR( mcfg->fd, 1 );
        mcfg->cst = modem_state_idle;
        mcfg->nstt = 0;

        // Configure termios
        mcfg->sercfg = (struct termios){0};
        mcfg->sercfg.c_iflag = IGNBRK;
        mcfg->sercfg.c_cflag = CS8 | CREAD | CLOCAL | HUPCL;

        pfd->fd = mcfg->fd;
        pfd->events = POLLIN;

        // Negotiate baud rate
        int bd;
        for ( bd = 0; baud_table[bd].rate != B0; bd++ )
        {
            cfsetispeed( &mcfg->sercfg, baud_table[bd].rate );
            cfsetospeed( &mcfg->sercfg, baud_table[bd].rate );

            // Apply settings and flush I/O
            tcsetattr( mcfg->fd, TCSAFLUSH, &mcfg->sercfg );

            write( mcfg->fd, "AT\r", 3 );

            if ( poll( pfd, 1, 300 ) > 0 )
            {
                if ( pfd->revents & POLLIN )
                {
                    #define BAUDCHECK_BUF 32
                    char rbuf[BAUDCHECK_BUF];

                    read( pfd->fd, rbuf, BAUDCHECK_BUF );

                    int cci;
                    for ( cci = 0; cci < BAUDCHECK_BUF - 1; cci++ )
                    {
                        if ( rbuf[cci] == 'O' && rbuf[cci + 1] == 'K' ) break;
                    }

                    if ( cci < BAUDCHECK_BUF - 1 ) break;
                }
            }
        }

        write( mcfg->fd, "ATZ\r", 4 );

        tcflush( mcfg->fd, TCIOFLUSH );

        // Failed to negotiate baud rate
        if ( baud_table[bd].rate == B0 )
        {
            printf( "Failed to detect baud rate for %s\n", devpath );

            fclose(mcf);
            free(devpath);
            close(mcfg->fd);
            continue;
        }

        printf( "Configured %s for %s baud\n", devpath, baud_table[bd].name );

        modem_count++;
        fclose(mcf);
        free(devpath);
    }

    closedir(dcfg);

    return modem_count;
}
