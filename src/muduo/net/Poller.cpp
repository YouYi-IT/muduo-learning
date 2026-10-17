#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Poller.h>

namespace muduo::net
{

Poller::Poller(EventLoop* loop)
    : owner_loop_(loop)
{
}

bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->getFd());
    return it != channels_.end() && it->second == channel;
}

}  // namespace muduo::net

// 如果要在这里写newDefaultPoller函数，就要
// #include "PollPoller.h"
// #include <muduo/net/EpollPoller.h>
// 基类里引入派生类，并不好，所有放入DefaultPoller.cpp中
// Poller* Poller::newDefaultPoller(EventLoop* loop)
// {
// }