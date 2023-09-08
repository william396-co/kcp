#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <functional>
#include <thread>

#include "util.h"
#include "server.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;
constexpr auto BUFFER_SIZE = 1024;

int main()
{
    std::unique_ptr<Server> server = std::make_unique<Server>( port );

    std::thread work( &Server::run, server.get() );
    std::thread input( &Server::input, server.get() );

    input.join();
    work.join();

    return 0;
}
