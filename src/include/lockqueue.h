#pragma once

#include <queue>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>

// 异步的日志写队列

template <typename T>
class LockQueue
{
public:
    void push(const T &value) // 入队
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(value);
        _cv.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]()
                 { return !_queue.empty() || _is_stop; });
        if (_is_stop && _queue.empty())
        {
            throw std::runtime_error("queue closed");
        }
        T data = _queue.front();
        _queue.pop();
        return data;
    }
    bool empty()
    {
        std::lock_guard<std::mutex> lcok(_mutex);
        return _queue.empty();
    }
    void shutdown()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _is_stop = true;
        _cv.notify_all();
    }

private:
    bool _is_stop = false;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::queue<T> _queue;
};