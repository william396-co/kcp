#pragma once
#include <memory>
#include <iostream>
#include <string>

#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

class Client
{
public:
    Client( const char * ip, uint16_t port, uint32_t conv );
    ~Client();

    void run();
    void input();

    ikcpcb * getKcp() { return kcp; }

private:
    std::unique_ptr<UdpSocket> client;
    ikcpcb * kcp;
};
