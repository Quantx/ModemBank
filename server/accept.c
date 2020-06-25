#include "server.h"

int accept_modem( struct modem * mhead, struct session *** ctail )
{
    int sess_count = 0;

    // Check each modem
    for ( struct modem * cmodm = mhead; cmodm != NULL; cmodm = cmodm->next )
    {
        int curDCD = getDCD( cmodm->fd );
        int curDTR = getDTR( cmodm->fd );

        // Incoming call
        if ( cmodm->state == modem_state_idle && curDCD )
        {
            // Create and empty struct
            struct session * new_sess = malloc( sizeof(struct session) );
            memset( new_sess, 0, sizeof(struct session) );

            // Assign modem to session
            new_sess->modm_in = cmodm;

            // TODO: Read Caller ID

            // Spawn new client
            if ( !spawn_client( cmodm->fd, new_sess ) )
            {
                // Tell modem to hangup
                setDTR( cmodm->fd, 0 );
                cmodm->state = modem_state_hangup;

                free( new_sess );
                continue;
            }

            cmodm->state = modem_state_dial_in;

            // Add session to list
            **ctail = new_sess;

            *ctail = &(**ctail)->next;
            sess_count++;
        }
        // Waiting for modem to hangup
        else if ( cmodm->state == modem_state_hangup )
        {
            // Modem actually hung up
            if ( !curDCD )
            {
                // Switch back to idle
                setDTR( cmodm->fd, 1 );
                cmodm->state = modem_state_idle;
            }
            // Forgot to unassert DTR
            else if ( curDTR )
            {
                setDTR( cmodm->fd, 0 );
            }
        }
    }

    return sess_count;
}

int accept_network( int serv_sock, struct session *** ctail )
{
    int sess_count = 0;

    // Loop until there are no new sessions
    while (1)
    {
        struct session * new_sess = malloc( sizeof(struct session) );
        memset( new_sess, 0, sizeof(struct session) );

        // Accept new network connection, and record IP address
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

        // Add session to list
        **ctail = new_sess;

        *ctail = &(**ctail)->next;
        sess_count++;
    }

    return sess_count;
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

        perror( "Failed child creation fork" );
        return 0;
    }

    // *** Parent

    // Close child's end of the pipes
    close( p_send[0] );
    close( p_recv[1] );
    close( p_error[1] );

    // Record pid
    cli_sess->pid = cli_pid;

    // Record time of birth
    time( &(cli_sess->born) );

    // Record pipes
    cli_sess->pipe_send = p_send[1];
    cli_sess->pipe_recv = p_recv[0];
    cli_sess->pipe_error = p_error[0];

    return 1;
}
