#pragma once
#include <vector>
#include <thread>
#include <memory>
#include <atomic>

namespace server::core {

class JobQueue;

class ThreadManager {
public:
    /**
     * @brief ThreadManager 생성자
     * @param job_queue 워커 스레드들이 작업을 가져올 큐
     */
    ThreadManager(JobQueue& job_queue);
    ~ThreadManager();

    /**
     * @brief 지정된 수만큼 워커 스레드를 생성하고 시작합니다.
     * @param num_threads 생성할 스레드 수
     */
    void Start(int num_threads);

    /**
     * @brief 모든 워커 스레드를 정지하고 종료될 때까지 대기합니다.
     */
    void Stop();

private:
    void WorkerLoop();

    JobQueue& job_queue_;
    std::vector<std::thread> threads_;
    std::atomic<bool> stopped_{true};
};

} // namespace server::core

