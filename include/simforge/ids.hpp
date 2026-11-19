#pragma once
// Geração de identificadores ULID (26 caracteres Crockford base32).

#include <cstdint>
#include <string>
#include <string_view>

namespace simforge {

/// ULID monotonamente crescente dentro do processo.
std::string new_ulid();

/// Reinicia o gerador (testes determinísticos).
void seed_ids(std::uint64_t seed);

bool is_ulid(std::string_view text);

}  // namespace simforge
