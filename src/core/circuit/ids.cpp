#include "simforge/ids.hpp"

#include <array>
#include <cstddef>

namespace simforge {
namespace {

// Alphabet Crockford base32 (sem I, L, O, U).
constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::uint64_t g_state = 0x9E3779B97F4A7C15ull;
std::uint64_t g_counter = 0;

std::uint64_t splitmix64() {
    std::uint64_t z = (g_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

}  // namespace

void seed_ids(std::uint64_t seed) {
    g_state = seed ? seed : 1;
    g_counter = 0;
}

std::string new_ulid() {
    // 48 bits de "timestamp" determinístico + 80 bits de entropia do contador.
    std::array<std::uint8_t, 16> bytes{};
    const std::uint64_t stamp = ++g_counter;
    const std::uint64_t high = splitmix64() ^ (stamp << 16);
    const std::uint64_t low = splitmix64() ^ stamp;

    for (int i = 0; i < 6; ++i) {
        bytes[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((high >> (40 - 8 * i)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        bytes[static_cast<std::size_t>(6 + i)] =
            static_cast<std::uint8_t>((low >> (56 - 8 * i)) & 0xFF);
    }

    // Converte 128 bits para 26 dígitos base32 por divisão longa.
    std::string out(26, '0');
    for (int pos = 25; pos >= 0; --pos) {
        std::uint32_t remainder = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const std::uint32_t value =
                (remainder << 8) | static_cast<std::uint32_t>(bytes[i]);
            bytes[i] = static_cast<std::uint8_t>(value / 32);
            remainder = value % 32;
        }
        out[static_cast<std::size_t>(pos)] = kAlphabet[remainder];
    }
    return out;
}

bool is_ulid(std::string_view text) {
    if (text.size() != 26) return false;
    for (const char c : text) {
        if (c >= 'a' && c <= 'z') return false;
        bool found = false;
        for (const char a : kAlphabet) {
            if (a == c) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

}  // namespace simforge
