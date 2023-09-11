#include <memory>

#include "util.h"
#include "client.h"
#include "joining_thread.h"

constexpr auto default_ip = "127.0.0.1";
constexpr auto default_port = 9527;

constexpr auto conv = 0x12345;

int main( int argc, char ** argv )
{
    int mode = 0;
    std::string ip = default_ip;
    uint16_t port = default_port;

    if ( argc >= 2 ) {
        ip = argv[1];
    }

    if ( argc >= 3 ) {
        port = atoi( argv[2] );
    }

    if ( argc >= 4 ) {
        mode = atoi( argv[3] );
    }
    std::unique_ptr<Client> client = std::make_unique<Client>( ip.c_str(), port, conv );

    util::ikcp_set_mode( client->getKcp(), mode );

    joining_thread work( &Client::run, client.get() );
    joining_thread input( &Client::input, client.get() );

    return 0;
}
