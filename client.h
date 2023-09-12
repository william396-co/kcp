#pragma once
#include <memory>
#include <iostream>
#include <string>

#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

extern bool is_running;

class Client
{
public:
    Client( const char * ip, uint16_t port, uint32_t conv );
    ~Client();

    void run();
    void input();

    void setmode( int mode );

private:
    void auto_input();

private:
    std::unique_ptr<UdpSocket> client;
    ikcpcb * kcp;
    int md;
};
