#include <iostream>
#include <string>
#include <cstring>

#include "util.h"
#include "udpsocket.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

constexpr auto BUFFER_SIZE = 1024;

int main()
{
    struct sockaddr_in addr;
    int clientfd, len = 0;
    socklen_t addr_len = sizeof( struct sockaddr_in );
    /* 建立socket，注意必须是SOCK_DGRAM */
    if ( ( clientfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        perror( "socket" );
        exit( 1 );
    }
    /* 填写sockaddr_in*/
    bzero( &addr, sizeof( addr ) );
    addr.sin_family = AF_INET;
    addr.sin_port = htons( port );
    addr.sin_addr.s_addr = inet_addr( ip );

    std::string writeBuffer;
    char readBuffer[BUFFER_SIZE];
    while ( true ) {
        printf( "Please enter a string to send to server: \n" );
        /* 从标准输入设备取得字符串*/
        writeBuffer.clear();
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            /* 将字符串传送给server端*/
            sendto( clientfd, writeBuffer.data(), writeBuffer.size(), 0, (struct sockaddr *)&addr, addr_len );
        }

        /* 接收server端返回的字符串*/
        std::memset( readBuffer, 0, sizeof( readBuffer ) );
        len = recvfrom( clientfd, readBuffer, sizeof( readBuffer ), 0, (struct sockaddr *)&addr, &addr_len );
        printf( "Receive from server[%s:%d]:  %s\n", inet_ntoa( addr.sin_addr ), ntohs( addr.sin_port ), readBuffer );
    }

    return 0;
}
