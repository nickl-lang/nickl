#include "nk/common/logger.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <streambuf>
#include <thread>

#include <unistd.h>

#include <gtest/gtest.h>

NK_LOG_USE_SCOPE(test);

static constexpr std::size_t c_buffer_size = 4096;

class logger : public testing::Test {
    void SetUp() override {
        // Assign test msg buffer to STDERR
        setvbuf(stderr, m_msg_buffer, _IOFBF, c_buffer_size);

        NK_LOGGER_INIT(NkLoggerOptions{NkLog_Trace, NkLog_Color_Auto});
    }

    void TearDown() override {
        // Reset STDERR buffer
        setvbuf(stderr, nullptr, _IONBF, c_buffer_size);
    }

protected:
    void writeTestMsg(NkLogLevel level, char const *msg) {
        // Flush any existing data
        std::fflush(stderr);
        std::fflush(stdout);

        // Clear the test msg buffer
        std::memset(m_msg_buffer, 0, c_buffer_size);

        // Save original streams
        m_stderr_save = dup(STDERR_FILENO);
        m_stdout_save = dup(STDOUT_FILENO);

        // Redirect stdout to disable colored output
        FILE *dummy;
        dummy = std::freopen("/dev/null", "a", stdout);
        (void)dummy;

        _NK_LOG_CHK(level, msg);

        // Flush any test msg data
        std::fflush(stderr);
        std::fflush(stdout);

        // Restore original streams
        dup2(m_stderr_save, STDERR_FILENO);
        dup2(m_stdout_save, STDOUT_FILENO);
    }

    struct LogMsg {
        std::string index;
        std::string timestamp;
        std::string level;
        std::string scope;
        std::string text;
    };

    LogMsg parseLogMsg() {
        std::istringstream ss{m_msg_buffer};

        LogMsg msg{};

        ss >> msg.index;
        ss >> msg.timestamp;
        ss >> msg.level;
        ss >> msg.scope;

        msg.text = std::string{std::istreambuf_iterator<char>{ss}, std::istreambuf_iterator<char>{}};

        return msg;
    }

protected: // fields
    char m_msg_buffer[c_buffer_size];

    int m_stderr_save;
    int m_stdout_save;
};

#define EXPECT_LOG_MSG(IDX, LEV, SCOPE, TEXT) \
    {                                         \
        auto const msg = parseLogMsg();       \
        EXPECT_EQ(msg.index, (IDX));          \
        EXPECT_EQ(msg.level, (LEV));          \
        EXPECT_EQ(msg.scope, (SCOPE));        \
        EXPECT_EQ(msg.text, (TEXT));          \
    }

TEST_F(logger, write) {
    writeTestMsg(NkLog_Error, "This is error log");
    EXPECT_LOG_MSG("0001", "error", "test", " This is error log\n");

    writeTestMsg(NkLog_Warning, "This is warning log");
    EXPECT_LOG_MSG("0002", "warning", "test", " This is warning log\n");

    writeTestMsg(NkLog_Info, "This is info log");
    EXPECT_LOG_MSG("0003", "info", "test", " This is info log\n");

    writeTestMsg(NkLog_Debug, "This is debug log");
    EXPECT_LOG_MSG("0004", "debug", "test", " This is debug log\n");

    writeTestMsg(NkLog_Trace, "This is trace log");
    EXPECT_LOG_MSG("0005", "trace", "test", " This is trace log\n");
}

TEST_F(logger, complex) {
    NK_LOG_INF("float=%lf, int=%i", 3.14, 42);
}

TEST_F(logger, threads) {
    bool stop = false;
    std::thread t1{[&]() {
        while (!stop) {
            NK_LOG_TRC("This is thread 1!");
        }
    }};
    std::thread t2{[&]() {
        while (!stop) {
            NK_LOG_DBG("This is thread 2!");
        }
    }};
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
    stop = true;
    t1.join();
    t2.join();
}
