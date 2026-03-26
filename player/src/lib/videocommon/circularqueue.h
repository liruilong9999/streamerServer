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
 * 循环队列。
 *
 * \author bf
 * \date 2025/8/5
 *
 * \tparam T Generic type parameter.
 */

template <typename T>
class CircularQueue
{
public:

    /*!
     * 初始化 <see cref="CircularQueue"/> 类的新实例.
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  容量（可选）容量。
     */

    explicit CircularQueue(size_t capacity = 1000000)
        : m_buffer(capacity)
        , m_capacity(capacity)
        , m_front(0)
        , m_rear(0)
        , m_size(0)
        , m_stopped(false)
    {
    }

    /*!
     * 默认析构函数。
     *
     * \author bf
     * \date 2025/8/5
     */

    ~CircularQueue() = default;

    /*!
     * 已删除拷贝构造函数
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  parameter1：第一个参数.
     */

    CircularQueue(const CircularQueue &)             = delete;

    /*!
     * 已删除赋值运算符。
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  parameter1：第一个参数.
     *
     * \returns 此对象的浅拷贝.
     */

    CircularQueue & operator=(const CircularQueue &) = delete;

    /*!
     * 入队（拷贝）
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  值.
     *
     * \returns 如果成功则为真，否则为假。
     */

    bool enqueue(const T & value)
    {
        return enqueueImpl(value);
    }

    /*!
     * 入队（移动）
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param [in,out] value 值.
     *
     * \returns 如果成功则为真，否则为假。
     */

    bool enqueue(T && value)
    {
        return enqueueImpl(std::move(value));
    }

    /*!
     * 出队
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param [in,out] value 值.
     *
     * \returns 如果成功则为真，否则为假。
     */

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
        m_cv.notify_one(); // 唤醒一个生产者
        return true;
    }

    /*!
     * 停止队列（唤醒所有等待线程）
     *
     * \author bf
     * \date 2025/8/5
     */

    void stop()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopped = true;
        m_cv.notify_all();
    }

    /*!
     * 重置停止状态（仅状态位，不负责重启线程）
     *
     * \author bf
     * \date 2025/8/5
     */

    void start()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopped = false;
    }

    /*!
     * 清空队列（线程安全）
     *
     * \author bf
     * \date 2025/8/5
     */

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_front = 0;
        m_rear  = 0;
        m_size  = 0;
        m_cv.notify_all();
    }

    /*!
     * 查询此对象是否已停止
     *
     * \author bf
     * \date 2025/8/5
     *
     * \returns bool
     */

    bool isStopped() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_stopped;
    }

    /*!
     * 获取大小
     *
     * \author bf
     * \date 2025/8/5
     *
     * \returns size_t .
     */

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_size;
    }

private:

    /*!
     * 入队实现。
     *
     * \tparam U Generic type parameter.
     * \param [in,out] value 值.
     *
     * \returns 如果成功则为真，否则为假。
     */

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
        m_cv.notify_one(); // 唤醒一个消费者
        return true;
    }

private:
    // ===== 成员变量 =====
    std::vector<T> m_buffer;	///< 缓冲区
    size_t         m_capacity;  ///< 容量
    size_t         m_front; ///< 前面
    size_t         m_rear;  ///< 后部
    size_t         m_size;  ///< 大小
    bool           m_stopped;   ///< 停止时为真.

    /*!
     * 获取矩阵
     *
     * \returns m: mtx.
     */

    mutable std::mutex      m_mtx;
    std::condition_variable m_cv;   ///< cv
};

#endif // CIRCULARQUEUE_H
