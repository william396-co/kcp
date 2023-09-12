#include "util.h"

#include <signal.h>

namespace util {

void ikcp_set_mode( ikcpcb * kcp, int mode )
{
    assert( mode >= 0 && mode < 3 );
    const char * mode_name[] = { "DEFAULT", "NORMAL", "FAST" };

    ikcp_wndsize( kcp, 128, 128 );
    switch ( mode ) {
        case 1: // normal mode
        {
            ikcp_nodelay( kcp, 0, 10, 0, 1 );
            break;
        }
        case 2: // fast start mode
        {
            ikcp_nodelay( kcp, 2, 10, 2, 1 );
            kcp->rx_minrto = 10;
            kcp->fastresend = 1;
            break;
        }

        default: { // default mode
            ikcp_nodelay( kcp, 0, 10, 0, 0 );
            break;
        }
    }
    printf( "KCP run on [%s] mode\n", mode_name[mode] );
}

void kcp_log( const char * log, ikcpcb * kcp, void * user )
{
    std::cout << "[log]" << log << "\n";
}

void ikcp_set_log( ikcpcb * kcp, int mask )
{
    kcp->logmask |= mask;
    kcp->writelog = kcp_log;
}

void signal_handler( int sig )
{
    is_running = false;
}
void handle_signal()
{
    signal( SIGPIPE, SIG_IGN );
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

void rand_str( std::string & str )
{
    size_t sz = rand() % 2000;
    for ( size_t i = 0; i != sz; ++i ) {
        str.push_back( rand() % 94 + 33 );
    }
}

} // namespace util
