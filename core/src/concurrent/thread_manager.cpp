#include "server/core/concurrent/thread_manager.hpp"
#include "server/core/concurrent/job_queue.hpp"

namespace server::core {

ThreadManager::ThreadManager(JobQueue& job_queue)
    : job_queue_(job_queue) {}

ThreadManager::~ThreadManager() {
    Stop();
}

void ThreadManager::Start(int num_threads) {
    stopped_ = false;
    threads_.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] { WorkerLoop(); });
    }
}

void ThreadManager::Stop() {
    if (stopped_) return;

    stopped_ = true;
    job_queue_.Stop(); // Wake up any waiting threads

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

void ThreadManager::WorkerLoop() {
    while (!stopped_) {
        Job job = job_queue_.Pop();
        if (!job) { // nullptr job is the signal to stop
            break;
        }
        job();
    }
}

} // namespace server::core
