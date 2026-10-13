#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    static ThreadPool& instance();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Task>
    auto submit(Task&& task) -> std::future<std::invoke_result_t<Task>> {
        using Result = std::invoke_result_t<Task>;

        auto packagedTask = std::make_shared<std::packaged_task<Result()>>(std::forward<Task>(task));
        std::future<Result> future = packagedTask->get_future();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push([packagedTask]() { (*packagedTask)(); });
        }
        m_cv.notify_one();
        return future;
    }

private:
    explicit ThreadPool(size_t workerCount);
    ~ThreadPool();

    void workerLoop();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::function<void()>> m_jobs;
    std::vector<std::thread> m_workers;
    bool m_stopping = false;
};
