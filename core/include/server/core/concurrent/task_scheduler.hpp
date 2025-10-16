#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

namespace server::core::concurrent {

class TaskScheduler {
public:
    using Clock = std::chrono::steady_clock;
    using Task = std::function<void()>;

    TaskScheduler();
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    void shutdown();

    void post(Task task);
    void schedule(Task task, Clock::duration delay);
    void schedule_every(Task task, Clock::duration interval);

    std::size_t poll(std::size_t max_tasks = static_cast<std::size_t>(-1));

    bool empty() const;

private:
    struct DelayedTask {
        Clock::time_point due;
        Task task;
    };

    struct CompareDue {
        bool operator()(const DelayedTask& a, const DelayedTask& b) const {
            return a.due > b.due;
        }
    };

    void collect_ready(std::vector<Task>& out, std::size_t max_tasks);
    bool is_shutdown() const;

    mutable std::mutex mutex_;
    std::queue<Task> ready_;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, CompareDue> delayed_;
    std::atomic_bool shutdown_{false};
};

} // namespace server::core::concurrent




