// test_error_handler.cpp — ErrorHandler unit tests
#include <gtest/gtest.h>
#include "common/error_handler.h"
#include <stdexcept>

using namespace aurora::common;

class ErrorHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ErrorHandler::instance().resetFailures();
    }
};

// ===== Retry with Success =====

TEST_F(ErrorHandlerTest, RetrySucceedsOnFirstAttempt) {
    RetryPolicy policy{.max_retries = 3, .initial_interval_sec = 0.001,
                       .max_interval_sec = 1.0, .multiplier = 2.0, .jitter = false};

    int call_count = 0;
    auto result = ErrorHandler::instance().retry("test_op", policy, [&]() -> int {
        call_count++;
        return 42;
    });

    EXPECT_EQ(result, 42);
    EXPECT_EQ(call_count, 1);
}

TEST_F(ErrorHandlerTest, RetrySucceedsAfterFailures) {
    RetryPolicy policy{.max_retries = 3, .initial_interval_sec = 0.001,
                       .max_interval_sec = 1.0, .multiplier = 2.0, .jitter = false};

    int call_count = 0;
    auto result = ErrorHandler::instance().retry("test_op", policy, [&]() -> int {
        call_count++;
        if (call_count < 3) throw std::runtime_error("temporary error");
        return 99;
    });

    EXPECT_EQ(result, 99);
    EXPECT_EQ(call_count, 3);
}

TEST_F(ErrorHandlerTest, RetryExhaustedThrows) {
    RetryPolicy policy{.max_retries = 2, .initial_interval_sec = 0.001,
                       .max_interval_sec = 1.0, .multiplier = 2.0, .jitter = false};

    EXPECT_THROW({
        ErrorHandler::instance().retry("test_op", policy, [&]() -> void {
            throw std::runtime_error("always fails");
        });
    }, std::runtime_error);
}

// ===== withFallback =====

TEST_F(ErrorHandlerTest, FallbackNotCalledOnSuccess) {
    DegradePolicy policy{.enable_fallback = true, .failures_before_degrade = 1,
                         .recovery_timeout_sec = 300.0};

    bool primary_called = false;
    bool fallback_called = false;

    auto result = ErrorHandler::instance().withFallback("test_op", policy,
        [&]() -> int {
            primary_called = true;
            return 10;
        },
        [&]() -> int {
            fallback_called = true;
            return 20;
        }
    );

    EXPECT_EQ(result, 10);
    EXPECT_TRUE(primary_called);
    EXPECT_FALSE(fallback_called);
}

TEST_F(ErrorHandlerTest, FallbackCalledAfterThreshold) {
    // Reset to get clean state
    ErrorHandler::instance().resetFailures();
    DegradePolicy policy{.enable_fallback = true, .failures_before_degrade = 1,
                         .recovery_timeout_sec = 300.0};

    bool fallback_called = false;

    // First call: primary fails, failure count goes to 1, threshold is 1 → fallback
    auto result = ErrorHandler::instance().withFallback("test_op", policy,
        [&]() -> int {
            throw std::runtime_error("primary fails");
        },
        [&]() -> int {
            fallback_called = true;
            return 20;
        }
    );

    EXPECT_EQ(result, 20);
    EXPECT_TRUE(fallback_called);
}

// ===== validateRange =====

TEST_F(ErrorHandlerTest, ValidateRangeNormal) {
    EXPECT_TRUE(ErrorHandler::validateRange("test", 5.0, 0.0, 10.0));
    EXPECT_TRUE(ErrorHandler::validateRange("test", 0.0, 0.0, 10.0));
    EXPECT_TRUE(ErrorHandler::validateRange("test", 10.0, 0.0, 10.0));
}

TEST_F(ErrorHandlerTest, ValidateRangeOutOfBounds) {
    EXPECT_FALSE(ErrorHandler::validateRange("test", -1.0, 0.0, 10.0));
    EXPECT_FALSE(ErrorHandler::validateRange("test", 11.0, 0.0, 10.0));
}

TEST_F(ErrorHandlerTest, ValidateRangeNaNInf) {
    EXPECT_FALSE(ErrorHandler::validateRange("test", std::nan(""), 0.0, 10.0));
    EXPECT_FALSE(ErrorHandler::validateRange("test", std::numeric_limits<double>::infinity(), 0.0, 10.0));
    EXPECT_FALSE(ErrorHandler::validateRange("test", -std::numeric_limits<double>::infinity(), 0.0, 10.0));
}

// ===== validateNotEmpty =====

TEST_F(ErrorHandlerTest, ValidateNotEmptyNormal) {
    EXPECT_TRUE(ErrorHandler::validateNotEmpty("test", "hello"));
}

TEST_F(ErrorHandlerTest, ValidateNotEmptyEmpty) {
    EXPECT_FALSE(ErrorHandler::validateNotEmpty("test", ""));
}

// ===== diskSafeWrite =====

TEST_F(ErrorHandlerTest, DiskSafeWriteSuccess) {
    bool result = ErrorHandler::instance().diskSafeWrite("test_op", [&]() {
        return true;
    });
    EXPECT_TRUE(result);
}

TEST_F(ErrorHandlerTest, DiskSafeWriteException) {
    bool result = ErrorHandler::instance().diskSafeWrite("test_op", [&]() {
        throw std::runtime_error("disk full");
        return true;
    });
    EXPECT_FALSE(result);
}
