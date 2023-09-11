#include <memory>

#include "util.h"
#include "client.h"
#include "joining_thread.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

constexpr auto conv = 0x12345;

int main( int argc, char ** argv )
{

    int mode = 0;
    if ( argc >= 2 ) {
        mode = atoi( argv[1] );
    }
    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port, conv );

    util::ikcp_set_mode( client->getKcp(), mode );

    joining_thread work( &Client::run, client.get() );
    joining_thread input( &Client::input, client.get() );

    return 0;
}
