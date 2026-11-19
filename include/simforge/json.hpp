#pragma once
// DOM JSON interno: ordem de inserção preservada, números com precisão
// preservada e erro com linha/coluna. Ver ADR 0019.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace simforge::json {

class Value;

using Array = std::vector<Value>;
using Member = std::pair<std::string, Value>;
/// Objeto como lista de membros: a ordem de inserção é parte do formato.
using Object = std::vector<Member>;

enum class Type { Null, Bool, Number, String, Array, Object };

const char* to_string(Type t);

class TypeError : public std::runtime_error {
public:
    explicit TypeError(const std::string& what) : std::runtime_error(what) {}
};

struct ParseError {
    std::string message;
    std::size_t offset = 0;
    int line = 1;
    int column = 1;
    bool ok() const { return message.empty(); }
};

class Value {
public:
    Value() = default;  // null
    Value(std::nullptr_t) {}
    Value(bool v) : type_(Type::Bool), bool_(v) {}
    Value(int v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(long v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(long long v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(unsigned v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(unsigned long v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(unsigned long long v) : type_(Type::Number), raw_(std::to_string(v)) {}
    Value(double v);
    Value(const char* v) : type_(Type::String), str_(v ? v : "") {}
    Value(std::string v) : type_(Type::String), str_(std::move(v)) {}
    Value(std::string_view v) : type_(Type::String), str_(v) {}

    static Value array();
    static Value object();
    static Value number_raw(std::string raw);

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool() const;
    std::int64_t as_int64() const;
    double as_double() const;
    const std::string& as_string() const;
    std::string& as_string();

    const Array& as_array() const;
    Array& as_array();
    const Object& as_object() const;
    Object& as_object();

    std::size_t size() const;  // array/object

    // Objeto: substitui no lugar ou acrescenta ao fim.
    Value& set(std::string_view key, Value v);
    const Value* find(std::string_view key) const;
    Value* find(std::string_view key);
    bool contains(std::string_view key) const { return find(key) != nullptr; }
    bool erase(std::string_view key);
    const Value& at(std::string_view key) const;  // TypeError se ausente
    Value get(std::string_view key, Value fallback) const;

    // Array
    Value& push(Value v);
    const Value& at(std::size_t index) const;
    Value& at(std::size_t index);

    // Conveniência: cria null se ausente (objeto por chave, array por índice).
    Value& operator[](std::string_view key);
    const Value& operator[](std::string_view key) const;
    Value& operator[](std::size_t index);

    std::string dump(int indent = 2) const;
    /// Forma canônica (chaves ordenadas, sem espaços) para hash/comparação.
    std::string dump_canonical() const;

    friend bool operator==(const Value& a, const Value& b);

private:
    void dump_to(std::string& out, int indent, int level, bool canonical) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    std::string raw_;  // número como escrito
    std::string str_;
    Array arr_;
    Object obj_;
};

/// Retorna null e preenche `err` em caso de falha (nunca lança).
Value parse(std::string_view text, ParseError* err = nullptr);

inline constexpr int kMaxDepth = 64;

}  // namespace simforge::json
