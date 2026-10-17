#include <muduo/base/CurrentThread.h>
#include <muduo/base/Thread.h>

#include <semaphore.h>

namespace muduo::base
{

std::atomic_int Thread::num_created_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();  // pthread_detach()分离线程
    }
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // 开启线程
    thread_ = std::make_shared<std::thread>(
        [&]()
        {
            // 获取线程tid
            tid_ = CurrentThread::tid();
            sem_post(&sem);
            // 开启一个新线程，专门执行该线程任务
            func_();
        });

    // 这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

bool Thread::started() const
{
    return started_;
}

pid_t Thread::tid() const
{
    return tid_;
}

const std::string& Thread::getName() const
{
    return name_;
}

int Thread::numCreated()
{
    return num_created_;
}

void Thread::setDefaultName()
{
    int num = ++num_created_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}

}  // namespace muduo::base