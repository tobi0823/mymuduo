#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <atomic>

using std::string;

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const string &name = string());
    ~Thread();

    void start();
    int join();

    bool started() const { return started_; }

    pid_t tid() const { return tid_; }
    const string &name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();
    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    string name_;

    static std::atomic_int32_t numCreated_;
};