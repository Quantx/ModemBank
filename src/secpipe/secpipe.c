#include "secpipe.h"

pid_t secpipe( int fd_rx, int fd_tx, char * name )
{
     // Enough room to store a 32bit value in hexadecimal
     char rx_str[9], tx_str[9];

     // Convert file descriptors to hexadecimal
     sprintf( rx_str, "%X", fd_rx );
     sprintf( tx_str, "%X", fd_tx );

     char * name_str;

     if ( name )
     {
         name_str = malloc( strlen(name) + 9 );
         strcpy( name_str, "secpipe-" );
         strcat( name_str, name );
     }

     pid_t spp = fork();

     // Parent process or error
     if ( spp ) return spp;

     char * argv[4];
     argv[0] = name ? name_str : "secpipe";
     argv[1] = rx_str;
     argv[2] = tx_str;
     argv[3] = NULL;

     execv( "secpipe", argv );

     // Failed to execv, abort!
     perror("execv secpipe failed");
     exit(1);
}
