#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <iostream>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <utility>

template <typename T>
class CircularQueue
{
private:
    std::unique_ptr<T[]>    buffer;   // 使用智能指针管理原始内存
    size_t                  front;    // 头部索引（无符号类型）
    size_t                  rear;     // 尾部索引
    size_t                  capacity; // 队列容量
    size_t                  size;     // 当前元素数量
    bool                    stopped;  // 停止标志
    std::mutex              mtx;      // 互斥锁
    std::condition_variable cv;       // 条件变量

public:
    // 显式构造函数，避免隐式转换
    explicit CircularQueue(size_t capacity = 1000000)
        : buffer(std::make_unique<T[]>(capacity))
        , front(0)
        , rear(0)
        , capacity(capacity)
        , size(0)
        , stopped(false)
    {}

    ~CircularQueue()
    {
        // 析构时显式销毁剩余元素
        while (size > 0)
        {
            buffer[front].~T();
            front = (front + 1) % capacity;
            --size;
        }
    }

    // 禁止拷贝和赋值
    CircularQueue(const CircularQueue &)             = delete;
    CircularQueue & operator=(const CircularQueue &) = delete;

    // 入队（拷贝语义）
    bool enqueue(const T & value)
    {
        return enqueue_impl(value);
    }

    // 入队（移动语义）
    bool enqueue(T && value)
    {
        return enqueue_impl(std::move(value));
    }

    // 出队
    bool dequeue(T & value)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !isEmpty() || stopped;
        });

        if (stopped && isEmpty())
            return false;

        // 移动赋值并显式销毁元素
        value = std::move(buffer[front]);
        buffer[front].~T();
        front = (front + 1) % capacity;
        --size;

        cv.notify_one(); // 仅唤醒一个生产者
        return true;
    }
    void start()
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = false;
        cv.notify_all(); // 必须唤醒所有线程
    }
    void stop()
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = true;
        cv.notify_all(); // 必须唤醒所有线程
    }

    void clear()
    {
        T value;
        while (isEmpty() == false)
        {
            dequeue(value);
        }
            
    }

    bool   isEmpty() const { return size == 0; }
    bool   isFull() const { return size == capacity; }
    size_t getSize() const { return size; }
    bool   isStopped() const { return stopped; }

private:
    // 通用入队实现
    template <typename U>
    bool enqueue_impl(U && value)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !isFull() || stopped;
        });

        if (stopped)
            return false;

        // 使用完美转发构造新元素
        new (buffer.get() + rear) T(std::forward<U>(value));
        rear = (rear + 1) % capacity;
        ++size;

        cv.notify_one(); // 仅唤醒一个消费者
        return true;
    }
};

#endif // CIRCULARQUEUE_H