#include "framework.hpp"

#include <cstring>
#include <iostream>

namespace st {
namespace {
int g_failures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

int failure_count() { return g_failures; }

void report_failure(const char* file, int line, const std::string& message) {
    ++g_failures;
    std::cout << "  FALHA " << file << ":" << line << "  " << message << "\n";
}

int run_all(const char* filter) {
    int ran = 0;
    int failed_tests = 0;
    for (const auto& test : registry()) {
        if (filter && *filter && std::strstr(test.name, filter) == nullptr) continue;
        const int before = g_failures;
        std::cout << "[ RUN  ] " << test.name << "\n";
        try {
            test.fn();
        } catch (const std::exception& e) {
            report_failure(__FILE__, __LINE__,
                           std::string("exceção não tratada: ") + e.what());
        } catch (...) {
            report_failure(__FILE__, __LINE__, "exceção desconhecida");
        }
        ++ran;
        if (g_failures != before) {
            ++failed_tests;
            std::cout << "[ FAIL ] " << test.name << "\n";
        } else {
            std::cout << "[  OK  ] " << test.name << "\n";
        }
    }
    std::cout << "\n" << ran << " testes, " << failed_tests << " falharam, "
              << g_failures << " asserções falharam\n";
    return g_failures == 0 ? 0 : 1;
}

}  // namespace st

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    return st::run_all(filter);
}
