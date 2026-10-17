#pragma once

#include <muduo/base/Thread.h>
#include <muduo/base/noncopyable.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

namespace muduo::net
{

class EventLoop;

class EventLoopThread : public muduo::base::noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    muduo::base::Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback thread_init_callback_;
};

}  // namespace muduo::net