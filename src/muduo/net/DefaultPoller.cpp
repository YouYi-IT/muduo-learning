#include <muduo/net/EpollPoller.h>
#include <muduo/net/Poller.h>

#include <stdlib.h>

namespace muduo::net
{

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;  // 生成poll的实例
    }
    else
    {
        return new EpollPoller(loop);  // 生成epoll的实例
    }
}

}  // namespace muduo::net