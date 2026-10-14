#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"

Poller::Poller(EventLoop* loop):owner_loop_(loop)
{

}

bool Poller::hasChannel(Channel* channel) const
{
    auto it=channels_.find(channel->getFd());
    return it!=channels_.end()&&it->second==channel;
}

// 如果要在这里写newDefaultPoller函数，就要
// #include "PollPoller.h"
// #include "EpollPoller.h"
// 基类里引入派生类，并不好，所有放入DefaultPoller.cpp中
// Poller* Poller::newDefaultPoller(EventLoop* loop)
// {
// }