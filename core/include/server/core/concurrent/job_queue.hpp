#pragma once
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace server::core {

// A simple job type.
using Job = std::function<void()>;

class JobQueue {
public:
    void Push(Job job);
    Job Pop();
    void Stop();

private:
    std::queue<Job> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

} // namespace server::core

