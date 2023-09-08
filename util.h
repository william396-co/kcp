#pragma once

#include "ikcp.h"
#include <cstdio>

#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
#include <windows.h>
#elif !defined( __unix )
#define __unix
#endif

#ifdef __unix
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <chrono>
#include <thread>

namespace util {
using namespace std::chrono;
using namespace std::chrono_literals;

// time second
inline time_t now()
{
    return duration_cast<seconds>( system_clock::now().time_since_epoch() ).count();
}

// millisecond
inline time_t now_ms()
{
    return duration_cast<milliseconds>( system_clock::now().time_since_epoch() ).count();
}

// microseconds
inline time_t now_us()
{
    return duration_cast<microseconds>( system_clock::now().time_since_epoch() ).count();
}

// nanoseconds
inline time_t now_ns()
{
    return duration_cast<nanoseconds>( system_clock::now().time_since_epoch() ).count();
}

inline int iclock()
{
    return (IUINT32)( now_ms() & 0xfffffffful );
}

/* sleep in millisecond */
inline void isleep( unsigned long millisecond )
{
    std::this_thread::sleep_for( std::chrono::milliseconds( millisecond ) );
}

inline bool get_local_addr( int fd, struct sockaddr_in * addr )
{
    socklen_t addr_len;
    if ( getsockname( fd, (struct sockaddr *)addr, &addr_len ) == -1 ) {
        perror( "getsockname failed" );
        return false;
    }

    printf( "fd: %d local addr [%s:%d]\n", fd, inet_ntoa( addr->sin_addr ), ntohs( addr->sin_port ) );

    return true;
}

inline bool get_remote_addr( int fd, struct sockaddr_in * addr )
{
    socklen_t len;
    if ( getpeername( fd, (struct sockaddr *)addr, &len ) == -1 ) {
        perror( "getpeername failed" );
        return false;
    }
    printf( "fd:%d remote addr [%s:%d]\n", fd, inet_ntoa( addr->sin_addr ), ntohs( addr->sin_port ) );
    return true;
}
} // namespace util