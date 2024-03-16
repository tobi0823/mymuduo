#pragma once
// 用于获取当前线程的线程ID

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid; //__thread表示这是线程局部存储的变量，每个线程都有一份独立的副本

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}