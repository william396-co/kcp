#include "util.h"
#include "server.h"
#include "joining_thread.h"

#include "ikcp.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

bool is_running = true;

int main( int argc, char ** argv )
{

    util::handle_signal();

    int mode = 0;
    if ( argc >= 2 ) {
        mode = atoi( argv[1] );
    }
    std::unique_ptr<Server> server = std::make_unique<Server>( port, conv );
    server->setmode( mode );
    //  server->show_data( true );
    //    util::ikcp_set_log(IKCP_LOG_INPUT|IKCP_LOG_OUTPUT);

    joining_thread accept( &Server::accept, server.get() );
    joining_thread work( &Server::run, server.get() );

    return 0;
}
