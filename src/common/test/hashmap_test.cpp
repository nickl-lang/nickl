#include "nk/ds/hashmap.hpp"

#include <random>
#include <unordered_map>

#include <gtest/gtest.h>

#include "nk/str/string.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

using namespace nk;

class hashmap : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

LOG_USE_SCOPE(nk::test);

template <class K, class V>
struct DoubleMap {
    HashMap<K, V> hm{};
    std::unordered_map<K, V> std_map;

    ~DoubleMap() {
        hm.deinit();
    }

    void insert(K const &key, V const &value) {
        hm.insert(key, value);
        std_map[key] = value;
    }

    void remove(K const &key) {
        hm.remove(key);
        std_map.erase(key);
    }
};

#define DOUBLE_MAP_VERIFY(MAP)                                            \
    EXPECT_EQ((MAP).hm.size, (MAP).std_map.size());                       \
    for (auto const &it : (MAP).std_map) {                                \
        auto found = (MAP).hm.find(it.first);                             \
        ASSERT_TRUE(found) << "Key not found: " << it.first;              \
        EXPECT_EQ(*found, it.second) << "Value not equal: " << it.second; \
    }

#define DOUBLE_MAP_INSERT(MAP, KEY, VALUE) \
    (MAP).insert((KEY), (VALUE));          \
    DOUBLE_MAP_VERIFY(MAP);

#define DOUBLE_MAP_REMOVE(MAP, KEY) \
    (MAP).remove((KEY));            \
    DOUBLE_MAP_VERIFY(MAP);

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

TEST_F(hashmap, insert_remove_example) {
    DoubleMap<int, int> map;

    DOUBLE_MAP_INSERT(map, 1, 0);
    DOUBLE_MAP_REMOVE(map, 1);
    DOUBLE_MAP_INSERT(map, 1, 0);
    DOUBLE_MAP_REMOVE(map, 1);

    DOUBLE_MAP_INSERT(map, 1, 0);
    DOUBLE_MAP_INSERT(map, 3, 0);
}

TEST_F(hashmap, insert_remove_loop) {
    DoubleMap<int, int> map;

    DOUBLE_MAP_INSERT(map, 0, 42);
    for (size_t i = 1; i <= 100; i++) {
        DOUBLE_MAP_INSERT(map, i, 24);
        DOUBLE_MAP_REMOVE(map, i);
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
            LOG_INF("add: key=%lu, val=%lu", key, val);
            std_map[key] = val;
            hm.insert(key, val);
        } else {
            auto it = std::next(std::begin(std_map), gen() % std_map.size());
            key_t key = it->first;
            LOG_INF("del: key=%lu", key);
            std_map.erase(it);
            hm.remove(key);
        }

        ASSERT_EQ(hm.size, std_map.size());

        for (auto const &it : std_map) {
            auto found = hm.find(it.first);
            ASSERT_TRUE(found) << "Key not found: " << it.first;
            ASSERT_EQ(*found, it.second) << "Value not equal: " << it.second;
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
    auto found = hm.find(cs2s("val"));
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, 42);
}

TEST_F(hashmap, index_operator) {
    using key_t = int;
    using val_t = string;
    using hashmap_t = HashMap<key_t, val_t>;

    hashmap_t hm{};
    defer {
        hm.deinit();
    };

    val_t *found = nullptr;

    hm[1] = cs2s("one");

    EXPECT_EQ(hm.size, 1);

    found = hm.find(1);
    ASSERT_TRUE(found);
    EXPECT_EQ(std_str(*found), "one");

    hm[42];

    EXPECT_EQ(hm.size, 2);

    found = hm.find(42);
    ASSERT_TRUE(found);
    EXPECT_EQ(std_str(*found), "");

    hm[42] = cs2s("forty-two");

    EXPECT_EQ(hm.size, 2);

    found = hm.find(42);
    ASSERT_TRUE(found);
    EXPECT_EQ(std_str(*found), "forty-two");
}
