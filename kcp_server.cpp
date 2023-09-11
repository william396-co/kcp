#include "util.h"
#include "server.h"
#include "joining_thread.h"

#include "ikcp.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;
constexpr auto BUFFER_SIZE = 1024;

constexpr auto conv = 0x12345;
constexpr auto suser = 0x100;

int main( int argc, char ** argv )
{
    int mode = 0;
    if ( argc >= 2 ) {
        mode = atoi( argv[1] );
    }
    std::unique_ptr<Server> server = std::make_unique<Server>( port, conv );

    util::ikcp_set_mode( server->getKcp(), mode );
    //    util::ikcp_set_log(IKCP_LOG_INPUT|IKCP_LOG_OUTPUT);

    joining_thread work( &Server::run, server.get() );
    joining_thread input( &Server::input, server.get() );

    return 0;
}
