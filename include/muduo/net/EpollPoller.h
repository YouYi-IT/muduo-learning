#pragma once

#include <muduo/base/Timestamp.h>
#include <muduo/net/Poller.h>

#include <sys/epoll.h>
#include <vector>

namespace muduo::net
{

class Channel;
class EventLoop;

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    // 重写基类Poller里的抽象方法
    muduo::base::Timestamp poll(int timeout_ms, ChannelList* active_channels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int num_events, ChannelList* active_channels) const;
    // 更新channel通道
    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};

}  // namespace muduo::net