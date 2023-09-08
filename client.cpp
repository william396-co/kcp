#include "client.h"

#include <stdexcept>

Client::Client( const char * ip, uint16_t port )
    : client { nullptr }
{
    client = std::make_unique<UdpSocket>();
    client->setNonblocking();
    if ( !client->connect( ip, port ) ) {
        throw std::runtime_error( "client connect failed" );
    }
}

void Client::input()
{
    std::string writeBuffer;
    while ( true ) {
        printf( "Please enter a string to send to server(%s:%d):", client->getRemoteIp(), client->getRemotePort() );

        writeBuffer.clear();
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            client->send( writeBuffer.data(), writeBuffer.size() );
        }
    }
}

void Client::run()
{
    while ( true ) {
        util::isleep( 1 );
        if ( client->recv() > 0 ) {
            printf( "Receive from server[%s:%d]: %s\n", client->getRemoteIp(), client->getRemotePort(), client->getRecvBuffer() );
        }
    }
}
