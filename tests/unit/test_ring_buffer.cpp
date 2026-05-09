// test_ring_buffer.cpp — LockFreeRingBuffer unit tests
#include <gtest/gtest.h>
#include "common/performance_utils.h"
#include <thread>
#include <vector>

using namespace aurora::performance;

class RingBufferTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

// ===== Basic Operations =====

TEST_F(RingBufferTest, PushAndPop) {
    LockFreeRingBuffer<int, 8> buf;
    EXPECT_TRUE(buf.push(42));
    int val = 0;
    EXPECT_TRUE(buf.pop(val));
    EXPECT_EQ(val, 42);
}

TEST_F(RingBufferTest, EmptyBuffer) {
    LockFreeRingBuffer<int, 8> buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);

    int val = 0;
    EXPECT_FALSE(buf.pop(val));
}

TEST_F(RingBufferTest, SizeTracking) {
    LockFreeRingBuffer<int, 16> buf;
    EXPECT_EQ(buf.size(), 0u);

    buf.push(1);
    buf.push(2);
    buf.push(3);
    EXPECT_EQ(buf.size(), 3u);

    int val;
    buf.pop(val);
    EXPECT_EQ(buf.size(), 2u);
}

// ===== Capacity and Overflow =====

TEST_F(RingBufferTest, FullBufferRejectsPush) {
    LockFreeRingBuffer<int, 4> buf;  // capacity 4 means max 3 usable (one slot wasted for ring)
    EXPECT_TRUE(buf.push(1));
    EXPECT_TRUE(buf.push(2));
    EXPECT_TRUE(buf.push(3));
    EXPECT_FALSE(buf.push(4));  // buffer full
}

TEST_F(RingBufferTest, FIFOOrder) {
    LockFreeRingBuffer<int, 16> buf;
    for (int i = 0; i < 5; ++i) {
        buf.push(i);
    }

    for (int i = 0; i < 5; ++i) {
        int val;
        EXPECT_TRUE(buf.pop(val));
        EXPECT_EQ(val, i);
    }
}

// ===== Wrap-around =====

TEST_F(RingBufferTest, WrapAround) {
    LockFreeRingBuffer<int, 4> buf;

    // Fill and drain multiple times to test wrap-around
    for (int cycle = 0; cycle < 10; ++cycle) {
        EXPECT_TRUE(buf.push(cycle * 10));
        EXPECT_TRUE(buf.push(cycle * 10 + 1));

        int val;
        EXPECT_TRUE(buf.pop(val));
        EXPECT_EQ(val, cycle * 10);
        EXPECT_TRUE(buf.pop(val));
        EXPECT_EQ(val, cycle * 10 + 1);
    }
}

// ===== Concurrent Access =====

TEST_F(RingBufferTest, ConcurrentProducerConsumer) {
    LockFreeRingBuffer<int, 1024> buf;
    const int NUM_ITEMS = 500;
    std::vector<int> consumed;
    std::atomic<int> consumed_count{0};

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buf.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        for (int i = 0; i < NUM_ITEMS; ) {
            int val;
            if (buf.pop(val)) {
                consumed.push_back(val);
                consumed_count++;
                i++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed.size(), static_cast<size_t>(NUM_ITEMS));
    // Verify all values present (order may vary due to timing)
    std::sort(consumed.begin(), consumed.end());
    for (int i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(consumed[i], i);
    }
}

// ===== Floating Point Types =====

TEST_F(RingBufferTest, DoubleValues) {
    LockFreeRingBuffer<double, 16> buf;
    EXPECT_TRUE(buf.push(3.14159));

    double val;
    EXPECT_TRUE(buf.pop(val));
    EXPECT_NEAR(val, 3.14159, 1e-10);
}

// ===== Struct Types =====

struct TestPoint {
    float x, y;
    bool operator==(const TestPoint& other) const {
        return x == other.x && y == other.y;
    }
};

TEST_F(RingBufferTest, StructType) {
    LockFreeRingBuffer<TestPoint, 8> buf;
    TestPoint p{1.0f, 2.0f};
    EXPECT_TRUE(buf.push(p));

    TestPoint out;
    EXPECT_TRUE(buf.pop(out));
    EXPECT_EQ(out.x, 1.0f);
    EXPECT_EQ(out.y, 2.0f);
}
