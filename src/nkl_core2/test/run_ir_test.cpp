#include <sstream>

#include <gtest/gtest.h>

#include "nkl/core/nickl.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class nkl_run_ir : public testing::Test {
protected:
    void SetUp() override {
        NK_LOG_INIT({});

        nkl = nkl_newState();

        com = nkl_newCompilerHost(nkl);

        nkl_addLibraryAlias(com, nk_cs2s("c"), nk_cs2s(SYSTEM_LIBC));
        nkl_addLibraryAlias(com, nk_cs2s("m"), nk_cs2s(SYSTEM_LIBM));
        nkl_addLibraryAlias(com, nk_cs2s("pthread"), nk_cs2s(SYSTEM_LIBPTHREAD));

        mod = nkl_newModule(com);
    }

    void TearDown() override {
        defer {
            nkl_freeState(nkl);
        };
    }

    std::string printErrors() {
        std::ostringstream ss;
        NklError const *err = nkl_getErrors(nkl);
        while (err) {
            ss << "ERROR:" << err->loc.lin << ":" << err->loc.col << ": " << err->msg;
            err = err->next;
            if (err) {
                ss << "\n";
            }
        }
        return ss.str();
    }

    NklState nkl;
    NklCompiler com;
    NklModule mod;
};

#define EXPECT_NO_ERROR(status) EXPECT_TRUE((status) && !nkl_getErrors(nkl)) << printErrors()

TEST_F(nkl_run_ir, empty) {
    bool ok = nkl_compileStringIr(mod, nk_cs2s(R"(
)"));

    EXPECT_NO_ERROR(ok);
}

TEST_F(nkl_run_ir, basic) {
    bool ok = nkl_compileStringIr(mod, nk_cs2s(R"(
pub proc _entry() {
    ret
}
)"));

    EXPECT_NO_ERROR(ok);

    auto entry = (void (*)(void))nkl_getSymbolAddress(mod, nk_cs2s("_entry"));
    ASSERT_TRUE(entry);
    entry();
}

TEST_F(nkl_run_ir, plus) {
    bool ok = nkl_compileStringIr(mod, nk_cs2s(R"(
pub proc plus(:i64 %a, :i64 %b) :i64 {
    add %a, %b -> %ret
    ret %ret
}
)"));

    EXPECT_NO_ERROR(ok);

    auto plus = (i64 (*)(i64, i64))nkl_getSymbolAddress(mod, nk_cs2s("plus"));
    ASSERT_TRUE(plus);
    EXPECT_EQ(plus(4, 5), 9);
}

TEST_F(nkl_run_ir, consecutive_resolve) {
    bool ok = nkl_compileStringIr(mod, nk_cs2s(R"(
extern "c" proc puts() :i32

pub proc bar() {
    call puts, ("bar") -> :i32
    ret
}

pub proc foo() {
    call puts, ("foo") -> :i32
    call bar, ()
    ret
}
)"));

    EXPECT_NO_ERROR(ok);

    auto bar = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("bar"));
    ASSERT_TRUE(bar);
    bar();

    auto foo = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("foo"));
    ASSERT_TRUE(foo);
    foo();
}
