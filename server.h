#pragma once
#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

#include <unordered_map>
#include <functional>
#include <string>
#include <memory>

/*
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
*/

extern bool is_running;

class Server
{
public:
    Server( uint16_t port, uint32_t conv );
    ~Server();

    //   UdpSocket * findConn( const char * ip, uint16_t port );

    void run();
    void input();

    ikcpcb * getKcp() { return kcp; }

private:
    void doRecv();

private:
    std::unique_ptr<UdpSocket> listen;
    //    ConnMap connections;
    ikcpcb * kcp;
};

