#pragma once
#include <memory>
#include <iostream>
#include <string>

#include "util.h"
#include "udpsocket.h"

class Client
{
public:
    Client( const char * ip, uint16_t port );
    ~Client() {}

    void run();
    void input();

private:
    std::unique_ptr<UdpSocket> client;
};
