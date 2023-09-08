#include <memory>
#include <thread>

#include "util.h"
#include "client.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

int main()
{

    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port );

    std::thread work( &Client::run, client.get() );

    std::thread input( &Client::input, client.get() );

    input.join();
    work.join();

    return 0;
}
