// Incogine — minimal headless test harness (no third-party deps).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The repo had no test infrastructure, so this is deliberately tiny rather than
// a framework: count checks, print failures with file/line, return a non-zero
// exit code. Registered with CTest (`add_test`), so `ctest` runs the suites.
//
// Each suite is its own executable with its own `main()` (see anim_tests.cpp /
// asset_tests.cpp), which keeps adding a module to a few lines of CMake instead
// of a central registration list.
//
//   CHECK(cond)            records a failure and keeps going
//   REQUIRE(cond)          records a failure and returns from the test function
//                          (use before dereferencing, so one failure does not
//                          cascade into a crash)
//   CHECK_EQ(a, b)         compares with ==, printing both values
//   CHECK_NEAR(a, b, eps)  float comparison
//
// TEST_GROUP labels the output so a failure names the area, not just a line.

#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace icgtest {

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline int& checkCount() {
    static int count = 0;
    return count;
}

inline std::string& currentGroup() {
    static std::string group = "general";
    return group;
}

// Renders a value for failure output. Falls back to "<?>"" for types without
// operator<< (enums, structs) so a test can still use CHECK_EQ on them.
template <typename T>
inline std::string Show(const T& value) {
    return "<?>";
}
inline std::string Show(const bool& value) { return value ? "true" : "false"; }
inline std::string Show(const int& value) { return std::to_string(value); }
inline std::string Show(const unsigned& value) { return std::to_string(value); }
inline std::string Show(const long& value) { return std::to_string(value); }
inline std::string Show(const unsigned long& value) {
    return std::to_string(value);
}
inline std::string Show(const long long& value) {
    return std::to_string(value);
}
inline std::string Show(const unsigned long long& value) {
    return std::to_string(value);
}
inline std::string Show(const float& value) { return std::to_string(value); }
inline std::string Show(const double& value) { return std::to_string(value); }
inline std::string Show(const char* value) {
    return value != nullptr ? std::string("\"") + value + "\"" : "(null)";
}
inline std::string Show(const std::string& value) {
    return "\"" + value + "\"";
}

inline void Record(bool passed, const char* expr, const char* file, int line,
                   const std::string& detail) {
    ++checkCount();
    if (passed) {
        return;
    }
    ++failureCount();
    std::printf("FAIL  [%s] %s%s%s  (%s:%d)\n", currentGroup().c_str(), expr,
                detail.empty() ? "" : " -- ", detail.c_str(), file, line);
}

inline int Report(const char* suite) {
    if (failureCount() == 0) {
        std::printf("PASS  %s (%d checks)\n", suite, checkCount());
        return 0;
    }
    std::printf("FAIL  %s (%d of %d checks failed)\n", suite,
                failureCount(), checkCount());
    return 1;
}

} // namespace icgtest

#define TEST_GROUP(name) ::icgtest::currentGroup() = (name)

#define CHECK(cond) \
    ::icgtest::Record((cond), #cond, __FILE__, __LINE__, std::string())

#define REQUIRE(cond)                                     \
    do {                                                  \
        ::icgtest::Record((cond), #cond, __FILE__,        \
                          __LINE__, std::string());      \
        if (!(cond)) {                                   \
            return;                                      \
        }                                                 \
    } while (0)

#define CHECK_MSG(cond, msg) \
    ::icgtest::Record((cond), #cond, __FILE__, __LINE__, (msg))

// Every macro below binds its arguments to locals FIRST, so each expression is
// evaluated exactly once. Without that, a side-effecting argument (a call that
// mutates the document under test, say) would run once for the comparison and
// again while formatting the failure message — which silently corrupts the
// state the test is asserting on and reports a nonsense mismatch.
#define CHECK_EQ(a, b)                                                     \
    do {                                                                    \
        auto icg_a = (a);                                                   \
        auto icg_b = (b);                                                   \
        ::icgtest::Record(icg_a == icg_b, #a " == " #b, __FILE__,          \
                          __LINE__,                                         \
                          "got " + ::icgtest::Show(icg_a) + ", want " +     \
                              ::icgtest::Show(icg_b));                     \
    } while (0)

#define REQUIRE_EQ(a, b)                                                   \
    do {                                                                    \
        auto icg_a = (a);                                                   \
        auto icg_b = (b);                                                   \
        const bool icg_ok = icg_a == icg_b;                                 \
        ::icgtest::Record(icg_ok, #a " == " #b, __FILE__, __LINE__,        \
                          "got " + ::icgtest::Show(icg_a) + ", want " +     \
                              ::icgtest::Show(icg_b));                     \
        if (!icg_ok) {                                                      \
            return;                                                         \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                    \
        auto icg_a = (a);                                                   \
        auto icg_b = (b);                                                   \
        const double icg_eps = static_cast<double>(eps);                   \
        ::icgtest::Record(                                                  \
            std::fabs(static_cast<double>(icg_a) -                         \
                      static_cast<double>(icg_b)) <= icg_eps,               \
            #a " ~= " #b, __FILE__, __LINE__,                               \
            "got " + ::icgtest::Show(icg_a) + ", want " +                  \
                ::icgtest::Show(icg_b));                                    \
    } while (0)