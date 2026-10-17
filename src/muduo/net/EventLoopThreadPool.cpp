#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoopThreadPool.h>

#include <memory>

namespace muduo::net
{

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop, const std::string& name_arg)
    : base_loop_(base_loop),
      name_(name_arg),
      started_(false),
      num_threads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
}

void EventLoopThreadPool::setThreadNum(int num_threads)
{
    num_threads_ = num_threads;
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;

    for (int i = 0; i < num_threads_; ++i)
    {
        std::string buf = name_ + std::to_string(i);
        // threads_.push_back(std::make_unique<EventLoopThread>(cb, buf));
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());  // 底层创建线程，绑定一个新的EventLoop,并返回loop的地址
    }

    if (num_threads_ == 0 && cb)
    {
        cb(base_loop_);
    }
}

// 如果是工作在多线程中，base_loop默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = base_loop_;

    if (!loops_.empty())  // 通过轮询获取下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, base_loop_);
    }
    else
    {
        return loops_;
    }
}

bool EventLoopThreadPool::started() const
{
    return started_;
}

const std::string EventLoopThreadPool::get_name() const
{
    return name_;
}

}  // namespace muduo::net