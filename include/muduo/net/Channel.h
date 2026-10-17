#pragma once

#include <functional>
#include <memory>
#include <muduo/base/Timestamp.h>
#include <muduo/base/noncopyable.h>

namespace muduo::net
{

class EventLoop;

// 封装sockfd和要监听的事件event
class Channel : public muduo::base::noncopyable
{
public:
    using EventCallback = std::function<void()>;
    // 函数参数、模板参数里面的类型名，会走 ADL，所以可以直接写Timestamp
    // 编译器找函数名的时候，不光在当前作用域找，还会自动去函数参数类型所在的命名空间里面查找
    using ReadEventCallback = std::function<void(const muduo::base::Timestamp&)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(const muduo::base::Timestamp& receive_time);

    void setReadCallback(ReadEventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

    // 防止Channel被remove掉后，Channel还在执行Callback
    void tie(const std::shared_ptr<void>& obj);

    int getFd() const;
    int getEvents() const;
    void setRevents(int revents);

    // 设置fd相应的事件状态
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    // 返回fd当前的事件状态
    bool isNoneEvents() const;
    bool isReading() const;
    bool isWriting() const;

    int getIndex();
    void setIndex(int idx);

    EventLoop* ownerLoop() const;
    void remove();

private:
    void update();
    void handleEventWithGuard(const muduo::base::Timestamp& receive_time);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;  // 事件循环
    const int fd_;     // fd,Poller监听的对象
    int events_;       // 注册的fd感兴趣的事件
    int revents_;      // poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;  // 改用enable_shared_from_this
    bool tied_;

    // 因为Channel内可以获知fd最终发生的具体事件revents，使用Channel负责事件回调的调用
    ReadEventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

}  // namespace muduo::net