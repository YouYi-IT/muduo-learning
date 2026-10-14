#include "CurrentThread.h"

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    __thread int t_cached_tid=0;

    void cacheTid()
    {
        if(t_cached_tid==0)
        {
            // 通过linux系统调用获取当前线程的tid值
            // syscall(SYS_gettid) 表示调用系统调用号为 SYS_gettid 的系统调用
            t_cached_tid=static_cast<pid_t>(syscall(SYS_gettid));
        }
    }
}