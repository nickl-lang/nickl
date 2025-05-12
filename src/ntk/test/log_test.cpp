#include "ntk/log.h"

#include <atomic>
#include <cstring>
#include <sstream>
#include <thread>

#include <gtest/gtest.h>

#include "ntk/string.h"

NK_LOG_USE_SCOPE(test);

#define MSG_BUF_SIZE 4096

struct StreamBuf {
    char data[MSG_BUF_SIZE];
    usize size;
};

static i32 testStreamProc(void *stream_data, char *buf, usize size, NkStreamMode mode) {
    if (mode == NkStreamMode_Write) {
        std::cerr << std::string_view{buf, size};

        auto const stream_buf = (StreamBuf *)stream_data;

        usize const cpy_count = std::min(size, MSG_BUF_SIZE - stream_buf->size);
        memcpy(stream_buf->data + stream_buf->size, buf, cpy_count);
        stream_buf->size += cpy_count;

        return size;
    } else {
        return -1;
    }
}

class log : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT(NkLogOptions{NkLogLevel_Trace, NkLogColorMode_Never});
        nk_log_injectStream({&m_buf, testStreamProc});
    }

    void TearDown() override {
    }

protected:
    void writeTestMsg(NkLogLevel level, char const *msg) {
        m_buf.size = 0;
        NK_LOG_SEV(level, "%s", msg);
    }

    struct LogMsg {
        std::string index;
        std::string timestamp;
        std::string level;
        std::string scope;
        std::string text;
    };

    LogMsg parseLogMsg() {
        std::istringstream ss{nk_s2stdStr(NkString{m_buf.data, m_buf.size})};

        LogMsg msg{};

        ss >> msg.index;
        ss >> msg.timestamp;
        ss >> msg.level;
        ss >> msg.scope;

        msg.text = std::string{std::istreambuf_iterator<char>{ss}, std::istreambuf_iterator<char>{}};

        return msg;
    }

protected:
    StreamBuf m_buf{};
};

#define EXPECT_LOG_MSG(IDX, LEV, SCOPE, TEXT) \
    {                                         \
        auto const msg = parseLogMsg();       \
        EXPECT_EQ(msg.index, (IDX));          \
        EXPECT_EQ(msg.level, (LEV));          \
        EXPECT_EQ(msg.scope, (SCOPE));        \
        EXPECT_EQ(msg.text, (TEXT));          \
    }

TEST_F(log, write) {
    writeTestMsg(NkLogLevel_Error, "This is error log");
    EXPECT_LOG_MSG("0001", "error", "test", " This is error log\n");

    writeTestMsg(NkLogLevel_Warning, "This is warning log");
    EXPECT_LOG_MSG("0002", "warning", "test", " This is warning log\n");

    writeTestMsg(NkLogLevel_Info, "This is info log");
    EXPECT_LOG_MSG("0003", "info", "test", " This is info log\n");

    writeTestMsg(NkLogLevel_Debug, "This is debug log");
    EXPECT_LOG_MSG("0004", "debug", "test", " This is debug log\n");

    writeTestMsg(NkLogLevel_Trace, "This is trace log");
    EXPECT_LOG_MSG("0005", "trace", "test", " This is trace log\n");
}

TEST_F(log, complex) {
    NK_LOG_INF("f32=%lf, int=%i", 3.14, 42);
}

TEST_F(log, threads) {
    std::atomic_bool stop = false;
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
