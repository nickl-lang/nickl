#include "pipe_stream.hpp"

#include <new>
#include <sstream>

#include "nk/common/allocator.hpp"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/sys/error.h"
#include "nk/sys/file.h"
#include "nk/sys/process.h"

namespace {

NK_LOG_USE_SCOPE(pipe_stream);

void _makeCmdStr(NkStringBuilder sb, nkstr cmd, bool quiet) {
    nksb_printf(sb, "%.*s", (int)cmd.size, cmd.data);
    if (quiet) {
        nksb_printf(sb, " >/dev/null 2>&1");
    }
    nksb_printf(sb, "%c", '\0');
}

struct FdBufBase : std::stringbuf {
    explicit FdBufBase(nkfd_t fd, nkpid_t pid)
        : m_fd{fd}
        , m_pid{pid} {
    }

    virtual ~FdBufBase() {
    }

    int close() {
        nk_close(m_fd);
        int ret = 1;
        if (m_pid > 0) {
            nk_waitpid(m_pid, &ret);
        }
        return ret;
    }

    nkfd_t m_fd;
    nkpid_t m_pid;
};

struct OutputFdBuf : FdBufBase {
    explicit OutputFdBuf(nkfd_t fd, nkpid_t pid)
        : FdBufBase{fd, pid} {
    }

    int sync() override {
        auto const &data = this->str();
        int n = nk_write(m_fd, data.c_str(), data.size());
        this->str("");
        return n;
    }
};

struct InputFdBuf : FdBufBase {
    static constexpr size_t c_buf_size = 4096;

    explicit InputFdBuf(nkfd_t fd, nkpid_t pid)
        : FdBufBase{fd, pid}
        , m_buf{new char[c_buf_size]} {
    }

    ~InputFdBuf() {
        delete[] m_buf;
    }

    int underflow() override {
        if (this->gptr() == this->egptr()) {
            int size = nk_read(m_fd, m_buf, c_buf_size);
            this->setg(m_buf, m_buf, m_buf + size);
        }
        return this->gptr() == this->egptr() ? std::char_traits<char>::eof()
                                             : std::char_traits<char>::to_int_type(*this->gptr());
    }

    char *m_buf;
};

} // namespace

std::istream nk_pipe_streamRead(nkstr cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder_T sb{};
    defer {
        nksb_free(&sb);
    };
    _makeCmdStr(&sb, cmd, quiet);

    NK_LOG_DBG("exec(\"%.*s\")", (int)sb.size, sb.data);

    nkpipe_t out = nk_createPipe();
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, nullptr, &out) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"%.*s\") failed: %s", (int)sb.size, sb.data, nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, nullptr);
        }
        nk_close(out.read);
        nk_close(out.write);
        return std::istream{nullptr};
    } else {
        return std::istream{new (nk_alloc_t<InputFdBuf>(nk_default_allocator)) InputFdBuf{out.read, pid}};
    }
}

std::ostream nk_pipe_streamWrite(nkstr cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder_T sb{};
    defer {
        nksb_free(&sb);
    };
    _makeCmdStr(&sb, cmd, quiet);

    NK_LOG_DBG("exec(\"%.*s\")", (int)sb.size, sb.data);

    nkpipe_t in = nk_createPipe();
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, &in, nullptr) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"%.*s\") failed: %s", (int)sb.size, sb.data, nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, nullptr);
        }
        nk_close(in.read);
        nk_close(in.write);
        return std::ostream{nullptr};
    }
    return std::ostream{new (nk_alloc_t<OutputFdBuf>(nk_default_allocator)) OutputFdBuf{in.write, pid}};
}

bool nk_pipe_streamClose(std::ios const &stream) {
    NK_LOG_TRC("%s", __func__);

    auto buf = (FdBufBase *)stream.rdbuf();
    int code = 1;
    if (buf) {
        buf->pubsync();
        code = buf->close();
        buf->~FdBufBase();
        nk_free_t(nk_default_allocator, buf);
    }
    return code == 0;
}
