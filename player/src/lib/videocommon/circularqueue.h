/*!
 * \file  .\src\lib\videocommon\circularqueue.h.
 *
 * Declares the circularqueue class
 */

#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <utility>

/*!
 * Thread-safe circular queue.
 *
 * \tparam T element type.
 */
template <typename T>
class CircularQueue
{
public:
    explicit CircularQueue(size_t capacity = 1000000)
        : m_buffer(capacity)
        , m_capacity(capacity)
        , m_front(0)
        , m_rear(0)
        , m_size(0)
        , m_stopped(false)
    {
    }

    ~CircularQueue() = default;

    CircularQueue(const CircularQueue &)             = delete;
    CircularQueue & operator=(const CircularQueue &) = delete;

    bool enqueue(const T & value)
    {
        return enqueueImpl(value);
    }

    bool enqueue(T && value)
    {
        return enqueueImpl(std::move(value));
    }

    bool dequeue(T & value)
    {
        std::unique_lock<std::mutex> lock(m_mtx);

        m_cv.wait(lock, [this] {
            return m_size > 0 || m_stopped;
        });

        if (m_stopped && (m_size == 0))
        {
            return false;
        }

        value   = std::move(m_buffer[m_front]);
        m_front = (m_front + 1) % m_capacity;
        --m_size;

        lock.unlock();
        m_cv.notify_one();
        return true;
    }

    // Non-blocking pop for UI thread.
    bool tryDequeue(T & value)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_size == 0)
        {
            return false;
        }

        value   = std::move(m_buffer[m_front]);
        m_front = (m_front + 1) % m_capacity;
        --m_size;

        lock.unlock();
        m_cv.notify_one();
        return true;
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopped = true;
        m_cv.notify_all();
    }

    void start()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopped = false;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_front = 0;
        m_rear  = 0;
        m_size  = 0;
        m_cv.notify_all();
    }

    bool isStopped() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_stopped;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_size;
    }

private:
    template <typename U>
    bool enqueueImpl(U && value)
    {
        std::unique_lock<std::mutex> lock(m_mtx);

        m_cv.wait(lock, [this] {
            return m_size < m_capacity || m_stopped;
        });

        if (m_stopped)
        {
            return false;
        }

        m_buffer[m_rear] = std::forward<U>(value);
        m_rear           = (m_rear + 1) % m_capacity;
        ++m_size;

        lock.unlock();
        m_cv.notify_one();
        return true;
    }

private:
    std::vector<T> m_buffer;
    size_t         m_capacity;
    size_t         m_front;
    size_t         m_rear;
    size_t         m_size;
    bool           m_stopped;

    mutable std::mutex      m_mtx;
    std::condition_variable m_cv;
};

#endif // CIRCULARQUEUE_H
