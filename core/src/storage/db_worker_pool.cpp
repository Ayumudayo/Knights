#include "server/core/storage/db_worker_pool.hpp"

#include <stdexcept>
#include <utility>

#include "server/core/runtime_metrics.hpp"
#include "server/core/util/log.hpp"

namespace server::core::storage {

namespace {
std::size_t normalize_worker_count(std::size_t count) {
    if (count == 0) {
        count = std::thread::hardware_concurrency();
    }
    return count == 0 ? 1 : count;
}
} // namespace

DbWorkerPool::DbWorkerPool(std::shared_ptr<IConnectionPool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("DbWorkerPool requires a valid connection pool");
    }
}

DbWorkerPool::~DbWorkerPool() {
    stop();
}

// DB 작업은 네트워크 트랜잭션과 독립적으로 재시도되어야 하므로 별도의 worker pool로 분리한다.
// start()는 worker_count(0이면 HW 동시성)를 기준으로 스레드를 띄우고,
// stop()은 큐에 중단 신호를 넣어 안전하게 종료한다.
void DbWorkerPool::start(std::size_t worker_count) {
    if (running_) {
        return;
    }
    queue_.reset();
    stopping_.store(false, std::memory_order_relaxed);
    worker_count = normalize_worker_count(worker_count);
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this, i]() { worker_loop(i); });
    }
    running_.store(true, std::memory_order_relaxed);
}

void DbWorkerPool::stop() {
    if (!running_) {
        return;
    }
    stopping_.store(true, std::memory_order_relaxed);
    queue_.stop();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    running_.store(false, std::memory_order_relaxed);
    queue_.reset();
    runtime_metrics::record_db_job_queue_depth(0);
}

bool DbWorkerPool::running() const {
    return running_.load(std::memory_order_relaxed);
}

void DbWorkerPool::submit(Task task, bool auto_commit) {
    if (!task) {
        return;
    }
    if (!running_) {
        throw std::runtime_error("DbWorkerPool is not running");
    }
    queue_.push(Job{std::move(task), auto_commit});
    runtime_metrics::record_db_job_queue_depth(queue_.size());
}

void DbWorkerPool::worker_loop(std::size_t index) {
    (void)index;
    while (true) {
        auto job = queue_.pop_blocking();
        if (!job.has_value()) {
            if (stopping_.load(std::memory_order_relaxed)) {
                break;
            }
            continue;
        }
        runtime_metrics::record_db_job_queue_depth(queue_.size());
        // DB 호출은 외부 시스템 상태에 따라 예외가 자주 발생하므로,
        // 작업 단위별로 metrics를 남기고 계속 루프를 돌린다.
        try {
            process_job(*job);
            runtime_metrics::record_db_job_processed();
        } catch (const std::exception& ex) {
            runtime_metrics::record_db_job_failed();
            log::error(std::string("DbWorkerPool job exception: ") + ex.what());
        } catch (...) {
            runtime_metrics::record_db_job_failed();
            log::error("DbWorkerPool job exception: unknown error");
        }
    }
}

void DbWorkerPool::process_job(const Job& job) {
    auto unit = pool_->make_unit_of_work();
    if (!unit) {
        throw std::runtime_error("Failed to create unit_of_work from connection pool");
    }

    const bool auto_commit = job.auto_commit;
    // unit_of_work는 commit/rollback을 명시적으로 호출해야 하므로
    // auto_commit 여부에 따라 커서를 정리한다.
    try {
        job.task(*unit);
        if (auto_commit) {
            unit->commit();
        }
    } catch (...) {
        try {
            unit->rollback();
        } catch (...) {
            // ignore rollback failure
        }
        throw;
    }

    if (!auto_commit) {
        try {
            unit->rollback();
        } catch (...) {
            // no-op: caller may already have committed
        }
    }
}

} // namespace server::core::storage
