#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}

void Channel::setReadCallback(ReadEventCallback cb)
{
    read_callback_ = std::move(cb);
}
void Channel::setWriteCallback(EventCallback cb)
{
    write_callback_ = std::move(cb);
}
void Channel::setCloseCallback(EventCallback cb)
{
    close_callback_ = std::move(cb);
}
void Channel::setErrorCallback(EventCallback cb)
{
    error_callback_ = std::move(cb);
}

// 防止Channel被remove掉后，Channel还在执行Callback
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

int Channel::getFd() const
{
    return fd_;
}
int Channel::getEvents() const
{
    return events_;
}
void Channel::setRevents(int revents)
{
    revents_ = revents;
}

// 设置fd相应的事件状态
void Channel::enableReading()
{
    events_ |= kReadEvent;
    update();
}
void Channel::disableReading()
{
    events_ &= ~kReadEvent;
    update();
}
void Channel::enableWriting()
{
    events_ |= kWriteEvent;
    update();
}
void Channel::disableWriting()
{
    events_ &= ~kWriteEvent;
    update();
}
void Channel::disableAll()
{
    events_ = kNoneEvent;
    update();
}

// 返回fd当前的事件状态
bool Channel::isNoneEvents() const
{
    return events_ == 0;
}
bool Channel::isReading() const
{
    return events_ & kReadEvent;
}
bool Channel::isWriting() const
{
    return events_ & kWriteEvent;
}

int Channel::getIndex()
{
    return index_;
}
void Channel::setIndex(int idx)
{
    index_ = idx;
}

EventLoop* Channel::ownerLoop() const
{
    return loop_;
}

// fd收到poller通知后，处理事件
void Channel::handleEvent(Timestamp receive_time)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();  // 将weak_ptr提升为shared_ptr
        // 提升失败，说明对象已经没有了
        if (guard)
        {
            handleEventWithGuard(receive_time);
        }
    }
    else
    {
        handleEventWithGuard(receive_time);
    }
}
void Channel::handleEventWithGuard(Timestamp receive_time)
{
    LOG_INFO("channel handleEvent revents : %d", revents_);

    // 不能return，必须所有事件都检查
    
    // 如果没有 !(revents_ & EPOLLIN)
    // 只要看到 EPOLLHUP 就立刻调 close_callback_() 关闭连接，就会丢掉 socket 缓冲区里还没读完的数据
    // 只有不正常关闭才会走这个，一般正常关闭都走read_callback里执行的 handleRead 里n==0的close
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (close_callback_)
        {
            close_callback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (error_callback_)
        {
            error_callback_();
        }
    }

    if (revents_ & kReadEvent)
    {
        if (read_callback_)
        {
            read_callback_(receive_time);
        }
    }

    if (revents_ & kWriteEvent)
    {
        if (write_callback_)
        {
            write_callback_();
        }
    }
}

// 依赖EventLoop

// 当改变Channel里面的fd对应的events事件后，update去改变epoller里fd的响应事件（通过epoll_ctl）
// EventLoop => ChannelList + Epoller   所有update通过EventLoop去与Epoller联系
void Channel::update()
{
    // loop_->updateChannel(this);
}
void Channel::remove()
{
    // loop_->removeChannel(this);
}