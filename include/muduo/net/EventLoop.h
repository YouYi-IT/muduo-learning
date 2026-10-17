#pragma once

#include <muduo/base/Timestamp.h>
#include <muduo/base/noncopyable.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace muduo::net
{

class Channel;
class Poller;

class EventLoop : public muduo::base::noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    muduo::base::Timestamp pollReturnTime() const;

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    // 唤醒loop所在线程
    void wakeup();

    // EventLoop方法 => Poller方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const;

private:
    void handleRead();         // wakeup
    void doPendingFunctors();  // 执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  // 原子操作，通过CAS实现
    std::atomic_bool quit_;     // 标志退出loop循环
    const pid_t thread_id_;     // 记录当前loop所在线程的id
    // Linux 下线程 TID 用 gettid() 或 syscall(SYS_gettid) 获取，类型是 pid_t
    // POSIX 的线程 ID 是 pthread_t，它不保证是整数，不能直接和 pid_t 混用
    muduo::base::Timestamp poll_return_time_;  // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    // 主要作用：当mainloop获取一个新用户的channel
    // 通过轮询算法选择一个subloop，通过该成员变量唤醒subloop处理channel
    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;

    ChannelList active_channels_;

    std::atomic_bool calling_pending_functors_;  // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pending_functors_;      // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                           // 用来保护上面的vector容器的线程安全操作
};

}  // namespace muduo::net