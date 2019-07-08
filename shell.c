#include "modembank.h"

void terminalRaw( user * muser )
{
    conn * mconn = muser->stdin;

    if ( mconn->buflen <= 0 || muser->cmdwnt >= 0 ) return;

    // Convert request to positive length
    int want = abs( muser->cmdwnt );

    // Empty the buffer
    mconn->buflen = 0;
}

void terminalLine( user * muser )
{
    // Get our input buffer
    conn * mconn = muser->stdin;

    // Print the prompt
    if ( muser->cmdppt[0] != '\0' )
    {
        uprintf( muser, muser->cmdppt );
        muser->cmdppt[0] = '\0';
    }

    // Nothing to do here
    if ( mconn->buflen <= 0 || muser->cmdwnt <= 0 ) return;

    char acms[9]; // For sending ANSI CSI strings
    int i;

    // Fix a weird bug
    if ( mconn->buf[0] == '\x0D' && mconn->buf[1] == '\0' ) mconn->buflen = 1;

    if ( mconn->buflen == 1 ) // Standard ascii
    {
        // Check if this is printable ascii
        if ( mconn->buf[0] >= 32 && mconn->buf[0] < 127 )
        {
            // Check if we have room in the command string
            if ( muser->cmdlen < muser->cmdwnt && muser->cmdlen < COMMAND_LEN )
            {
                // Make room for character
                for ( i = muser->cmdlen; i > muser->cmdpos; i-- )
                {
                    muser->cmdbuf[i] = muser->cmdbuf[i - 1];
                }

                // Insert character
                muser->cmdbuf[i] = mconn->buf[0];
                // Increase string length
                muser->cmdlen++;

                if ( !muser->cmdsec )
                {
                    // Reprint string
                    write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                    // Move cursor back to correct spot
                    for ( i = muser->cmdlen; i > muser->cmdpos + 1; i-- ) write( mconn->fd, "\b", 1 );
                }
                // Advance cursor
                muser->cmdpos++;
            }
            else
            {
                // No room, ring their bell to let them know
                write( mconn->fd, "\a", 1 );
            }
        }
        else if ( mconn->buf[0] == '\x0D' ) // Return character sent
        {
            // Add null byte
            muser->cmdbuf[muser->cmdlen] = '\0';

            // Reset internal vars
            muser->cmdlen = 0;
            muser->cmdpos = 0;

            // Indicate ready
            muser->cmdwnt = 0;

            // Send newline
            write( mconn->fd, "\r\n", 2 );

            // Log command
            if ( muser->cmdsec )
            {
                xlog( muser, "New secure command\n" );
            }
            else
            {
                xlog( muser, "New command: '%s'\n", muser->cmdbuf );
            }
        }
        else if ( mconn->buf[0] == '\b' || mconn->buf[0] == '\x7F' ) // Backspace character sent
        {
            if ( muser->cmdpos > 0 ) // Anything to delete?
            {
                // Shift all characters left by one
                for ( i = muser->cmdpos; i < muser->cmdlen; i++ )
                {
                    muser->cmdbuf[i - 1] = muser->cmdbuf[i];
                }

                // Decrease length of string
                muser->cmdlen--;

                // Move cursor
                muser->cmdpos--;

                if ( !muser->cmdsec )
                {
                    write( mconn->fd, "\b", 1 );
                    // Reprint string
                    write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                    // Add a space to cover up deleted char
                    write( mconn->fd, " \b", 2 );
                    // Move cursor back to correct spot
                    for ( i = muser->cmdlen; i > muser->cmdpos; i-- ) write( mconn->fd, "\b", 1 );
                }
            }
        }
        else
        {
            xlog( muser, "Got unknown ascii: %02x\n", mconn->buf[0] & 0xFF );
        }
    }
    else if ( mconn->buf[0] == '\e' && mconn->buf[1] == '[' )// Extended control string (Arrow key, Home, etc.)
    {
        switch ( mconn->buf[2] ) // Check for arrow keys
        {
            case 'A': // Up arrow
                break;
            case 'B': // Down arrow
                break;
            case 'C': // Right arrow
                if ( muser->cmdpos < muser->cmdlen )
                {
                    // Move cursor to the right
                    muser->cmdpos++;
                    if ( !muser->cmdsec ) write( mconn->fd, "\e[C", 3 );
                }
                break;
            case 'D': // Left arrow
                if ( muser->cmdpos > 0 )
                {
                    // Move cursor to the left
                    muser->cmdpos--;
                    if ( !muser->cmdsec ) write( mconn->fd, "\b", 1 );
                }
                break;
            case 'H': // Home
                if ( muser->cmdpos > 0 )
                {
                    muser->cmdpos = 0;
                    write( mconn->fd, "\r", 1 );
                }
                break;
            case 'F': // End
                if ( muser->cmdpos > 0 )
                {
                    if ( !muser->cmdsec )
                    {
                        // Compute ANSI CSI string
                        sprintf( acms, "\e[%dC", muser->cmdlen - muser->cmdpos );
                        write( mconn->fd, acms, strlen( acms ) );
                    }
                    // Update cursor position
                    muser->cmdpos = muser->cmdlen;
                }
                break;
            case '3': // Delete
                if ( muser->cmdpos < muser->cmdlen )
                {
                    // Shift all characters left by one
                    for ( i = muser->cmdpos; i < muser->cmdlen - 1; i++ )
                    {
                        muser->cmdbuf[i] = muser->cmdbuf[i + 1];
                    }

                    // Decrease length of string
                    muser->cmdlen--;

                    if ( !muser->cmdsec )
                    {
                        // Reprint string
                        write( mconn->fd, muser->cmdbuf + muser->cmdpos, muser->cmdlen - muser->cmdpos );
                        // Add a space to cover up deleted char
                        write( mconn->fd, " \b", 2 );
                        // Move cursor back to correct spot
                        for ( i = muser->cmdlen; i > muser->cmdpos; i-- ) write( mconn->fd, "\b", 1 );
                    }
                }
                break;
        }
    }
    else
    {
        zlog( "Random string: " );
        for ( i = 0; i < mconn->buflen; i++ )printf( "%02x|", mconn->buf[i] & 0xFF );
        printf( "\b \b\n" );
    }

    // Empty the buffer
    mconn->buflen = 0;
}

