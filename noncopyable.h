#pragma once
/*
noncopyable被继承后，派生类对象可以正常的构造析构，但无法进行拷贝构造和拷贝赋值
*/
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};