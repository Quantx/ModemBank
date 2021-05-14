#include "central.h"

int listen_server( char * ip, char * port )
{
    int sock = socket( AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP );
    if ( sock < 0 ) { perror("sctp socket"); return -1; }

    // Let client decide number of streams
    struct sctp_initmsg init = { 0xFFFF, 0xFFFF };
    setsockopt( sock, SOL_SCTP, SCTP_INITMSG, &init, sizeof(struct sctp_initmsg) );

    struct addrinfo * addr, hint = {0};
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_SEQPACKET;
    hint.ai_protocol = IPPROTO_SCTP;
    getaddrinfo( ip, port, &hint, &addr );

    if ( bind( sock, addr->ai_addr, addr->ai_addrlen ) < 0 )
    {
        perror("bind");
        freeaddrinfo(addr);
        return -1;
    }

    freeaddrinfo(addr);

    if ( listen( sock, LISTEN_BACKLOG ) < 0 )
    {
        perror("listen");
        return -1;
    }

    return sock;
}
