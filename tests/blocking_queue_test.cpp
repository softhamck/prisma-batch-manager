#include "prisma/blocking_queue.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <thread>

using prisma::BlockingQueue;

TEST(BlockingQueueTest, DeliversItemsInFifoOrder) {
    BlockingQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
    EXPECT_EQ(queue.pop(), 3);
}

TEST(BlockingQueueTest, ConsumerReceivesItemFromAnotherThread) {
    BlockingQueue<int> queue;
    std::optional<int> received;

    std::thread consumer([&] { received = queue.pop(); });
    queue.push(42);
    consumer.join();

    EXPECT_EQ(received, 42);
}

// If close() did not wake the consumer, this test would hang.
TEST(BlockingQueueTest, CloseWakesUpAWaitingConsumer) {
    BlockingQueue<int> queue;
    std::optional<int> received{-1};

    std::thread consumer([&] { received = queue.pop(); });
    queue.close();
    consumer.join();

    EXPECT_FALSE(received.has_value());
}

TEST(BlockingQueueTest, CloseStillDeliversPendingItems) {
    BlockingQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.close();

    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(BlockingQueueTest, PushAfterCloseIsRejected) {
    BlockingQueue<int> queue;
    queue.close();

    EXPECT_FALSE(queue.push(1));
    EXPECT_FALSE(queue.pop().has_value());
}
