#pragma once

// Minimal self-contained test harness. No third-party dependency, so the
// tests build anywhere the plugin builds.
//
//   TEST_CASE(name) { CHECK(x); CHECK_EQ(a, b); CHECK_NEAR(a, b, eps); }
//
// REQUIRE(x) stops the current test case on failure. Test cases run in
// registration order within a file; files run in link order. Each case
// must set up whatever global plugin state it depends on.

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace testfw
{
    struct Case
    {
        const char*           name;
        const char*           file;
        std::function<void()> fn;
    };

    inline std::vector<Case>& Registry()
    {
        static std::vector<Case> r;
        return r;
    }

    struct Registrar
    {
        Registrar(const char* name, const char* file, std::function<void()> fn)
        {
            Registry().push_back({ name, file, std::move(fn) });
        }
    };

    struct RequireFailed {};

    inline int& FailuresInCase()
    {
        static int n = 0;
        return n;
    }

    inline void ReportFailure(const char* file, int line, const std::string& msg)
    {
        ++FailuresInCase();
        std::printf("    FAIL %s:%d: %s\n", file, line, msg.c_str());
    }

    template <typename T>
    std::string Show(const T& v)
    {
        std::ostringstream os;
        if constexpr (std::is_same_v<T, bool>) os << (v ? "true" : "false");
        else if constexpr (std::is_enum_v<T>)  os << static_cast<long long>(v);
        else if constexpr (std::is_pointer_v<T> && !std::is_same_v<T, const char*> && !std::is_same_v<T, char*>)
            os << static_cast<const void*>(v);
        else if constexpr (std::is_same_v<T, std::wstring>)
            for (wchar_t c : v) os << (c < 128 ? static_cast<char>(c) : '?');
        else if constexpr (requires { os << v; }) os << v;
        else                                    os << "<value>";
        return os.str();
    }
}

#define TESTFW_CAT2(a, b) a##b
#define TESTFW_CAT(a, b) TESTFW_CAT2(a, b)

#define TEST_CASE(name)                                                        \
    static void name();                                                        \
    static ::testfw::Registrar TESTFW_CAT(name, _registrar)(#name, __FILE__, &name); \
    static void name()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) ::testfw::ReportFailure(__FILE__, __LINE__, "CHECK(" #expr ")"); \
    } while (0)

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) {                                                         \
            ::testfw::ReportFailure(__FILE__, __LINE__, "REQUIRE(" #expr ")"); \
            throw ::testfw::RequireFailed{};                                   \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        /* Copies, not references: (a) may point into a temporary. */          \
        const auto testfw_a = (a);                                             \
        const auto testfw_b = (b);                                             \
        if (!(testfw_a == testfw_b))                                           \
            ::testfw::ReportFailure(__FILE__, __LINE__,                        \
                std::string("CHECK_EQ(" #a ", " #b "): ")                      \
                + ::testfw::Show(testfw_a) + " != " + ::testfw::Show(testfw_b)); \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        const double testfw_a = static_cast<double>(a);                        \
        const double testfw_b = static_cast<double>(b);                        \
        if (!(std::fabs(testfw_a - testfw_b) <= (eps)))                        \
            ::testfw::ReportFailure(__FILE__, __LINE__,                        \
                std::string("CHECK_NEAR(" #a ", " #b "): ")                    \
                + ::testfw::Show(testfw_a) + " vs " + ::testfw::Show(testfw_b)); \
    } while (0)
