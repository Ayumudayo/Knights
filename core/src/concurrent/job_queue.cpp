#include "server/core/concurrent/job_queue.hpp"
#include "server/core/runtime_metrics.hpp"

namespace server::core {

// JobQueue는 생산자 스레드가 DB/백그라운드 작업을 밀어 넣고,
// 워커 스레드가 Pop()으로 끌어가는 간단한 FIFO 구조다.
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
        return nullptr; // nullptr이면 종료 신호
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
