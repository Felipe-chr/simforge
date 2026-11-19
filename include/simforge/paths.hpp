#pragma once

// Conversão entre as strings de caminho da API (sempre UTF-8) e
// `std::filesystem::path`.
//
// No Windows, `std::filesystem::path` guarda `wchar_t`; construir o caminho
// direto de um `std::string` interpreta os bytes no code page ANSI e quebra com
// acentos ("Área de Trabalho", "usuário"). Como o JSON e a ABI C trafegam UTF-8,
// a conversão precisa passar por `char8_t`.

#include <filesystem>
#include <string>
#include <string_view>

namespace simforge {

/// Caminho UTF-8 vindo da API → `std::filesystem::path`.
inline std::filesystem::path path_from_utf8(std::string_view text) {
#if defined(_WIN32)
    const auto* begin = reinterpret_cast<const char8_t*>(text.data());
    return std::filesystem::path(std::u8string(begin, begin + text.size()));
#else
    return std::filesystem::path(std::string(text));
#endif
}

/// `std::filesystem::path` → caminho UTF-8 para devolver pela API.
inline std::string utf8_from_path(const std::filesystem::path& path) {
#if defined(_WIN32)
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
#else
    return path.string();
#endif
}

}  // namespace simforge
