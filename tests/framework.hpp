#pragma once
// Harness mínimo de testes (ADR 0019). Sem dependências externas.

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace st {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry();
int run_all(const char* filter);

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

void report_failure(const char* file, int line, const std::string& message);
int failure_count();

}  // namespace st

#define TEST(name)                                                 \
    static void name();                                            \
    static ::st::Registrar st_reg_##name(#name, &name);            \
    static void name()

#define ST_FAIL(msg) ::st::report_failure(__FILE__, __LINE__, (msg))

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) ST_FAIL(std::string("CHECK falhou: ") + #cond);    \
    } while (0)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto& st_a = (actual);                                        \
        const auto& st_b = (expected);                                      \
        if (!(st_a == st_b)) {                                              \
            std::ostringstream st_os;                                       \
            st_os << "CHECK_EQ falhou: " << #actual << " == " << #expected \
                  << " (esquerda=" << st_a << ", direita=" << st_b << ")";  \
            ST_FAIL(st_os.str());                                           \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                              \
    do {                                                                     \
        const double st_a = static_cast<double>(actual);                     \
        const double st_b = static_cast<double>(expected);                   \
        const double st_t = static_cast<double>(tolerance);                  \
        const double st_d = st_a > st_b ? st_a - st_b : st_b - st_a;         \
        if (!(st_d <= st_t)) {                                               \
            std::ostringstream st_os;                                        \
            st_os << "CHECK_NEAR falhou: " << #actual << " ≈ " << st_b       \
                  << " (delta " << st_d << " > " << st_t << ")";             \
            ST_FAIL(st_os.str());                                            \
        }                                                                    \
    } while (0)

#define CHECK_THROWS(expr)                                              \
    do {                                                                \
        bool st_threw = false;                                          \
        try {                                                           \
            (void)(expr);                                               \
        } catch (...) {                                                 \
            st_threw = true;                                            \
        }                                                               \
        if (!st_threw) ST_FAIL("CHECK_THROWS não lançou: " #expr);      \
    } while (0)
