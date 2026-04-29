#pragma once

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <cstddef>

namespace cache {
namespace concurrency {

// Fixed-size thread pool with a bounded task queue.
// Submit tasks with enqueue(); workers drain the queue until shutdown() is called.
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Enqueue a callable. Returns a future for the result.
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Graceful shutdown: drain remaining tasks then join all threads.
    void shutdown();

    [[nodiscard]] size_t thread_count() const noexcept { return workers_.size(); }
    [[nodiscard]] size_t queue_depth()  const noexcept;

private:
    void worker_loop();

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex                mutex_;
    std::condition_variable           cv_;
    std::atomic<bool>                 stop_{false};
};

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using ReturnType = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    auto fut = task->get_future();

    {
        std::unique_lock lock(mutex_);
        if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
}

} // namespace concurrency
} // namespace cache
