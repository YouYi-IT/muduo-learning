#pragma once

#include <muduo/base/noncopyable.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

#include <functional>

namespace muduo::net
{

class EventLoop;
class InetAddress;

class Acceptor : public muduo::base::noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress inet_addr)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb);

    bool listenning() const;
    void listen();

private:
    void handleRead();

    EventLoop* loop_;  // Acceptor用的就是用户定义的那个baseloop（mainloop）
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listenning_;
};

}  // namespace muduo::net