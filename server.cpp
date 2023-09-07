#include "server.h"

#include <iostream>
#include <stdexcept>

Server::Server( uint16_t port )
    : listen { nullptr }
{
    listen = new UdpSocket();
    listen->setNonblocking();
    if ( !listen->server_bind( port ) ) {
        throw std::runtime_error( "listen socket bind error" );
    }
}

Server::~Server()
{
    delete listen;
    for ( auto & it : connections ) {
        delete it.second;
    }
}

UdpSocket * Server::findConn( const char * ip, uint16_t port )
{
    auto it = connections.find( std::make_pair( ip, port ) );
    if ( it != connections.end() ) {
        return it->second;
    }
    std::cout << __FUNCTION__ << "(" << ip << "," << port << ")\n";
    UdpSocket * conn = new UdpSocket();
    if ( conn->connect( ip, port ) ) {
        //        conn->client_bind();
        connections.emplace( std::make_pair( ip, port ), conn );
        return conn;
    }
    delete conn;
    return nullptr;
}

void Server::DoRecv()
{
    printf( "Received a string from client [%s:%d] -> [ %s:%d], string is: %s\n",
        listen->getRemoteIp(),
        listen->getRemotePort(),
        listen->getLocalIp(),
        listen->getLocalPort(),
        listen->getRecvBuffer() );

    UdpSocket * conn = findConn( listen->getRemoteIp(), listen->getRemotePort() );
    if ( conn ) {
        /* 将收到的字符串消息返回给client端 */
        conn->send( listen->getRecvBuffer(), listen->getRecvSize() );
    }
}

void Server::run()
{
    while ( true ) {
        util::isleep( 10 );

        if ( listen->recv() > 0 ) {
            DoRecv();
        }
    }
}

