#include <muduo/net/Socket.h>
#include <muduo/base/Logger.h>
#include <muduo/net/InetAddress.h>

#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

namespace muduo::net
{

Socket::Socket(int sockfd)
    : sockfd_(sockfd)
{
}

Socket::~Socket()
{
    close(sockfd_);
}

int Socket::getFd() const
{
    return sockfd_;
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if (::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0)
    {
        LOG_FATAL("bind sockfd:%d fail", sockfd_);
    }
}

void Socket::listen()
{
    if (::listen(sockfd_, 1024) != 0)
    {
        LOG_FATAL("listen sockfd:%d fail", sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr)
{
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr, sizeof(addr));
    int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

}  // namespace muduo::net