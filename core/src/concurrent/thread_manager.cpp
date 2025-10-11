#include "server/core/concurrent/thread_manager.hpp"
#include "server/core/concurrent/job_queue.hpp"

namespace server::core {

ThreadManager::ThreadManager(JobQueue& job_queue)
    : job_queue_(job_queue) {}

ThreadManager::~ThreadManager() {
    Stop();
}

void ThreadManager::Start(int num_threads) {
    stopped_.store(false, std::memory_order_relaxed);
    threads_.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] { WorkerLoop(); });
    }
}

void ThreadManager::Stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    job_queue_.Stop(); // 대기 중인 스레드를 깨워 종료시킨다.

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

void ThreadManager::WorkerLoop() {
    while (!stopped_.load(std::memory_order_acquire)) {
        Job job = job_queue_.Pop();
        if (!job) { // nullptr 작업이 오면 종료한다.
            break;
        }
        job();
    }
}

} // namespace server::core
