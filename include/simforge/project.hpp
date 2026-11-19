#pragma once
// Documento de projeto `.sforge`: o DOM é a fonte de verdade (ADR 0017).

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "simforge/json.hpp"

namespace simforge {

enum class Severity { Info, Warning, Error };
const char* to_string(Severity severity);

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;
    std::string message;
    std::string path;  // ex.: "/nets/0"
    std::string net;
    std::string fix;

    json::Value to_json() const;
};

class Project {
public:
    static Project create(std::string name, std::string board_type,
                          std::string* error);

    /// Valida o mínimo (format/schema) e assume o documento.
    static std::optional<Project> from_json(json::Value document,
                                            std::string* error);

    static std::optional<Project> load(const std::filesystem::path& path,
                                       std::string* error);

    bool save(const std::filesystem::path& path, std::string* error) const;
    std::string to_string() const;

    const json::Value& document() const { return document_; }
    json::Value& document() { return document_; }

    std::uint64_t revision() const;
    void set_revision(std::uint64_t revision);
    void bump_revision();

    std::string id() const;
    std::string name() const;

    std::vector<Diagnostic> validate() const;

    const json::Value* find_object(std::string_view array,
                                   std::string_view id) const;
    json::Value* find_object(std::string_view array, std::string_view id);

    /// `true` se o documento tem erros (severity Error).
    static bool has_errors(const std::vector<Diagnostic>& diagnostics);

private:
    json::Value document_;
};

}  // namespace simforge
