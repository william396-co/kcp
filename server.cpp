#include "server.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

Server::Server( uint16_t port )
    : listen { nullptr }
{
    listen = std::make_unique<UdpSocket>();
    listen->setNonblocking();
    if ( !listen->bind( port ) ) {
        throw std::runtime_error( "listen socket bind error" );
    }
}

Server::~Server()
{
    /*  for ( auto & it : connections ) {
          delete it.second;
      }*/
}

/*
UdpSocket * Server::findConn( const char * ip, uint16_t port )
{
    auto it = connections.find( std::make_pair( ip, port ) );
    if ( it != connections.end() ) {
        return it->second;
    }
    std::cout << __FUNCTION__ << "(" << ip << "," << port << ")\n";
    UdpSocket * conn = new UdpSocket();
    if ( conn->connect( ip, port ) ) {
        connections.emplace( std::make_pair( ip, port ), conn );
        return conn;
    }
    delete conn;
    return nullptr;
}*/

void Server::doRecv()
{
    printf( "Received a string from client [%s:%d] -> [ %s:%d], string is: %s\n",
        listen->getRemoteIp(),
        listen->getRemotePort(),
        listen->getLocalIp(),
        listen->getLocalPort(),
        listen->getRecvBuffer() );

    /* 将收到的字符串消息返回给client端 */
    listen->send( listen->getRecvBuffer(), listen->getRecvSize(), listen->getRemoteIp(), listen->getRemotePort() );
}

void Server::input()
{
    return;

    std::string writeBuffer;
    char ip[512];
    int port;
    while ( true ) {
        memset( ip, 0, sizeof( ip ) );
        port = 0;
        writeBuffer.clear();

        printf( "Enter Send Ip:" );
        scanf( "%s\n", ip );
        printf( "Enter Send Port:" );
        scanf( "%d\n", &port );
        printf( "Enter a string send to client:" );
        std::getline( std::cin, writeBuffer );
        if ( ip[0] != '\0' && port > 0 && !writeBuffer.empty() ) {
            listen->send( writeBuffer.data(), writeBuffer.size(), ip, port );
        }
    }
}

void Server::run()
{
    while ( true ) {
        util::isleep( 10 );

        if ( listen->recv() > 0 ) {
            doRecv();
        }
    }
}

