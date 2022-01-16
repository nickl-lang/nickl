#include "nk/common/logger.hpp"

#include <thread>

#include <gtest/gtest.h>

LOG_USE_SCOPE(test)

class logger : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(logger, basic) {
    ;
    LOG_INF("This is info log")
    LOG_DBG("This is debug log")
    LOG_TRC("This is trace log")
}

TEST_F(logger, complex) {
    ;
    LOG_INF("float=%lf, int=%i", 3.14, 42)
}

TEST_F(logger, theads) {
    bool stop = false;
    std::thread t1{[&]() {
        while (!stop) {
            LOG_TRC("This is thread 1!")
        }
    }};
    std::thread t2{[&]() {
        while (!stop) {
            LOG_DBG("This is thread 2!")
        }
    }};
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    stop = true;
    t1.join();
    t2.join();
}
