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

    /**
     * @brief 스케줄러를 종료하고 대기 중인 작업을 정리합니다.
     */
    void shutdown();

    /**
     * @brief 작업을 즉시 실행 대기열에 추가합니다. (지연 없음)
     */
    void post(Task task);

    /**
     * @brief 일정 시간 후에 실행되도록 작업을 예약합니다.
     * @param task 실행할 작업
     * @param delay 지연 시간
     */
    void schedule(Task task, Clock::duration delay);

    /**
     * @brief 일정 간격으로 반복 실행되도록 작업을 예약합니다.
     * 주의: 작업 내부에서 다음 스케줄링을 스스로 하는 방식이 아니라,
     * 외부에서 주기적으로 작업을 생성하여 넣어주는 방식입니다.
     * (현재 구현상 schedule_every는 내부적으로 재귀적 호출을 하지 않음)
     */
    void schedule_every(Task task, Clock::duration interval);

    /**
     * @brief 대기열에 있는 작업들을 실행합니다.
     * 메인 루프나 별도의 스레드에서 주기적으로 호출해야 합니다.
     * @param max_tasks 한 번에 처리할 최대 작업 수
     * @return 실행된 작업 수
     */
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




