#include "server.h"

int accept_modem( struct modem * mhead, struct session *** ctail )
{
    // Check each modem
    for ( struct modem * cmodm = mhead; cmodm != NULL; cmodm = cmodm->next )
    {
        
    }
}

int accept_network( int serv_sock, struct session *** ctail )
{
    // Loop until there are no new sessions
    while (1)
    {
        struct session * new_sess = malloc( sizeof(struct session) );

        int addr_len = sizeof(new_sess->in_addr);
        int cli_sock = accept( serv_sock, (struct sockaddr *) &(new_sess->in_addr), &addr_len );

        // No more new sessions
        if ( cli_sock < 0 )
        {
            free( new_sess );
            break;
        }

        // TODO check IP against ban logs

        // Try and create the client
        if ( !spawn_client( cli_sock, new_sess ) )
        {
            close( cli_sock );
            free( new_sess );
            continue;
        }

        // No longer need our copy of the client socket
        close( cli_sock );
    }
}

int spawn_client( int cli_fd, struct session * cli_sess )
{
    // Generate pipes (myPipe[0] is read, myPipe[1] is write)
    int p_send[2];
    int p_recv[2];
    int p_error[2];

    pipe( p_send );
    pipe( p_recv );
    pipe( p_error );

    // *** Spawn child
    pid_t cli_pid = fork();

    if ( !cli_pid ) // *** Child
    {
        // Setup IPC
        dup2( p_send[0], STDIN_FILENO ); // Overwrite stdin
        dup2( p_recv[1], STDOUT_FILENO ); // Overwrite stdout
        dup2( p_error[1], STDERR_FILENO ); // Overwrite stderr

        // Close all of the pipes
        close( p_send[0] ); close( p_send[1] );
        close( p_recv[0] ); close( p_recv[1] );
        close( p_error[0] ); close( p_error[1] );

        // Convert client fd to an argument
        char fdarg[10]; // Enough chars to hold INT_MAX
        sprintf( fdarg, "%d", cli_fd );

        // Generate arguments
        char * args[3];
        args[0] = "client";
        args[1] = fdarg;
        args[2] = NULL;

        // Load client image (final thing executed by this image)
        execv( *args, args );

        // *** This can only execute if execv errors ***
        perror("Failed to execv");
        exit(1);
    }
    else if ( cli_pid < 0 ) // *** Fork error
    {
        // Close all of the pipes
        close( p_send[0] ); close( p_send[1] );
        close( p_recv[0] ); close( p_recv[1] );
        close( p_error[0] ); close( p_error[1] );

        perror( "Failed child create fork" );
        return 0;
    }

    // *** Parent

    // Close child's end of the pipes
    close( p_send[0] );
    close( p_recv[1] );
    close( p_error[1] );

    // Record pid
    cli_sess->pid = cli_pid;

    // Record pipes
    cli_sess->pipe_send = p_send[1];
    cli_sess->pipe_recv = p_recv[0];
    cli_sess->pipe_error = p_error[0];

    return 1;
}
