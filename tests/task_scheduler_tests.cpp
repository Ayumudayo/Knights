#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "server/core/concurrent/task_scheduler.hpp"

using namespace std::chrono_literals;
using server::core::concurrent::TaskScheduler;

TEST(TaskSchedulerTests, PostExecutesInOrder) {
    TaskScheduler scheduler;
    std::vector<int> results;

    scheduler.post([&]() { results.push_back(1); });
    scheduler.post([&]() { results.push_back(2); });
    scheduler.post([&]() { results.push_back(3); });

    auto executed = scheduler.poll();

    ASSERT_EQ(executed, 3u);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
}

TEST(TaskSchedulerTests, DelayedTasksExecuteAfterDelay) {
    TaskScheduler scheduler;
    std::atomic<int> counter{0};

    scheduler.schedule([&]() { counter.fetch_add(1); }, 20ms);

    EXPECT_EQ(scheduler.poll(), 0u);
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(scheduler.poll(), 1u);
    EXPECT_EQ(counter.load(), 1);
}

TEST(TaskSchedulerTests, ScheduleEveryRepeatsUntilShutdown) {
    TaskScheduler scheduler;
    std::atomic<int> counter{0};

    scheduler.schedule_every([&]() { counter.fetch_add(1); }, 5ms);

    auto start = TaskScheduler::Clock::now();
    while (TaskScheduler::Clock::now() - start < 80ms) {
        scheduler.poll();
        std::this_thread::sleep_for(5ms);
    }

    EXPECT_GE(counter.load(), 3);

    scheduler.shutdown();
    auto before = counter.load();
    std::this_thread::sleep_for(20ms);
    scheduler.poll();
    EXPECT_EQ(counter.load(), before);
    EXPECT_TRUE(scheduler.empty());
}

TEST(TaskSchedulerTests, PollHonorsMaxTasks) {
    TaskScheduler scheduler;
    std::vector<int> order;

    scheduler.post([&]() { order.push_back(1); });
    scheduler.post([&]() { order.push_back(2); });
    scheduler.post([&]() { order.push_back(3); });

    auto first = scheduler.poll(2);
    EXPECT_EQ(first, 2u);
    EXPECT_EQ(order.size(), 2u);

    auto second = scheduler.poll(1);
    EXPECT_EQ(second, 1u);
    EXPECT_EQ(order.size(), 3u);
}

TEST(TaskSchedulerTests, ShutdownClearsPendingTasks) {
    TaskScheduler scheduler;
    std::atomic<int> counter{0};

    scheduler.post([&]() { counter.fetch_add(1); });
    scheduler.schedule([&]() { counter.fetch_add(1); }, 1ms);

    scheduler.shutdown();

    EXPECT_TRUE(scheduler.empty());
    EXPECT_EQ(scheduler.poll(), 0u);
    EXPECT_EQ(counter.load(), 0);
}
