#include "udpsocket.h"

UdpSocket::UdpSocket()
    : m_fd { 0 }
{
}

UdpSocket::~UpdSocket()
{
    if ( m_fd )
        closeSocket( m_fd );
}

bool UdpSocket::bind( uint16_t port )
{
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons( port );
    m_addr.sin_addr.s_addr = htonl( INADDR_ANY ); // 接收任意IP发来的数据

    return bind( m_fd, (struct sockaddr *)&m_addr, sizeof( m_addr ) );
}

bool UdpSocket::connect( const char * ip, uint16_t port )
{
    m_fd = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    struct sockaddr_in sAddr;
    m_addr.sin_faimly = AF_INET;
    m_addr.sin_port = htons( port );
    m_addr.sin_addr.s_addr = inet_addr( ip );
    m_addr_len = sizeo( sAddr );
    return connect( m_fd, (struct sockaddr *)&m_addr, m_addr_len );
}

void UdpSocket::close()
{
    closesocket( m_fd );
}

int32_t UdpSocket::send( const char * bytes, uint32_t size )
{
    return sendto( m_fd, bytes, size, 0, (struct sockaddr *)&m_addr, sizeof( m_addr ) );
}

int32_t UdpSocket::recv()
{
    m_recvSize = ::recvfrom( m_fd, m_recvBuffer, sizeof( m_recvBuffer ), 0, (struct sockaddr *)&m_addr, &m_addr_len );
    return m_recvSize;
}

int UdpSocket::setNonblocking( bool isNonblocking )
{
    unsigned long imode = isNonblocking ? 1 : 0;
#ifdef _unix
    return isNonblocking ? fcntl( m_fd, F_SETFL, fcntl( m_fd, F_GETFL ) | O_NONBLOCK ) : fcntl( m_fd, F_SETFL, fcntl( m_fd, F_GETFL ) & ~O_NONBLOCK );
#else
    return ioctlsocket( m_fd, FIONBIO, &imode );
#endif
}
