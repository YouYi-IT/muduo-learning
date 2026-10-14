#include "EventLoop.h"
#include "Channel.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Poller.h"

#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <sys/eventfd.h>
#include <unistd.h>

// 防止一个线程创建多个EventLoop
thread_local EventLoop* t_loop_in_this_thread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimesMs = 10000;

// 创建wakeupfd，用来唤醒subReactor，处理新来的channel
int createEventfd()
{
    // 创建一个内核维护 64 位无符号计数器的特殊 fd，用于轻量级事件通知
    // 可被 epoll/poll/select 监听，常用于 Reactor 模型唤醒 EventLoop
    // EFD_NONBLOCK：非阻塞
    // EFD_CLOEXEC：exec 时自动关闭 fd
    // EFD_SEMAPHORE：信号量模式（read 只减 1，不清零）
    // write：写入一个 8 字节 uint64_t，值原子加到内核计数器；计数器 > 0 时，fd 变为可读（触发 EPOLLIN）
    // 默认模式（无 EFD_SEMAPHORE）read：阻塞直到计数器 > 0；读出全部计数值，计数器清零
    // EFD_SEMAPHORE 信号量模式 read：读出固定值 1，计数器 -=1；计数器 > 0 就可读，类似信号量
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      thread_id_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeup_fd_(createEventfd()),
      wakeup_channel_(new Channel(this, wakeup_fd_)),
      calling_pending_functors_(false)
{
    LOG_DEBUG("EventLoop created %p in thread %d", this, thread_id_);
    if (t_loop_in_this_thread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d", t_loop_in_this_thread, thread_id_);
    }
    else
    {
        t_loop_in_this_thread = this;
    }

    // 设置wakeup_fd的事件类型以及发生事件后的回调操作
    wakeup_channel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的EPOLLIN读事件了
    wakeup_channel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    t_loop_in_this_thread = nullptr;
}

// wakeup
// 唤醒loop所在线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_FATAL("EventLoop::wakeup() error %lu bytes instread of 8", n);
    }
}

// epoll_wait 会阻塞，没有 IO 事件的时候，IO 线程卡在 epoll_wait() 睡觉
// 别的线程给 EventLoop 塞任务（回调函数），这个任务不是 socket IO 事件，epoll 感知不到
// 所以必须用 eventfd 主动制造一个可读事件，把 epoll_wait 从阻塞里叫醒，让 Loop 去执行任务队列里的回调
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8", n);
    }
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping", this);

    while (!quit_)
    {
        active_channels_.clear();
        poll_return_time_ = poller_->poll(kPollTimesMs, &active_channels_);
        for (Channel* channel : active_channels_)
        {
            // Poller监听哪些channel发生了事件，然后上报给EventLoop，然后EventLoop通知Channel处理相应事件
            channel->handleEvent(poll_return_time_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        // IO线程  mainloop accept fd《=》channel => subloop
        // mainloop 事先注册一个回调cb（需要subloop来执行）    wakeup唤醒subloop后subloop执行
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping", this);
    looping_ = false;
}

// 退出事件循环
// 1. loop在自己的线程中调用quit（此时自己肯定没有在调用poll了）
// 2. 如果是在其他线程中调用quit（比如在一个subloop中调用了mainloop的quit）
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread())
    {
        wakeup();
    }
}

Timestamp EventLoop::pollReturnTime() const
{
    return poll_return_time_;
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    // 在当前的loop线程中执行cb，就直接执行
    if (isInLoopThread())
    {
        cb();
    }
    else  // 否则queue入队存储起来，并且唤醒loop所在线程，去执行cb
    {
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pending_functors_.emplace_back(cb);
    }

    // 唤醒相应的、需要执行上面回调操作的loop线程
    // || calling_pending_functors_是因为当前loop正在执行回调，当时loop又有了新的回调，所有要再唤醒一下
    if (!isInLoopThread() || calling_pending_functors_)
    {
        wakeup();  // 唤醒loop所在线程
    }
}

// EventLoop方法 => Poller方法
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

// 判断EventLoop对象是否在自己的线程里面
bool EventLoop::isInLoopThread() const
{
    return thread_id_ == CurrentThread::tid();
}

// 执行回调
void EventLoop::doPendingFunctors()
{
    // 创建一个临时数组直接和pending_functors_交换
    // 然后pending_functors_为空了，拿到pending_functors_里的内容，出锁，执行内容
    // 而且pending_functors_不用一直拿在锁里面耽误时间，外面可能还等着用
    std::vector<Functor> functors;
    calling_pending_functors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }

    for (const Functor& functor : functors)
    {
        functor();  // 执行当前loop需要执行的回调操作
    }

    calling_pending_functors_ = false;
}