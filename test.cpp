//=====================================================================
//
// test.cpp - kcp 测试用例
//
// 说明：
// gcc test.cpp -o test -lstdc++
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include "test.h"

#define BUFF_LEN 2000
#define TEST_CNT 1000

#define USE_CPP_VERSION

#ifndef USE_CPP_VERSION
#include "ikcp.h"
#else
#include "kcpex.h"
#endif // !USE_CPP_VERSION

// 模拟网络
LatencySimulator * vnet;

// 模拟网络：模拟发送一个 udp包
#ifndef USE_CPP_VERSION

int udp_output( const char * buf, int len, ikcpcb * kcp, void * user )
{
    union
    {
        int id;
        void * ptr;
    } parameter;
    parameter.ptr = user;
    vnet->send( parameter.id, buf, len );
    return 0;
}

#else
int udp_output( const char * buf, int len, ikcpcb * kcp, void * user )
{

    union
    {
        int id;
        void * ptr;
    } parameter;
    parameter.ptr = user;
    vnet->send( parameter.id, buf, len );
    return 0;
}

#endif // !USE_CPP_VERSION

// 测试用例
void test( int mode )
{
    // 创建模拟网络：丢包率10%，Rtt 60ms~125ms
    vnet = new LatencySimulator( 10, 60, 125 );

    // 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
    // 最后一个是 user参数，用来传递标识
#ifndef USE_CPP_VERSION
    ikcpcb * kcp1 = ikcp_create( 0x11223344, (void *)0 );
    ikcpcb * kcp2 = ikcp_create( 0x11223344, (void *)1 );
#else
    std::unique_ptr<KcpEx> client = std::make_unique<KcpEx>( 0x11223344, (void *)0 );
    std::unique_ptr<KcpEx> server = std::make_unique<KcpEx>( 0x11223344, (void *)1 );
#endif // !USE_CPP_VERSION

       // 设置kcp的下层输出，这里为 udp_output，模拟udp网络输出函数
#ifndef USE_CPP_VERSION
    kcp1->output = udp_output;
    kcp2->output = udp_output;
#else
    client->set_output( udp_output );
    server->set_output( udp_output );
#endif

    IUINT32 current = iclock();
    IUINT32 slap = current + 20;
    IUINT32 index = 0;
    IUINT32 next = 0;
    IINT64 sumrtt = 0;
    int count = 0;
    int maxrtt = 0;

    // 配置窗口大小：平均延迟200ms，每20ms发送一个包，
    // 而考虑到丢包重发，设置最大收发窗口为128
#ifndef USE_CPP_VERSION
    ikcp_wndsize( kcp1, 128, 128 );
    ikcp_wndsize( kcp2, 128, 128 );
#else
    client->set_wndsize( 128, 128 );
    server->set_wndsize( 128, 128 );
#endif // ! USE_CPP_VERSION

    // 判断测试用例的模式
    if ( mode == 0 ) {
        // 默认模式
#ifndef USE_CPP_VERSION
        ikcp_nodelay( kcp1, 0, 10, 0, 0 );
        ikcp_nodelay( kcp2, 0, 10, 0, 0 );
#else
        client->set_nodelay( false, 10, 0, 0 );
        server->set_nodelay( false, 10, 0, 0 );
#endif // !USE_CPP_VERSION
    } else if ( mode == 1 ) {
        // 普通模式，关闭流控等
#ifndef USE_CPP_VERSION
        ikcp_nodelay( kcp1, 0, 10, 0, 1 );
        ikcp_nodelay( kcp2, 0, 10, 0, 1 );
#else
        client->set_nodelay( false, 10, 0, 1 );
        server->set_nodelay( false, 10, 0, 1 );
#endif // !USE_CPP_VERSION
    } else {
        // 启动快速模式
        // 第二个参数 nodelay-启用以后若干常规加速将启动
        // 第三个参数 interval为内部处理时钟，默认设置为 10ms
        // 第四个参数 resend为快速重传指标，设置为2
        // 第五个参数 为是否禁用常规流控，这里禁止
#ifndef USE_CPP_VERSION
        ikcp_nodelay( kcp1, 2, 10, 2, 1 );
        ikcp_nodelay( kcp2, 2, 10, 2, 1 );
        kcp1->rx_minrto = 10;
        kcp1->fastresend = 1;
#else
        client->set_nodelay( true, 10, 2, 1 );
        server->set_nodelay( true, 10, 2, 1 );
        client->set_rx_minrto( 10 );
        client->set_fastresend( 1 );
#endif // !USE_CPP_VERSION
    }

    char buffer[BUFF_LEN];
    int len;

    IUINT32 ts1 = iclock();

    while ( 1 ) {
        isleep( 1 );
        current = iclock();
#ifndef USE_CPP_VERSION
        ikcp_update( kcp1, iclock() );
        ikcp_update( kcp2, iclock() );
#else
        client->update( iclock() );
        server->update( iclock() );
#endif // !USE_CPP_VERSION

        // 每隔 20ms，kcp1发送数据
        for ( ; current >= slap; slap += 20 ) {
            ( (IUINT32 *)buffer )[0] = index++;
            ( (IUINT32 *)buffer )[1] = current;

            // 发送上层协议包
#ifndef USE_CPP_VERSION
            ikcp_send( kcp1, buffer, BUFF_LEN );
#else
            client->send( buffer, BUFF_LEN );

#endif // !USE_CPP_VERSION
        }

        // 处理虚拟网络：检测是否有udp包从p1->p2
        while ( 1 ) {
            len = vnet->recv( 1, buffer, BUFF_LEN );
            if ( len < 0 ) break;
                // 如果 p2收到udp，则作为下层协议输入到kcp2
#ifndef USE_CPP_VERSION
            ikcp_input( kcp2, buffer, len );
#else
            server->input( buffer, len );
#endif
        }

        // 处理虚拟网络：检测是否有udp包从p2->p1
        while ( 1 ) {
            len = vnet->recv( 0, buffer, BUFF_LEN );
            if ( len < 0 ) break;
                // 如果 p1收到udp，则作为下层协议输入到kcp1
#ifndef USE_CPP_VERSION
            ikcp_input( kcp1, buffer, len );
#else
            client->input( buffer, len );
#endif // !USE_CPP_VERSION
        }

        // kcp2接收到任何包都返回回去
        while ( 1 ) {
#ifndef USE_CPP_VERSION
            len = ikcp_recv( kcp2, buffer, BUFF_LEN );
#else
            server->recv( buffer, BUFF_LEN );
#endif // !USE_CPP_VERSION
       // 没有收到包就退出
            if ( len < 0 ) break;
                // 如果收到包就回射
#ifndef USE_CPP_VERSION
            ikcp_send( kcp2, buffer, len );
#else
            server->send( buffer, len );
#endif // !USE_CPP_VERSION
        }

        // kcp1收到kcp2的回射数据
        while ( 1 ) {
#ifndef USE_CPP_VERSION
            len = ikcp_recv( kcp1, buffer, BUFF_LEN );
#else
            len = client->recv( buffer, BUFF_LEN );
#endif // !USE_CPP_VERSION
       // 没有收到包就退出
            if ( len < 0 ) break;
            IUINT32 sn = *(IUINT32 *)( buffer + 0 );
            IUINT32 ts = *(IUINT32 *)( buffer + 4 );
            IUINT32 rtt = current - ts;

            if ( sn != next ) {
                // 如果收到的包不连续
                printf( "ERROR sn %d<->%d\n", (int)count, (int)next );
                return;
            }

            next++;
            sumrtt += rtt;
            count++;
            if ( rtt > (IUINT32)maxrtt ) maxrtt = rtt;

            printf( "[RECV] mode=%d sn=%d rtt=%d len=%d\n", mode, (int)sn, (int)rtt, len );
        }
        if ( next > TEST_CNT ) break;
    }

    ts1 = iclock() - ts1;

#ifndef USE_CPP_VERSION
    ikcp_release( kcp1 );
    ikcp_release( kcp2 );
#endif // !USE_CPP_VERSION

    const char * names[3] = { "default", "normal", "fast" };
    printf( "%s mode result (%dms):\n", names[mode], (int)ts1 );
    printf( "avgrtt=%d maxrtt=%d tx=%d\n", (int)( sumrtt / count ), (int)maxrtt, (int)vnet->tx1 );
    printf( "press enter to next ...\n" );
    char ch;
    scanf( "%c", &ch );
}

int main()
{
    test( 0 ); // 默认模式，类似 TCP：正常模式，无快速重传，常规流控
    test( 1 ); // 普通模式，关闭流控等
    test( 2 ); // 快速模式，所有开关都打开，且关闭流控
    return 0;
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507

normal mode result (20131ms):
avgrtt=156 maxrtt=571

fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/

