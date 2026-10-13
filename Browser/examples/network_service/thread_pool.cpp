#include "thread_pool.h"

#include <algorithm>

ThreadPool& ThreadPool::instance() {
    static ThreadPool pool(std::max(2u, std::thread::hardware_concurrency()));
    return pool;
}

ThreadPool::ThreadPool(size_t workerCount) {
    m_workers.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        m_workers.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_cv.notify_all();

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stopping || !m_jobs.empty(); });

            if (m_stopping && m_jobs.empty()) {
                return;
            }

            job = std::move(m_jobs.front());
            m_jobs.pop();
        }
        job();
    }
}
