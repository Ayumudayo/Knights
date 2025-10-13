#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include "server/core/concurrent/job_queue.hpp"
#include "server/core/concurrent/thread_manager.hpp"

using namespace std::chrono_literals;
using server::core::Job;
using server::core::JobQueue;
using server::core::ThreadManager;

TEST(JobQueueTests, PopBlocksUntilJobArrives) {
    JobQueue queue;
    std::atomic<int> counter{0};
    std::promise<void> ready;

    std::thread worker([&]() {
        ready.set_value();
        Job job = queue.Pop();
        ASSERT_TRUE(job);
        job();
    });

    ready.get_future().wait();
    queue.Push([&]() { counter.fetch_add(1); });
    worker.join();

    EXPECT_EQ(counter.load(), 1);
}

TEST(JobQueueTests, StopWakesWaitingConsumer) {
    JobQueue queue;
    std::promise<void> ready;
    std::atomic<bool> woke{false};

    std::thread waiter([&]() {
        ready.set_value();
        Job job = queue.Pop();
        EXPECT_FALSE(job);
        woke.store(true);
    });

    ready.get_future().wait();
    std::this_thread::sleep_for(5ms);
    queue.Stop();

    waiter.join();
    EXPECT_TRUE(woke.load());
}

TEST(ThreadManagerTests, ExecutesQueuedJobs) {
    JobQueue queue;
    ThreadManager manager(queue);

    manager.Start(2);
    std::atomic<int> completed{0};
    for (int i = 0; i < 6; ++i) {
        queue.Push([&completed]() { completed.fetch_add(1); });
    }

    for (int i = 0; i < 100 && completed.load() < 6; ++i) {
        std::this_thread::sleep_for(5ms);
    }

    manager.Stop();
    EXPECT_EQ(completed.load(), 6);
}

TEST(ThreadManagerTests, StopIsIdempotent) {
    JobQueue queue;
    ThreadManager manager(queue);

    manager.Start(1);
    queue.Push([]() {});

    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(1ms);
    }

    manager.Stop();
    manager.Stop();
    SUCCEED();
}
