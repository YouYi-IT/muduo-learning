#pragma once

#include <muduo/base/Timestamp.h>
#include <muduo/base/noncopyable.h>
#include <unordered_map>
#include <vector>

namespace muduo::net
{

class Channel;
class EventLoop;

// 多路事件分发器的核心IO模块
class Poller : public muduo::base::noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual muduo::base::Timestamp poll(int timeout_ms, ChannelList* active_channels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数Channel是否在当前Poller当中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    // 这是一个静态成员函数，没有 this 指针，可以通过类名调用
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* owner_loop_;  // Poller所属的事件循环EventLoop
};

}  // namespace muduo::net