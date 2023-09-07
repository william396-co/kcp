#pragma once
#include "util.h"
#include "udpsocket.h"

#include <unordered_map>
#include <functional>
#include <string>

using ConnID = std::pair<std::string, uint16_t>;

namespace std {
template<>
struct hash<ConnID>
{
    size_t operator()( ConnID const & cid ) const
    {
        size_t val = 17;
        val = 31 * val + std::hash<std::string> {}( cid.first );
        val = 31 * val + std::hash<uint16_t> {}( cid.second );
        return val;
    }
};
} // namespace std

using ConnMap = std::unordered_map<ConnID, UdpSocket *>;

class Server
{
public:
    Server( uint16_t port );
    ~Server();

    UdpSocket * findConn( const char * ip, uint16_t port );

    void run();

private:
    void DoRecv();

private:
    UdpSocket * listen;
    ConnMap connections;
};

