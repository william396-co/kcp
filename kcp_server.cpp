#include "util.h"
#include "server.h"
#include "joining_thread.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;
constexpr auto BUFFER_SIZE = 1024;

int main()
{
    std::unique_ptr<Server> server = std::make_unique<Server>( port );

    joining_thread work( &Server::run, server.get() );
    joining_thread input( &Server::input, server.get() );

    return 0;
}
