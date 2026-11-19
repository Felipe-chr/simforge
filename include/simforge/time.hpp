#pragma once
// Tempo de simulação. Sempre inteiro em nanossegundos (ADR 0003).

#include <cstdint>
#include <string>

namespace simforge {

using SimTime = std::int64_t;

inline constexpr SimTime kNsPerUs = 1'000;
inline constexpr SimTime kNsPerMs = 1'000'000;
inline constexpr SimTime kNsPerSec = 1'000'000'000;

namespace literals {
constexpr SimTime operator""_ns(unsigned long long v) noexcept {
    return static_cast<SimTime>(v);
}
constexpr SimTime operator""_us(unsigned long long v) noexcept {
    return static_cast<SimTime>(v) * kNsPerUs;
}
constexpr SimTime operator""_ms(unsigned long long v) noexcept {
    return static_cast<SimTime>(v) * kNsPerMs;
}
constexpr SimTime operator""_s(unsigned long long v) noexcept {
    return static_cast<SimTime>(v) * kNsPerSec;
}
}  // namespace literals

/// Maior instante representável sem overflow em somas comuns.
inline constexpr SimTime kMaxSimTime = INT64_MAX / 2;

/// Formata para leitura humana: "1.500 ms", "42 ns".
std::string format_sim_time(SimTime t);

}  // namespace simforge
