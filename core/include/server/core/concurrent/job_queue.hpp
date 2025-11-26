#pragma once
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace server::core {

using Job = std::function<void()>;

/**
 * @brief 간단한 작업 큐 (Thread-safe)
 * 
 * 여러 스레드에서 동시에 작업을 Push하고, 워커 스레드들이 Pop하여 처리할 수 있도록
 * Mutex와 Condition Variable을 사용하여 동기화합니다.
 */
class JobQueue {
public:
    /**
     * @brief 작업을 큐에 추가합니다.
     * @param job 실행할 함수 객체 (std::function<void()>)
     */
    void Push(Job job);

    /**
     * @brief 작업을 큐에서 꺼냅니다. (Blocking)
     * 큐가 비어있으면 작업이 들어올 때까지 대기합니다.
     * @return 꺼낸 작업 (Stop() 호출 시 빈 작업 반환 가능)
     */
    Job Pop();

    /**
     * @brief 큐를 정지시키고 대기 중인 모든 스레드를 깨웁니다.
     */
    void Stop();

private:
    std::queue<Job> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

} // namespace server::core
