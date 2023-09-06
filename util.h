#pragma once

#include "ikcp.h"

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
} // namespace util
