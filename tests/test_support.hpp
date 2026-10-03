// A test harness small enough to have no dependencies.
//
// The repository has no package manager yet and ADR-0002 (licence) is still
// open, so pulling in a framework would mean taking a dependency decision that
// is not ours to take. This is enough for null tests and numeric assertions.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace omni::test {

inline int g_checks = 0;
inline int g_failures = 0;
inline const char* g_suite = "";

inline void begin(const char* suite) {
    g_suite = suite;
    std::printf("== %s\n", suite);
}

inline int finish() {
    if (g_failures == 0) {
        std::printf("   %d check(s) passed\n", g_checks);
        return 0;
    }
    std::printf("   FAILED: %d of %d check(s)\n", g_failures, g_checks);
    return 1;
}

inline void fail(const char* file, int line, const std::string& what) {
    ++g_failures;
    std::printf("   FAIL %s:%d  %s\n", file, line, what.c_str());
}

inline void pass() { ++g_checks; }

/// Bit-level comparison, so +0.0f and -0.0f are distinguishable.
inline std::uint32_t bits(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

}  // namespace omni::test

#define OMNI_CHECK(cond)                                                      \
    do {                                                                      \
        ::omni::test::pass();                                                 \
        if (!(cond))                                                          \
            ::omni::test::fail(__FILE__, __LINE__, "expected: " #cond);       \
    } while (0)

#define OMNI_CHECK_MSG(cond, msg)                                             \
    do {                                                                      \
        ::omni::test::pass();                                                 \
        if (!(cond))                                                          \
            ::omni::test::fail(__FILE__, __LINE__,                            \
                               std::string(msg) + "  [" #cond "]");           \
    } while (0)

/// Exact float equality. For null tests: the criterion is zero, not small.
#define OMNI_CHECK_EXACT(a, b)                                                \
    do {                                                                      \
        ::omni::test::pass();                                                 \
        const float va_ = static_cast<float>(a), vb_ = static_cast<float>(b);  \
        if (!(va_ == vb_)) {                                                  \
            char buf_[160];                                                   \
            std::snprintf(buf_, sizeof buf_,                                  \
                          "not exactly equal: %.9g vs %.9g (bits %08x vs %08x)", \
                          static_cast<double>(va_), static_cast<double>(vb_), \
                          ::omni::test::bits(va_), ::omni::test::bits(vb_));  \
            ::omni::test::fail(__FILE__, __LINE__, buf_);                     \
        }                                                                     \
    } while (0)

#define OMNI_CHECK_NEAR(a, b, tol)                                            \
    do {                                                                      \
        ::omni::test::pass();                                                 \
        const double va_ = static_cast<double>(a), vb_ = static_cast<double>(b), \
                     t_ = static_cast<double>(tol);                           \
        if (!(std::fabs(va_ - vb_) <= t_)) {                                  \
            char buf_[160];                                                   \
            std::snprintf(buf_, sizeof buf_, "%.9g vs %.9g differ by %.3g > %.3g", \
                          va_, vb_, std::fabs(va_ - vb_), t_);                \
            ::omni::test::fail(__FILE__, __LINE__, buf_);                     \
        }                                                                     \
    } while (0)
