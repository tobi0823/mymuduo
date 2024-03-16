#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// 防止一个线程创建多个EventLoop __thread (thread_local)
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的poller IO复用接口的超时时间
const int kPollTimesMs = 10000;

// 创建wakeupfd，用来notify唤醒subreactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll(); // 对所有事件不感兴趣
    wakeupChannel_->remove();     // 从Poller中移除
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimesMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生了事件，上报给EventLoop，通知Channel处理相应的事件
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = NULL;
        // 执行当前EventLoop事件循环需要处理的回调操作
        doPendingFunctors(); // 处理待执行的用户回调函数
    }

    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

// 退出事件循环 1. loop在自己的线程中调用quit 2. 在非loop的线程中调用loop的quit
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread()) // 如果是在其他线程中调用quit，   在一个subloop（工作线程）中调用mainloop（IO线程）的quit
    {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb，需要唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // push_back(std::move(cb))
    }

    // 唤醒相应的需要执行上面回调操作的loop线程
    // ||callingPendingFunctors_ 当前loop正在执行回调，但是loop又有了新的回调，这时需要唤醒mainloop来处理新加入的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{

    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 将pendingFunctors_装入局部定义的vector functors中 将pendingFunctors解放了 不影响mainloop继续写入
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}