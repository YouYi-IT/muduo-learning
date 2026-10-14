#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      thread_init_callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        // 让EventLoop不在循环
        loop_->quit();
        // 让等待线程处理完现在持有的任务
        thread_.join();
        // 然后执行系统析构函数释放EventLoopThread、EventLoop、Thread
    }
}

EventLoop* EventLoopThread::startLoop()
{
    // 启动新线程，该线程执行该EventLoopThread构造时传入的thread构造参数里的ThreadFunc
    thread_.start();  // 注意，thread_的start方法里每次都make_shared创建新的线程

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 这个方法是单独在新线程里运行，是这个新线程的任务
void EventLoopThread::threadFunc()
{
    // 创建一个独立的EventLoop，和上面的线程一一对应   “one loop per thread”
    EventLoop loop;

    if (thread_init_callback_)
    {
        thread_init_callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();  // EventLoop loop => Poller.poll

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}