void terminalShell( user * muser )
{
    // Prep for next command
    muser->cmdwnt = 100;
    strcpy( muser->cmdppt, "@" );

    // No command given, reset
    if ( muser->cmdbuf[0] == '\0' ) return;

    int i, k;
    char hunt = '\0'; // Character to hunt for
    /* Modes:
       '\0' - Hunt for word character
       ' '  - Hunt for white space
       '\'' - Hunt for single quote (')
       '"'  - Hunt for double quote (")
       NOTE: It is illegal to end tokenization in modes 2, 3, or 4
    */

    int argc = 0;
    char * argv[256];

    // Tokenize this bitch
    for ( i = 0; muser->cmdbuf[i] != '\0'; i++ )
    {
        // Handle escapes first
        if ( muser->cmdbuf[i] == '\\' )
        {
            // Remove \ from command string
            for ( k = i; muser->cmdbuf[k] != '\0'; k++ )
            {
                muser->cmdbuf[k] = muser->cmdbuf[k + 1];
            }
        }
        else if ( hunt == '\0' )
        {
            if ( muser->cmdbuf[i] == '\''
            || muser->cmdbuf[i] == '"' )
            {
                // Skip the first quote and store an arg pointer
                argv[argc++] = muser->cmdbuf + i + 1;
                // Store the exit quote
                hunt = muser->cmdbuf[i];
            }
            else if ( muser->cmdbuf[i] != ' ' )
            {
                // Store an arg pointer
                argv[argc++] = muser->cmdbuf + i;
                // Hunt for the next space
                hunt = ' ';
            }
        }
        else if ( muser->cmdbuf[i] == hunt )
        {
            // Terminate argument and reset hunt
            muser->cmdbuf[i] = hunt = '\0';
        }
    }

    if ( hunt == '\'' )
    {
        uprintf( muser, "%%missing closing single-quote\r\n" );
        return;
    }
    else if ( hunt == '"' )
    {
        uprintf( muser, "%%missing closing double-quote\r\n" );
        return;
    }

    // Arg after last must be null
    argv[argc] = NULL;

    if ( argc <= 0 ) return;

    // Find command
    void (* const command) ( user * muser, int argc, char * argv[]) = findCommand( argv[0] );

    if ( command == NULL )
    {
        uprintf( muser, "%%unknown command: %s\r\n", argv[0] );
        return;
    }

    // Run command
    command( muser, argc, argv );
}

void terminalBridge( user * muser )
{
    if ( muser->stdin == NULL || muser->stdin->flags & FLAG_GARB
    ||  muser->stdout == NULL || muser->stdout->flags & FLAG_GARB ) return;

    // Copy in to out
    if ( muser->stdin->buflen > 0 )
    {
        write( muser->stdout->fd, muser->stdin->buf, muser->stdin->buflen );
        muser->stdin->buflen = 0;
    }

    // Copy out to in
    if ( muser->stdout->buflen > 0 )
    {
        write( muser->stdin->fd, muser->stdout->buf, muser->stdout->buflen );
        muser->stdout->buflen = 0;
    }
}
