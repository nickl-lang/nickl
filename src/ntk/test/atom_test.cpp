#include "ntk/atom.h"

#include <gtest/gtest.h>

#include "ntk/log.h"
#include "ntk/string.h"

class atom : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(atom, forward) {
    static constexpr char const *c_str_a = "String A.";
    static constexpr char const *c_str_b = "String B.";

    NkAtom const atom_a = nk_cs2atom(c_str_a);
    NkAtom const atom_b = nk_cs2atom(c_str_b);

    EXPECT_EQ(atom_a, nk_cs2atom(c_str_a));
    EXPECT_EQ(atom_b, nk_cs2atom(c_str_b));

    EXPECT_NE(atom_a, atom_b);
}

TEST_F(atom, backward) {
    static constexpr char const *c_str_a = "This is a first string.";
    static constexpr char const *c_str_b = "This is a second string.";

    NkAtom const atom_a = nk_cs2atom(c_str_a);
    NkAtom const atom_b = nk_cs2atom(c_str_b);

    EXPECT_EQ(c_str_a, nk_s2stdView(nk_atom2s(atom_a)));
    EXPECT_EQ(c_str_b, nk_s2stdView(nk_atom2s(atom_b)));
}

TEST_F(atom, nonexistent) {
    auto str = nk_atom2s(999999999);
    EXPECT_EQ(nullptr, str.data);
    EXPECT_EQ(0u, str.size);
}

TEST_F(atom, unique) {
    static constexpr char const *c_str_hello = "hello";

    NkAtom const hello = nk_cs2atom(c_str_hello);
    NkAtom const hello2 = nk_cs2atom(c_str_hello);

    NkAtom const hello_unique = nk_atom_unique(nk_cs2s(c_str_hello));

    EXPECT_EQ(hello, hello2);
    EXPECT_NE(hello, hello_unique);

    EXPECT_EQ(c_str_hello, nk_s2stdView(nk_atom2s(hello_unique)));
}
