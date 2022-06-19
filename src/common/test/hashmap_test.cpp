#include "nk/common/hashmap.hpp"

#include <random>
#include <unordered_map>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/utils.hpp"

class hashmap : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

LOG_USE_SCOPE(test);

TEST_F(hashmap, basic) {
    using hashmap_t = HashMap<uint8_t, uint8_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_EQ(hm.size, 0);
}

TEST_F(hashmap, insert) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_EQ(hm.size, 0);

    EXPECT_TRUE(hm.insert(cs2s("one"), 1));
    EXPECT_TRUE(hm.insert(cs2s("two"), 2));
    EXPECT_TRUE(hm.insert(cs2s("three"), 3));

    EXPECT_EQ(hm.size, 3);
}

TEST_F(hashmap, find) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_EQ(hm.size, 0);

    EXPECT_TRUE(hm.insert(cs2s("one"), 1));
    EXPECT_TRUE(hm.insert(cs2s("two"), 2));
    EXPECT_TRUE(hm.insert(cs2s("three"), 3));

    EXPECT_EQ(hm.size, 3);

    EXPECT_EQ(hm.find(cs2s("four")), nullptr);

    EXPECT_EQ(*hm.find(cs2s("one")), 1);
    EXPECT_EQ(*hm.find(cs2s("two")), 2);
    EXPECT_EQ(*hm.find(cs2s("three")), 3);
}

TEST_F(hashmap, remove) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_TRUE(hm.insert(cs2s("value"), 42));

    EXPECT_EQ(*hm.find(cs2s("value")), 42);

    hm.remove(cs2s("value"));
    EXPECT_EQ(hm.find(cs2s("value")), nullptr);
}

TEST_F(hashmap, overwrite) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_TRUE(hm.insert(cs2s("value"), 0));
    EXPECT_EQ(*hm.find(cs2s("value")), 0);

    EXPECT_EQ(hm.size, 1);

    EXPECT_FALSE(hm.insert(cs2s("value"), 42));
    EXPECT_EQ(*hm.find(cs2s("value")), 42);

    EXPECT_EQ(hm.size, 1);
}

TEST_F(hashmap, ptr_key) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_TRUE(hm.insert(cs2s("________ whatever"), 42));
    EXPECT_TRUE(hm.insert(cs2s("________ something else"), 0xDEADBEEF));

    val_t *pval = hm.find(cs2s("________ whatever"));
    ASSERT_TRUE(pval);
    EXPECT_EQ(*pval, 42);
}

TEST_F(hashmap, str_map) {
    static constexpr size_t c_key_size = 16;
    static constexpr size_t c_test_size = 100;

    using key_t = struct { uint8_t data[c_key_size]; };
    using val_t = uint64_t;

    struct HashMapContext {
        static hash_t hash(key_t const &key) {
            return hash_array(key.data, key.data + c_key_size);
        }

        static hash_t equal_to(key_t const &lhs, key_t const &rhs) {
            return std::memcmp(lhs.data, rhs.data, c_key_size) == 0;
        }
    };

    using hashmap_t = HashMap<key_t, val_t, HashMapContext>;

    std::random_device rd;
    std::mt19937_64 gen{rd()};

    std::unordered_map<std::string, val_t> stdmap;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    for (size_t i = 0; i < c_test_size; i++) {
        key_t key;

        size_t const partition = gen() % c_key_size;
        size_t j = 0;
        for (; j < partition; j++) {
            key.data[j] = '1';
        }
        for (; j < c_key_size; j++) {
            key.data[j] = '0';
        }

        std::string stdkey{key.data, key.data + c_key_size};

        val_t val = gen();

        val_t *found = hm.find(key);
        auto it = stdmap.find(stdkey);

        EXPECT_EQ((bool)found, it != stdmap.end());

        if (found) {
            EXPECT_EQ(*found, it->second);
        } else {
            stdmap[stdkey] = val;
            EXPECT_TRUE(hm.insert(key, val));
        }
    }
}

TEST_F(hashmap, stress) {
    using key_t = uint64_t;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    static constexpr size_t c_test_size = 1024;
    static constexpr size_t c_max_cap = 128;

    std::random_device rd;
    std::mt19937_64 gen{rd()};

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    hm.reserve(gen() % c_max_cap);

    std::unordered_map<key_t, val_t> std_map;

    for (size_t i = 0; i < c_test_size; i++) {
        if (gen() % 2 || std_map.empty()) {
            key_t key = gen() % 100;
            val_t val = gen();
            LOG_DBG("add: key=%lu, val=%lu", key, val);
            std_map[key] = val;
            hm.insert(key, val);
        } else {
            auto it = std::next(std::begin(std_map), gen() % std_map.size());
            key_t key = it->first;
            LOG_DBG("del: key=%lu", key);
            std_map.erase(it);
            hm.remove(key);
        }

        ASSERT_EQ(hm.size, std_map.size());

        for (auto const &it : std_map) {
            auto found = hm.find(it.first);
            ASSERT_TRUE(found);
            ASSERT_EQ(*found, it.second);
        }
    }
}

TEST_F(hashmap, zero_init) {
    using key_t = string;
    using val_t = uint64_t;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    EXPECT_EQ(hm.size, 0);
    EXPECT_EQ(hm.find(cs2s("val")), nullptr);

    EXPECT_TRUE(hm.insert(cs2s("val"), 42));

    EXPECT_EQ(hm.size, 1);
    EXPECT_EQ(*hm.find(cs2s("val")), 42);
}
