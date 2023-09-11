#include "server.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

constexpr auto BUFFER_SIZE = 1024 * 8;

int32_t kcp_output( const char * buf, int len, ikcpcb * kcp, void * user )
{
    UdpSocket * s = (UdpSocket *)user;
    return s->send( buf, len );
}

Server::Server( uint16_t port, uint32_t conv )
    : listen { nullptr }
{
    listen = std::make_unique<UdpSocket>();
    listen->setNonblocking();
    if ( !listen->bind( port ) ) {
        throw std::runtime_error( "listen socket bind error" );
    }

    kcp = ikcp_create( conv, listen.get() );
    ikcp_setoutput( kcp, kcp_output );

    ikcp_wndsize( kcp, 128, 128 );
    ikcp_nodelay( kcp, 0, 10, 0, 0 );
}

Server::~Server()
{
    /*  for ( auto & it : connections ) {
          delete it.second;
      }*/

    ikcp_release( kcp );
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
    return; // TODO

    std::string writeBuffer;
    char ip[512];
    int port;
    while ( is_running ) {
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
    char buff[BUFFER_SIZE];
    while ( is_running ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // lower level recv
        if ( listen->recv() <= 0 ) {
            continue;
        }
        ikcp_input( kcp, listen->getRecvBuffer(), listen->getRecvSize() );

        memset( &buff, 0, sizeof( buff ) );
        // user level recv
        int rc = ikcp_recv( kcp, buff, BUFFER_SIZE );
        if ( rc < 0 ) continue;

        IUINT32 sn = *(IUINT32 *)( buff );
        //  IUINT32 ts = *(IUINT32 *)( buff + 4 );
        printf( "RECV [%s:%d] -> [ %s:%d], sn:[%d] string is:{ %s}\n",
            listen->getRemoteIp(),
            listen->getRemotePort(),
            listen->getLocalIp(),
            listen->getLocalPort(),
            sn,
            &buff[8] );

        // send back to client
        ikcp_send( kcp, buff, rc );

        /*if ( listen->recv() > 0 ) {
            doRecv();}*/
    }
}

