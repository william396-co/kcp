#pragma once
#include <cstdint>
#include <functional>

struct IKCPCB;
using OutputFn = int ( * )( const char * buf, int len, IKCPCB * kcp, void * user );

class KcpEx
{
public:
    KcpEx( uint32_t conv, void * user );
    ~KcpEx();

    void set_output( OutputFn fn );

    int recv( char * buffer, int len );
    int send( const char * buffer, int len );

    void update( uint32_t current );
    uint32_t check( uint32_t current );

    int input( const char * data, long size );

    int peeksize();
    int setmtu( int mtu );
    int set_wndsize( int sndwnd, int rcvwnd );
    int waitsnd();
    int set_nodelay( bool nodelay_, int interval, int resend, int nc );

    void set_rx_minrto( int rx_minrto_ );
    void set_fastresend( int fastresend_ );

private:
    void flush();

private:
    IKCPCB * self;
};
