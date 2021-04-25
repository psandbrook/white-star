#pragma once

#include <loguru.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string.h>
#include <utility>

#define CONCATENATE_2(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_2(s1, s2)

#ifdef __COUNTER__
    #define ANONYMOUS_VARIABLE(prefix) CONCATENATE(prefix, __COUNTER__)
#else
    #define ANONYMOUS_VARIABLE(prefix) CONCATENATE(prefix, __LINE__)
#endif

#ifdef DEBUG
    #define DPRINT(...) DRAW_LOG_F(INFO, __VA_ARGS__)
    #define DEXPR(expr) DPRINT("{}: {}", #expr, (expr))
#else
    #define DPRINT(...)
    #define DEXPR(expr)
#endif

inline void dline() {
    DPRINT(" ");
}

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;
static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

template <class F>
class DeferT {
public:
    explicit DeferT(F f) : f_(std::move(f)) {}

    DeferT(const DeferT&) = delete;
    DeferT& operator=(const DeferT&) = delete;
    DeferT(DeferT&&) = delete;
    DeferT& operator=(DeferT&&) = delete;

    ~DeferT() {
        f_();
    }

private:
    F f_;
};

template <class F>
[[nodiscard]] inline DeferT<F> defer(F&& f) {
    return DeferT<F>(std::forward<F>(f));
}

#define DEFER(f) auto ANONYMOUS_VARIABLE(defer__) = ::defer(f)

inline constexpr f32 pi_f32 = static_cast<f32>(M_PI);
inline constexpr f64 pi_f64 = M_PI;

inline constexpr f64 default_epsilon = 0.00000000000001;

// See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
inline bool float_eq(const f64 a, const f64 b, const f64 epsilon = default_epsilon) {
    if (a < 1.0 && b < 1.0) {
        return std::abs(a - b) <= epsilon;
    } else {
        return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
    }
}

template <class T>
inline void hash_combine(size_t& seed, const T& value) {
    seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class K, class V, class Hash, class Equal>
inline bool has_key(const std::unordered_map<K, V, Hash, Equal>& c, const K& key) {
    return c.find(key) != c.end();
}

inline bool c_str_eq(const char* const lhs, const char* const rhs) {
    return strcmp(lhs, rhs) == 0;
}

struct CStrHash {
    size_t operator()(const char* const str) const {
        size_t result = 0;
        for (const char* c = str; *c != '\0'; ++c) {
            hash_combine(result, *c);
        }
        return result;
    }
};

struct CStrEqual {
    bool operator()(const char* const lhs, const char* const rhs) const {
        return c_str_eq(lhs, rhs);
    }
};
