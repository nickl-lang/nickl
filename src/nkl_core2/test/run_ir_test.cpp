#include <sstream>

#include <gtest/gtest.h>

#include "nkl/core/nickl.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class nkl_run_ir : public testing::Test {
protected:
    void SetUp() override {
        NK_LOG_INIT({
            .log_level = NkLogLevel_Warning,
            .color_mode = NkLogColorMode_Auto,
        });

        nkl = nkl_newState();

        com = nkl_newCompilerForHost(nkl);

        nkl_addLibraryAlias(com, nk_cs2s("c"), nk_cs2s(SYSTEM_LIBC));
        nkl_addLibraryAlias(com, nk_cs2s("m"), nk_cs2s(SYSTEM_LIBM));
        nkl_addLibraryAlias(com, nk_cs2s("pthread"), nk_cs2s(SYSTEM_LIBPTHREAD));
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
};

#define COMPILE(...)                                             \
    do {                                                         \
        bool const ok = nkl_compileStringIr(__VA_ARGS__);        \
        EXPECT_TRUE(ok && !nkl_getErrors(nkl)) << printErrors(); \
    } while (0)

TEST_F(nkl_run_ir, empty) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
)"));
}

TEST_F(nkl_run_ir, basic) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
pub proc _entry() {
    ret
}
)"));

    auto entry = (void (*)(void))nkl_getSymbolAddress(mod, nk_cs2s("_entry"));
    ASSERT_TRUE(entry);
    entry();
}

TEST_F(nkl_run_ir, plus) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
pub proc plus(:i64 %a, :i64 %b) :i64 {
    add %a, %b -> %ret
    ret %ret
}
)"));

    auto plus = (i64 (*)(i64, i64))nkl_getSymbolAddress(mod, nk_cs2s("plus"));
    ASSERT_TRUE(plus);
    EXPECT_EQ(plus(4, 5), 9);
}

TEST_F(nkl_run_ir, resolve_with_existing_dependency) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
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

    auto bar = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("bar"));
    ASSERT_TRUE(bar);
    bar();

    auto foo = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("foo"));
    ASSERT_TRUE(foo);
    foo();
}

TEST_F(nkl_run_ir, independent_modules) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
pub proc foo() :i64 {
    ret 42
}
)"));

    auto mod2 = nkl_newModule(com);

    COMPILE(mod2, nk_cs2s(R"(
pub proc foo() :i64 {
    ret 43
}
)"));

    {
        auto foo = (i64 (*)())nkl_getSymbolAddress(mod, nk_cs2s("foo"));
        ASSERT_TRUE(foo);
        EXPECT_EQ(foo(), 42);
    }

    {
        auto foo = (i64 (*)())nkl_getSymbolAddress(mod2, nk_cs2s("foo"));
        ASSERT_TRUE(foo);
        EXPECT_EQ(foo(), 43);
    }
}

TEST_F(nkl_run_ir, define_then_link) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
extern "c" proc puts() :i32
extern proc bar() :void

pub proc foo() {
    call puts, ("foo") -> :i32
    call bar, ()
    ret
}
)"));

    auto mod2 = nkl_newModule(com);

    COMPILE(mod2, nk_cs2s(R"(
extern "c" proc puts() :i32

pub proc bar() {
    call puts, ("bar") -> :i32
    ret
}
)"));

    nkl_linkModule(mod, mod2);

    auto foo = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("foo"));
    ASSERT_TRUE(foo);
    foo();
}

TEST_F(nkl_run_ir, link_then_define) {
    auto mod = nkl_newModule(com);

    COMPILE(mod, nk_cs2s(R"(
extern "c" proc puts() :i32
extern proc bar() :void

pub proc foo() {
    call puts, ("foo") -> :i32
    call bar, ()
    ret
}
)"));

    auto mod2 = nkl_newModule(com);

    nkl_linkModule(mod, mod2);

    COMPILE(mod2, nk_cs2s(R"(
extern "c" proc puts() :i32

pub proc bar() {
    call puts, ("bar") -> :i32
    ret
}
)"));

    auto foo = (void (*)())nkl_getSymbolAddress(mod, nk_cs2s("foo"));
    ASSERT_TRUE(foo);
    foo();
}

TEST_F(nkl_run_ir, override_libc) {
    auto mod = nkl_newModule(com);

    auto libc = nkl_newModuleNamed(com, nk_cs2s("c"));
    nkl_linkModule(mod, libc);

    COMPILE(libc, nk_cs2s(R"(
extern "c" proc printf(:i64, ...) :i32

pub proc puts(:i64 %s) :i32 {
    call printf, ("[my_puts] %s\n", ..., :i64 %s) -> :i32
    ret 42
}
)"));

    COMPILE(mod, nk_cs2s(R"(
extern "c" proc puts() :i32

pub proc hello() :i32 {
    call puts, ("hello") -> :i32 %ret
    ret %ret
}
)"));

    auto hello = (i32 (*)())nkl_getSymbolAddress(mod, nk_cs2s("hello"));
    ASSERT_TRUE(hello);
    EXPECT_EQ(hello(), 42);
}

TEST_F(nkl_run_ir, mutual_link) {
    auto mod0 = nkl_newModuleNamed(com, nk_cs2s("mod0"));
    auto mod1 = nkl_newModuleNamed(com, nk_cs2s("mod1"));

    nkl_linkModule(mod0, mod1);
    nkl_linkModule(mod1, mod0);

    COMPILE(mod0, nk_cs2s(R"(
extern "c" proc printf(:i64, ...) :i32
extern proc bar() :i32

pub proc foo() :i32 {
    call printf, ("foo\n") -> :i32 %a
    call bar, () -> :i32 %b
    add :i32 %a, %b -> %c
    ret %c
}

pub proc baz() :i32 {
    call printf, ("baz\n") -> :i32 %a
    ret %a
}
)"));

    COMPILE(mod1, nk_cs2s(R"(
extern "c" proc printf(:i64, ...) :i32
extern proc baz() :i32

pub proc bar() :i32 {
    call printf, ("bar\n") -> :i32 %a
    call baz, () -> :i32 %b
    add :i32 %a, %b -> %c
    ret %c
}
)"));

    auto foo = (i32 (*)())nkl_getSymbolAddress(mod0, nk_cs2s("foo"));
    ASSERT_TRUE(foo);
    EXPECT_EQ(foo(), 12);
}

// TEST_F(nkl_run_ir, link_cycle) {
//     auto mod0 = nkl_newModuleNamed(com, nk_cs2s("mod0"));
//     auto mod1 = nkl_newModuleNamed(com, nk_cs2s("mod1"));

//     nkl_linkModule(mod0, mod1);
//     nkl_linkModule(mod1, mod0);

//     COMPILE(mod0, nk_cs2s(R"(
// extern "c" proc puts() :i32
// extern proc bar() :void

// pub proc foo() {
//     call puts, ("foo") -> :i32
//     call bar, ()
//     ret
// }
// )"));

//     COMPILE(mod1, nk_cs2s(R"(
// extern "c" proc puts() :i32
// extern proc foo() :void

// pub proc bar() {
//     call puts, ("bar") -> :i32
//     call foo, ()
//     ret
// }
// )"));

//     auto foo = (void (*)())nkl_getSymbolAddress(mod0, nk_cs2s("foo"));
//     ASSERT_TRUE(foo);
//     foo();
// }
