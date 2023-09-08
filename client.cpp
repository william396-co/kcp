#include "client.h"

#include <stdexcept>
#include <cstring>

constexpr auto BUFFER_SIZE = 1024 * 4;
static int g_sn = 0;

int32_t kcp_output( const char * buf, int len, ikcpcb * kcp, void * user )
{
    UdpSocket * s = (UdpSocket *)user;
    return s->send( buf, len );
}

Client::Client( const char * ip, uint16_t port, uint32_t conv )
    : client { nullptr }
{
    client = std::make_unique<UdpSocket>();
    client->setNonblocking();
    if ( !client->connect( ip, port ) ) {
        throw std::runtime_error( "client connect failed" );
    }

    kcp = ikcp_create( conv, (void *)&client );
    ikcp_setoutput( kcp, kcp_output );
}

Client::~Client()
{
    ikcp_release( kcp );
}

void Client::input()
{
    std::string writeBuffer;
    char buff[BUFFER_SIZE];
    while ( true ) {
        printf( "Please enter a string to send to server(%s:%d):", client->getRemoteIp(), client->getRemotePort() );

        writeBuffer.clear();
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            ( (IUINT32 *)buff )[0] = ++g_sn;
            ( (IUINT32 *)buff )[1] = util::iclock();

            memcpy( &buff[8], writeBuffer.data(), writeBuffer.size() );
            ikcp_send( kcp, buff, writeBuffer.size() + 8 );
            // client->send( writeBuffer.data(), writeBuffer.size() );
        }
    }
}

void Client::run()
{
    char buff[BUFFER_SIZE];
    while ( true ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );
        int rc = ikcp_recv( kcp, buff, 10 );
        if ( rc < 0 )
            continue;

        IUINT32 sn = *(IUINT32 *)( buff + 0 );
        IUINT32 ts = *(IUINT32 *)( buff + 4 );
        IUINT32 rtt = util::iclock() - ts;

        printf( "Receive from server[%s:%d]: sn:%d rrt:%d %s\n", client->getRemoteIp(), client->getRemotePort(), sn, rtt, &buff[8] );

        /*        if ( client->recv() > 0 ) {
                    printf( "Receive from server[%s:%d]: %s\n", client->getRemoteIp(), client->getRemotePort(), client->getRecvBuffer() );
                }*/
    }
}
