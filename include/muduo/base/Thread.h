#pragma once

#include <muduo/base/noncopyable.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace muduo::base
{

class Thread : public noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const;
    pid_t tid() const;
    const std::string& getName() const;

    static int numCreated();

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;  // 不能直接std::thread thread_; 这样就直接开始线程了
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int num_created_;
};

}  // namespace muduo::base