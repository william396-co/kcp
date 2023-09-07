#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <functional>

#include "util.h"
#include "server.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;
constexpr auto BUFFER_SIZE = 1024;

int main()
{

    std::unique_ptr<Server> server = std::make_unique<Server>( port );
    server->run();

    return 0;
}
