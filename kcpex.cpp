#include "kcpex.h"
#include "ikcp.h"

KcpEx::KcpEx( uint32_t conv, void * user )
    : self { nullptr }
{
    self = ikcp_create( conv, user );
}

KcpEx::~KcpEx()
{
    ikcp_release( self );
}

void KcpEx::set_output( OutputFn fn )
{
    ikcp_setoutput( self, fn );
}

int KcpEx::recv( char * buffer, int len )
{
    return ikcp_recv( self, buffer, len );
}

int KcpEx::send( const char * buffer, int len )
{
    return ikcp_send( self, buffer, len );
}

void KcpEx::update( uint32_t current )
{
    ikcp_update( self, current );
}

uint32_t KcpEx::check( uint32_t current )
{
    return ikcp_check( self, current );
}

int KcpEx::input( const char * data, long size )
{
    return ikcp_input( self, data, size );
}

int KcpEx::peeksize()
{
    return ikcp_peeksize( self );
}

int KcpEx::setmtu( int mtu )
{
    return ikcp_setmtu( self, mtu );
}

int KcpEx::set_wndsize( int sndwnd, int rcvwnd )
{
    return ikcp_wndsize( self, sndwnd, rcvwnd );
}

int KcpEx::waitsnd()
{
    return ikcp_waitsnd( self );
}

int KcpEx::set_nodelay( bool nodelay, int interval, int resend, int nc )
{
    return ikcp_nodelay( self, nodelay ? 1 : 0, interval, resend, nc );
}

void KcpEx::flush()
{
    ikcp_flush( self );
}

void KcpEx::set_rx_minrto( int rx_minrto_ )
{
    self->rx_minrto = rx_minrto_;
}
void KcpEx::set_fastresend( int fastresend_ )
{
    self->fastresend = fastresend_;
}
