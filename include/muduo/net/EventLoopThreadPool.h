#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool:public noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* base_loop,const std::string& name_arg);
    ~EventLoopThreadPool();

    void setThreadNum(int num_threads = 0);

    void start(const ThreadInitCallback& cb=ThreadInitCallback());

    // 如果是工作在多线程中，base_loop默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const;
    const std::string get_name() const;

private:
    EventLoop* base_loop_;  // 如果不设置线程数量，就会只有base_loop，单线程
    std::string name_;
    bool started_;
    int num_threads_;
    size_t next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};