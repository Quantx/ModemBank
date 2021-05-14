#include "central.h"

int main( int argc, char ** argv )
{
    listen_server( argc > 1 ? argv[1] : DEFAULT_IP,
                   argc > 2 ? argv[2] : MODEMBANK_PORT_STR );

    return 0;
}
