#pragma once
#include <vector>
#include <thread>
#include <memory>

namespace server::core {

class JobQueue;

class ThreadManager {
public:
    ThreadManager(JobQueue& job_queue);
    ~ThreadManager();

    void Start(int num_threads);
    void Stop();

private:
    void WorkerLoop();

    JobQueue& job_queue_;
    std::vector<std::thread> threads_;
    bool stopped_ = false;
};

} // namespace server::core

