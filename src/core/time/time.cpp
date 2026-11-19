#include "simforge/time.hpp"

#include <cstdio>

namespace simforge {

std::string format_sim_time(SimTime t) {
    char buf[64];
    const bool negative = t < 0;
    const auto abs_t = static_cast<unsigned long long>(negative ? -t : t);
    if (abs_t < 1'000ULL) {
        std::snprintf(buf, sizeof(buf), "%s%llu ns", negative ? "-" : "", abs_t);
    } else if (abs_t < 1'000'000ULL) {
        std::snprintf(buf, sizeof(buf), "%s%.3f us", negative ? "-" : "",
                      static_cast<double>(abs_t) / 1e3);
    } else if (abs_t < 1'000'000'000ULL) {
        std::snprintf(buf, sizeof(buf), "%s%.3f ms", negative ? "-" : "",
                      static_cast<double>(abs_t) / 1e6);
    } else {
        std::snprintf(buf, sizeof(buf), "%s%.6f s", negative ? "-" : "",
                      static_cast<double>(abs_t) / 1e9);
    }
    return buf;
}

}  // namespace simforge
