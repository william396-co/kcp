#include <memory>

#include "util.h"
#include "client.h"
#include "joining_thread.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

int main()
{

    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port );

    joining_thread work( &Client::run, client.get() );

    joining_thread input( &Client::input, client.get() );

    return 0;
}
