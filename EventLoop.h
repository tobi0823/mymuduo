#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

// 事件循环类，主要包含了Channel和Poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);   // 在当前loop中执行
    void queueInLoop(Functor cb); // 把cb放入队列中，唤醒loop所在的线程，执行cb

    void wakeup(); // 唤醒loop所在的线程

    // Eventloop的方法调用Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;                // 原子操作的布尔值 表示进入循环
    std::atomic_bool quit_;                   // 标志退出loop循环
    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    const pid_t threadId_;                    // 记录当前loop所在线程的id
    Timestamp pollReturnTime_;                // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;          // 指向Poller类的智能指针，用于处理事件的监听和分发

    int wakeupFd_; // 当mainloop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                     // 互斥锁用来保护上面vector容器线程安全操作
};