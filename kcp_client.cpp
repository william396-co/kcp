#include <iostream>
#include <string>
#include <cstring>
#include <memory>

#include "util.h"
#include "udpsocket.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

constexpr auto BUFFER_SIZE = 1024;

int main()
{
    std::unique_ptr<UdpSocket> client = std::make_unique<UdpSocket>();
    client->setNonblocking();
    if ( !client->connect( ip, port ) ) {
        std::cerr << "connect failed\n";
        return 1;
    }

    if ( !client->client_bind() ) {
        std::cerr << "client bind failed\n";
        return 1;
    }

    std::string writeBuffer;
    while ( true ) {
        while ( true ) {
            printf( "Please enter a string to send to server[%s:%d] ->[%s:%d]: \n",
                client->getLocalIp(),
                client->getLocalPort(),
                client->getRemoteIp(),
                client->getRemotePort() );
            /* 从标准输入设备取得字符串*/
            writeBuffer.clear();
            std::getline( std::cin, writeBuffer );
            if ( writeBuffer.empty() ) break;
            /* 将字符串传送给server端*/
            client->send( writeBuffer.data(), writeBuffer.size() );
        }

        /* 接收server端返回的字符串*/
        if ( client->recv() > 0 ) {
            printf( "Receive from server[%s:%d]:  %s\n", client->getRemoteIp(), client->getRemotePort(), client->getRecvBuffer() );
        }
    }

    return 0;
}
