#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>

#define COPY_BUFFER_SIZE 65536 // 64KB

unsigned char copy_buf[COPY_BUFFER_SIZE];

int fd_rx, fd_tx;

int main( int argc, char ** argv )
{
    if ( argc < 3 ) return 1;

    // Get the file descriptors we're piping
    fd_rx = strtol( argv[1], NULL, 16 );
    fd_tx = strtol( argv[2], NULL, 16 );

    // Make sure these are valid file descriptors
    if ( !fd_rx ) return 2;
    if ( !fd_tx ) return 3;

    // Close all other open file descriptors
    {
        struct rlimit rlim_fd;
        int i;

        getrlimit( RLIMIT_NOFILE, &rlim_fd );
        for ( i = 0; i < rlim_fd.rlim_max; i++ )
        {
            if ( i != fd_rx && i != fd_tx ) close(i);
        }

        i = 0;
        rlim_fd = (struct rlimit){0};
    }

    // Make these files blocking
    {
        int flags;

        flags = fcntl( fd_rx, F_GETFL );
        if ( fcntl( fd_rx, F_SETFL, flags & ~O_NONBLOCK ) < 0 ) return 4;

        flags = fcntl( fd_tx, F_GETFL );
        if ( fcntl( fd_tx, F_SETFL, flags & ~O_NONBLOCK ) < 0 ) return 5;

        flags = 0;
    }

    argc = 0;
    argv = NULL;

    /* Initiate Secure Computing Mode
        This disables all system calls except:
            read, write, exit, sigreturn
        Any other system call will result in this process being SIGKILL'd
    */
    if ( prctl( PR_SET_SECCOMP, SECCOMP_MODE_STRICT ) < 0 ) return 6;

    // Main loop
    while (1)
    {
        int len;

        // Read in data
        len = read( fd_rx, copy_buf, COPY_BUFFER_SIZE );

        // Normal exit
        if ( !len ) return 0;
        // Read error
        else if ( len < 0 ) return 7;

        // Write out data
        len = write( fd_tx, copy_buf, len );

        // Normal exit
        if ( len < 0 ) return 0;
    }
}
