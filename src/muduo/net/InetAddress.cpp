#include "InetAddress.h"

#include <string.h>

InetAddress::InetAddress(std::string ip, uint16_t port)
{
    addr_.sin_family=AF_INET;
    addr_.sin_port=htons(port);
    addr_.sin_addr.s_addr=inet_addr(ip.c_str());
}

InetAddress::InetAddress(const sockaddr_in& addr) : addr_(addr) {}

std::string InetAddress::toIp() const
{
    char buf[INET_ADDRSTRLEN]={0};
    inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const
{
    char buf[INET_ADDRSTRLEN+8]={0};
    inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    size_t end=strlen(buf);
    uint16_t port=ntohs(addr_.sin_port);
    sprintf(buf+end,":%u",port);
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

const sockaddr_in* InetAddress::getSockAddr() const
{
    return &addr_;
}

void InetAddress::setSockAddr(const sockaddr_in& addr)
{
    addr_=addr;
}


// #include<iostream>
// int main()
// {
//     InetAddress inet_addr("192.168.88.129",5200);
//     std::cout<<inet_addr.toIp()<<std::endl;
//     std::cout<<inet_addr.toIpPort()<<std::endl;
//     std::cout<<inet_addr.toPort()<<std::endl;
//     return 0;
// }