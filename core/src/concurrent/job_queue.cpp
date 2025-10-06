#include "server/core/concurrent/job_queue.hpp"
#include "server/core/runtime_metrics.hpp"

namespace server::core {

void JobQueue::Push(Job job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
        runtime_metrics::record_job_queue_depth(jobs_.size());
    }
    cv_.notify_one();
}

Job JobQueue::Pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !jobs_.empty() || stopping_; });

    if (stopping_ && jobs_.empty()) {
        runtime_metrics::record_job_queue_depth(jobs_.size());
        return nullptr; // Sentinel for stopping
    }

    Job job = std::move(jobs_.front());
    jobs_.pop();
    runtime_metrics::record_job_queue_depth(jobs_.size());
    return job;
}

void JobQueue::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        runtime_metrics::record_job_queue_depth(jobs_.size());
    }
    cv_.notify_all();
}

} // namespace server::core
