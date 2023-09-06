#pragma once

#include "util.h"
constexpr auto RECV_BUF_SIZE = 1024 * 4;

class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    // server function
    bool bind( uint16_t port );
    // client function
    bool connect( const char * ip, uint16_t port );

    void close();
    int32_t send( const char * bytes, int32_t size );
    int32_t recv();
    const char * getRecvBuffer() const { return m_recvBuffer; }
    uint32_t getRecvSize() const { return m_recvSize; }
    int setNonblocking( bool isNonblocking = true );

private:
    int m_fd;
    struct sockaddr_in m_addr;
    socklen_t addr_len;
    char m_recvBuffer[RECV_BUF_SIZE];
    uint32_t m_recvSize;
};
