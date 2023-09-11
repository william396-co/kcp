#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

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

    kcp = ikcp_create( conv, client.get() );
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
    while ( is_running ) {
        printf( "Please enter a string to send to server(%s:%d):", client->getRemoteIp(), client->getRemotePort() );

        writeBuffer.clear();
        bzero( buff, sizeof( buff ) );
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            ( (IUINT32 *)buff )[0] = g_sn++;
            ( (IUINT32 *)buff )[1] = util::iclock();

            memcpy( &buff[8], writeBuffer.data(), writeBuffer.size() );
            ikcp_send( kcp, buff, writeBuffer.size() + 8 );
            ikcp_update( kcp, util::iclock() );
            // client->send( writeBuffer.data(), writeBuffer.size() );
        }
    }
}

void Client::run()
{
    char buff[BUFFER_SIZE];
    uint32_t next = 0;
    uint32_t sumrtt = 0;
    uint32_t count = 0;
    uint32_t maxrtt = 0;

    while ( is_running ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // udp pack received
        if ( client->recv() < 0 ) {
            continue;
        }
        ikcp_input( kcp, client->getRecvBuffer(), client->getRecvSize() );

        bzero( buff, sizeof( buff ) );
        // user/logic package received
        int rc = ikcp_recv( kcp, buff, BUFFER_SIZE );
        if ( rc < 0 )
            continue;

        IUINT32 sn = *(IUINT32 *)( buff + 0 );
        IUINT32 ts = *(IUINT32 *)( buff + 4 );
        IUINT32 rtt = util::iclock() - ts;

        if ( sn != next ) {
            printf( "ERROR sn %d<->%d\n", count, next );
            break;
        }
        ++next;
        sumrtt += rtt;
        ++count;
        maxrtt = rtt > maxrtt ? rtt : maxrtt;

        printf( "[RECV] sn:%d rrt:%d  content: {%s}\n", sn, rtt, &buff[8] );

        if ( next > 10 )
            break;

        /*        if ( client->recv() > 0 ) {
                    printf( "Receive from server[%s:%d]: %s\n", client->getRemoteIp(), client->getRemotePort(), client->getRecvBuffer() );
                }*/
    }

    /* summary */
    printf( "avgrtt=%d maxrtt=%d \n", int( sumrtt / count ), maxrtt );
}
