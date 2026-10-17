#pragma once

#include <arpa/inet.h>
#include <string>

namespace muduo::net
{

class InetAddress
{
public:
    explicit InetAddress(std::string ip = "0.0.0.0", uint16_t port = 8080);
    explicit InetAddress(const sockaddr_in& addr);

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const;
    void setSockAddr(const sockaddr_in& addr);

private:
    sockaddr_in addr_;
};

}  // namespace muduo::net