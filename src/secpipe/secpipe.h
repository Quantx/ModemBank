#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

/* secpipe
    Description:
    Securly transfer data from one file descriptor to another

    Arguments:
    fd_from = A valid file descriptor to read data from
    fd_to = A valid file descriptor to transfer the data to
    name = (Optional) A name for the spawned secpipe which will be
        concatenated with the string "secpipe-", NULL if not desired

    Returns: ID of created process or -1 on error (check errno)
*/

pid_t secpipe( int fd_from, int fd_to, char * name );
