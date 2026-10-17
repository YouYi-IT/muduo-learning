#pragma once

// 问题：为什么有 std::this_thread::get_id()，但不用？
// 答：
// 1. std::this_thread::get_id() 返回 std::thread::id，标准不保证它和 pthread_t、内核 TID 有任何关系
//    没法拿它去调 tgkill、gettid、pthread_setname_np 这些系统接口
//    CurrentThread::tid() 返回的是 Linux 内核线程 ID（TID），类型是 pid_t
// 2. CurrentThread 做了缓存，性能更好，虽然std::this_thread::get_id()也不慢

// namespace CurrentThread：命名空间，只是给这个全局变量加个"姓氏"，避免和别的t_cached_tid重名
//  extern：声明，告诉编译器"这个变量在别的 .cpp 文件里定义了，你先用着"
//  __thread：GCC 的扩展关键字，表示线程局部存储，每个线程都有自己独立的一份 t_cached_tid
//  C++11 起标准写法是 thread_local

namespace muduo::base
{

namespace CurrentThread
{
extern __thread int t_cached_tid;

void cacheTid();

// inline:
// 用来告诉编译器“这个函数（或变量）可以在多个 .cpp 文件里重复定义，链接时只保留一份”
// 同时建议编译器将函数体直接展开到调用处，减少函数调用开销
// 类内定义的成员函数自动是 inline
// 好处：运行更快（省去调用开销）
// 代价：代码体积变大（函数体被复制到每个调用点）
// 但注意：inline 只是建议，不是命令
inline int tid()
{
    // t_cached_tid == 0 很少发生，属于冷门分支
    // 编译器会把这个分支的代码放到冷代码段，CPU 预取时优先走 if 外面的主流程，减少分支预测失败的开销
    if (__builtin_expect(t_cached_tid == 0, 0))
    {
        cacheTid();
    }
    return t_cached_tid;
}
}  // namespace CurrentThread

}  // namespace muduo::base