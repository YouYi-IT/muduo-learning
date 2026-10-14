#define MUDEBUG

#include "EpollPoller.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

// epoll_create1(EPOLL_CLOEXEC)
// 当进程调用 execve() / exec()
// 系列函数成功加载并执行新程序时，内核会自动关闭这个 epoll fd 这样可以防止
// epoll fd 被新程序继承，避免文件描述符泄漏
EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop), epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d", errno);
    }
}

EpollPoller::~EpollPoller() { ::close(epollfd_); }

// 重写基类Poller里的抽象方法
Timestamp EpollPoller::poll(int timeout_ms, ChannelList* active_channels)
{
    // poll很平凡，所有用DEBUG，可以关闭，不用随时都打印
    LOG_DEBUG("func=%s => fd total count:%zu", __func__, channels_.size());

    int num_events = epoll_wait(epollfd_, events_.data(),
                                static_cast<int>(events_.size()), timeout_ms);
    int save_errno = errno;
    Timestamp now(Timestamp::now());

    if (num_events > 0)
    {
        LOG_DEBUG("%d events happened", num_events);
        fillActiveChannels(num_events, active_channels);
        // 发生事件数量与数组数量一致，扩容
        // 用vector也还要扩容；vector不会因为epoll_wait写入而自动变长
        if (static_cast<size_t>(num_events) == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (num_events == 0)
    {
        LOG_DEBUG("%s timeout!", __func__);
    }
    else // 外部中断
    {
        if (save_errno != EINTR)
        {
            errno = save_errno;
            LOG_ERROR("EPollPoller::poll error!");
        }
    }
    return now;
}

// Channel update remove => EventLoop updateChannel removeChannel
// => PollerupdateChannel removeChannel
void EpollPoller::updateChannel(Channel* channel)
{
    const int index = channel->getIndex();
    LOG_INFO("func=%s => fd=%d events=%d index=%d", __func__, channel->getFd(),
             channel->getEvents(), channel->getIndex());

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->getFd();
            channels_[fd] = channel;
        }

        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册了
    {
        if (channel->isNoneEvents())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EpollPoller::removeChannel(Channel* channel)
{
    int fd = channel->getFd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d", __func__, channel->getFd());

    const int index = channel->getIndex();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kNew);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int num_events,
                                     ChannelList* active_channels) const
{
    for (int i = 0; i < num_events; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        // EventLoop就拿到了它的poller给它返回的所有发生的事件的channel列表了
        active_channels->push_back(channel);
    }
}

// 更新channel通道
void EpollPoller::update(int operation, Channel* channel)
{
    // epoll_event event;
    // memset(&event,0,sizeof(event));
    struct epoll_event event{}; // C++11 值初始化，等价于零初始化
    event.events = channel->getEvents();
    // data
    // 是联合体，同一时刻只能有效使用其中一个成员，要么是fd，要么是ptr，要么是其他
    // 这里存了fd，后续需要用channels.find(fd)查找，也可以直接存channel*
    event.data.ptr = channel;
    int fd = channel->getFd();

    if (epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d", errno);
        }
    }
}