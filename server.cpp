#include <iostream>
#include <string>
#include <cstring>

#include "util.h"
#include "udpsocket.h"

constexpr auto port = 9527;
constexpr auto BUFFER_SIZE = 1024;

int main()
{
    struct sockaddr_in addr;
    int listenfd, len = 0;
    socklen_t addr_len = sizeof( struct sockaddr_in );

    /* 建立socket，注意必须是SOCK_DGRAM */
    if ( ( listenfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        perror( "socket" );
        exit( 1 );
    }
    /* 填写sockaddr_in 结构 */
    addr.sin_family = AF_INET;
    addr.sin_port = htons( port );
    addr.sin_addr.s_addr = htonl( INADDR_ANY ); // 接收任意IP发来的数据
                                                /* 绑定socket */
    if ( bind( listenfd, (struct sockaddr *)&addr, sizeof( addr ) ) < 0 ) {
        perror( "connect" );
        exit( 1 );
    }

    char readBuffer[BUFFER_SIZE];
    while ( 1 ) {
        memset( &readBuffer, 0, sizeof( readBuffer ) );
        len = recvfrom( listenfd, readBuffer, sizeof( readBuffer ), 0, (struct sockaddr *)&addr, &addr_len );
        /* 显示client端的网络地址和收到的字符串消息 */
        printf( "Received a string from client[ %s:%d], string is: %s\n",
            inet_ntoa( addr.sin_addr ),
            ntohs( addr.sin_port ),
            readBuffer );
        /* 将收到的字符串消息返回给client端 */
        sendto( listenfd, readBuffer, len, 0, (struct sockaddr *)&addr, addr_len );
    }

    return 0;
}
