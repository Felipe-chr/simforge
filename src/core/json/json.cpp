#include "simforge/json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace simforge::json {
namespace {

constexpr std::size_t kMaxStringBytes = 64u * 1024u * 1024u;

[[noreturn]] void type_fail(Type actual, const char* expected) {
    throw TypeError(std::string("json: esperado ") + expected + ", veio " +
                    to_string(actual));
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Parser {
public:
    Parser(std::string_view text, ParseError* err) : text_(text), err_(err) {}

    Value run() {
        skip_ws();
        Value v = parse_value(0);
        if (err_ && !err_->ok()) return Value();
        skip_ws();
        if (pos_ != text_.size()) {
            fail("conteúdo extra após o valor");
            return Value();
        }
        return v;
    }

private:
    void fail(const std::string& message) {
        if (!err_ || !err_->ok()) return;
        err_->message = message;
        err_->offset = pos_;
        err_->line = line_;
        err_->column = column_;
    }

    void advance() {
        if (pos_ < text_.size()) {
            if (text_[pos_] == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            ++pos_;
        }
    }

    void skip_ws() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
            } else {
                break;
            }
        }
    }

    bool consume(char expected) {
        if (pos_ < text_.size() && text_[pos_] == expected) {
            advance();
            return true;
        }
        return false;
    }

    bool consume_literal(std::string_view lit) {
        if (text_.compare(pos_, lit.size(), lit) == 0) {
            for (std::size_t i = 0; i < lit.size(); ++i) advance();
            return true;
        }
        return false;
    }

    Value parse_value(int depth) {
        if (depth > kMaxDepth) {
            fail("profundidade máxima excedida");
            return Value();
        }
        skip_ws();
        if (pos_ >= text_.size()) {
            fail("fim inesperado");
            return Value();
        }
        const char c = text_[pos_];
        switch (c) {
            case '{': return parse_object(depth);
            case '[': return parse_array(depth);
            case '"': return Value(parse_string());
            case 't':
                if (consume_literal("true")) return Value(true);
                fail("literal inválido");
                return Value();
            case 'f':
                if (consume_literal("false")) return Value(false);
                fail("literal inválido");
                return Value();
            case 'n':
                if (consume_literal("null")) return Value();
                fail("literal inválido");
                return Value();
            default:
                if (c == '-' || is_digit(c)) return parse_number();
                fail("caractere inesperado");
                return Value();
        }
    }

    Value parse_object(int depth) {
        Value obj = Value::object();
        advance();  // {
        skip_ws();
        if (consume('}')) return obj;
        while (true) {
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                fail("esperado nome de campo entre aspas");
                return Value();
            }
            std::string key = parse_string();
            skip_ws();
            if (!consume(':')) {
                fail("esperado ':'");
                return Value();
            }
            Value v = parse_value(depth + 1);
            if (err_ && !err_->ok()) return Value();
            obj.set(key, std::move(v));
            skip_ws();
            if (consume(',')) continue;
            if (consume('}')) break;
            fail("esperado ',' ou '}'");
            return Value();
        }
        return obj;
    }

    Value parse_array(int depth) {
        Value arr = Value::array();
        advance();  // [
        skip_ws();
        if (consume(']')) return arr;
        while (true) {
            Value v = parse_value(depth + 1);
            if (err_ && !err_->ok()) return Value();
            arr.push(std::move(v));
            skip_ws();
            if (consume(',')) continue;
            if (consume(']')) break;
            fail("esperado ',' ou ']'");
            return Value();
        }
        return arr;
    }

    std::uint32_t parse_hex4() {
        std::uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ >= text_.size()) {
                fail("\\u incompleto");
                return 0;
            }
            const char c = text_[pos_];
            cp <<= 4;
            if (c >= '0' && c <= '9') {
                cp |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                cp |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                cp |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                fail("hex inválido em \\u");
                return 0;
            }
            advance();
        }
        return cp;
    }

    std::string parse_string() {
        std::string out;
        advance();  // aspas de abertura
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == '"') {
                advance();
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                fail("caractere de controle em string");
                return {};
            }
            if (c == '\\') {
                advance();
                if (pos_ >= text_.size()) {
                    fail("escape incompleto");
                    return {};
                }
                const char e = text_[pos_];
                advance();
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        std::uint32_t cp = parse_hex4();
                        if (!(err_ && !err_->ok())) {
                            if (cp >= 0xD800 && cp <= 0xDBFF) {
                                if (text_.compare(pos_, 2, "\\u") == 0) {
                                    advance();
                                    advance();
                                    const std::uint32_t low = parse_hex4();
                                    if (low >= 0xDC00 && low <= 0xDFFF) {
                                        cp = 0x10000 + ((cp - 0xD800) << 10) +
                                             (low - 0xDC00);
                                    } else {
                                        fail("par surrogate inválido");
                                        return {};
                                    }
                                } else {
                                    fail("surrogate alto sem par");
                                    return {};
                                }
                            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                                fail("surrogate baixo isolado");
                                return {};
                            }
                            append_utf8(out, cp);
                        }
                        break;
                    }
                    default:
                        fail("escape inválido");
                        return {};
                }
                if (err_ && !err_->ok()) return {};
                continue;
            }
            out.push_back(c);
            advance();
            if (out.size() > kMaxStringBytes) {
                fail("string muito longa");
                return {};
            }
        }
        fail("string não terminada");
        return {};
    }

    Value parse_number() {
        const std::size_t start = pos_;
        if (consume('-')) { /* sinal */ }
        if (pos_ >= text_.size()) {
            fail("número incompleto");
            return Value();
        }
        if (text_[pos_] == '0') {
            advance();
        } else if (is_digit(text_[pos_])) {
            while (pos_ < text_.size() && is_digit(text_[pos_])) advance();
        } else {
            fail("dígito esperado");
            return Value();
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            advance();
            if (pos_ >= text_.size() || !is_digit(text_[pos_])) {
                fail("fração vazia");
                return Value();
            }
            while (pos_ < text_.size() && is_digit(text_[pos_])) advance();
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            advance();
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                advance();
            }
            if (pos_ >= text_.size() || !is_digit(text_[pos_])) {
                fail("expoente inválido");
                return Value();
            }
            while (pos_ < text_.size() && is_digit(text_[pos_])) advance();
        }
        return Value::number_raw(std::string(text_.substr(start, pos_ - start)));
    }

    std::string_view text_;
    ParseError* err_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

void dump_string(std::string& out, const std::string& s) {
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

}  // namespace

const char* to_string(Type t) {
    switch (t) {
        case Type::Null: return "null";
        case Type::Bool: return "bool";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Object: return "object";
    }
    return "?";
}

Value::Value(double v) : type_(Type::Number) {
    if (!std::isfinite(v)) {
        raw_ = "0";
        return;
    }
    char buf[40];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    raw_.assign(buf, res.ptr);
}

Value Value::array() {
    Value v;
    v.type_ = Type::Array;
    return v;
}

Value Value::object() {
    Value v;
    v.type_ = Type::Object;
    return v;
}

Value Value::number_raw(std::string raw) {
    Value v;
    v.type_ = Type::Number;
    v.raw_ = std::move(raw);
    return v;
}

bool Value::as_bool() const {
    if (type_ != Type::Bool) type_fail(type_, "bool");
    return bool_;
}

std::int64_t Value::as_int64() const {
    if (type_ != Type::Number) type_fail(type_, "number");
    std::int64_t out = 0;
    const auto res = std::from_chars(raw_.data(), raw_.data() + raw_.size(), out);
    if (res.ec != std::errc{} || res.ptr != raw_.data() + raw_.size()) {
        const double d = as_double();
        if (d != std::floor(d) || std::fabs(d) > 9.0e15) {
            throw TypeError("json: número não é inteiro: " + raw_);
        }
        return static_cast<std::int64_t>(d);
    }
    return out;
}

double Value::as_double() const {
    if (type_ != Type::Number) type_fail(type_, "number");
    char* end = nullptr;
    const double v = std::strtod(raw_.c_str(), &end);
    if (end == raw_.c_str()) throw TypeError("json: número inválido: " + raw_);
    return v;
}

const std::string& Value::as_string() const {
    if (type_ != Type::String) type_fail(type_, "string");
    return str_;
}

std::string& Value::as_string() {
    if (type_ != Type::String) type_fail(type_, "string");
    return str_;
}

const Array& Value::as_array() const {
    if (type_ != Type::Array) type_fail(type_, "array");
    return arr_;
}

Array& Value::as_array() {
    if (type_ != Type::Array) type_fail(type_, "array");
    return arr_;
}

const Object& Value::as_object() const {
    if (type_ != Type::Object) type_fail(type_, "object");
    return obj_;
}

Object& Value::as_object() {
    if (type_ != Type::Object) type_fail(type_, "object");
    return obj_;
}

std::size_t Value::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    return 0;
}

Value& Value::set(std::string_view key, Value v) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
    }
    for (auto& [k, existing] : obj_) {
        if (k == key) {
            existing = std::move(v);
            return existing;
        }
    }
    obj_.emplace_back(std::string(key), std::move(v));
    return obj_.back().second;
}

const Value* Value::find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& [k, v] : obj_) {
        if (k == key) return &v;
    }
    return nullptr;
}

Value* Value::find(std::string_view key) {
    if (type_ != Type::Object) return nullptr;
    for (auto& [k, v] : obj_) {
        if (k == key) return &v;
    }
    return nullptr;
}

bool Value::erase(std::string_view key) {
    if (type_ != Type::Object) return false;
    for (auto it = obj_.begin(); it != obj_.end(); ++it) {
        if (it->first == key) {
            obj_.erase(it);
            return true;
        }
    }
    return false;
}

const Value& Value::at(std::string_view key) const {
    const Value* v = find(key);
    if (!v) throw TypeError("json: campo ausente: " + std::string(key));
    return *v;
}

Value Value::get(std::string_view key, Value fallback) const {
    const Value* v = find(key);
    return v ? *v : std::move(fallback);
}

Value& Value::push(Value v) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
    }
    arr_.push_back(std::move(v));
    return arr_.back();
}

const Value& Value::at(std::size_t index) const {
    if (type_ != Type::Array) type_fail(type_, "array");
    if (index >= arr_.size()) throw TypeError("json: índice fora do intervalo");
    return arr_[index];
}

Value& Value::at(std::size_t index) {
    if (type_ != Type::Array) type_fail(type_, "array");
    if (index >= arr_.size()) throw TypeError("json: índice fora do intervalo");
    return arr_[index];
}

Value& Value::operator[](std::string_view key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
    }
    if (Value* v = find(key)) return *v;
    obj_.emplace_back(std::string(key), Value());
    return obj_.back().second;
}

const Value& Value::operator[](std::string_view key) const {
    return at(key);
}

Value& Value::operator[](std::size_t index) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
    }
    if (index >= arr_.size()) arr_.resize(index + 1);
    return arr_[index];
}

std::string Value::dump(int indent) const {
    std::string out;
    dump_to(out, indent, 0, false);
    return out;
}

std::string Value::dump_canonical() const {
    std::string out;
    dump_to(out, 0, 0, true);
    return out;
}

void Value::dump_to(std::string& out, int indent, int level, bool canonical) const {
    const auto newline = [&](int lvl) {
        if (indent <= 0) return;
        out.push_back('\n');
        out.append(static_cast<std::size_t>(lvl) * static_cast<std::size_t>(indent), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; return;
        case Type::Bool: out += bool_ ? "true" : "false"; return;
        case Type::Number: out += raw_.empty() ? "0" : raw_; return;
        case Type::String: dump_string(out, str_); return;
        case Type::Array: {
            if (arr_.empty()) {
                out += "[]";
                return;
            }
            out.push_back('[');
            for (std::size_t i = 0; i < arr_.size(); ++i) {
                if (i) out.push_back(',');
                newline(level + 1);
                arr_[i].dump_to(out, indent, level + 1, canonical);
            }
            newline(level);
            out.push_back(']');
            return;
        }
        case Type::Object: {
            if (obj_.empty()) {
                out += "{}";
                return;
            }
            std::vector<const Member*> members;
            members.reserve(obj_.size());
            for (const auto& m : obj_) members.push_back(&m);
            if (canonical) {
                std::sort(members.begin(), members.end(),
                          [](const Member* a, const Member* b) {
                              return a->first < b->first;
                          });
            }
            out.push_back('{');
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (i) out.push_back(',');
                newline(level + 1);
                dump_string(out, members[i]->first);
                out.push_back(':');
                if (indent > 0) out.push_back(' ');
                members[i]->second.dump_to(out, indent, level + 1, canonical);
            }
            newline(level);
            out.push_back('}');
            return;
        }
    }
}

bool operator==(const Value& a, const Value& b) {
    if (a.type_ != b.type_) return false;
    switch (a.type_) {
        case Type::Null: return true;
        case Type::Bool: return a.bool_ == b.bool_;
        case Type::Number:
            return a.as_double() == b.as_double();
        case Type::String: return a.str_ == b.str_;
        case Type::Array: {
            if (a.arr_.size() != b.arr_.size()) return false;
            for (std::size_t i = 0; i < a.arr_.size(); ++i) {
                if (!(a.arr_[i] == b.arr_[i])) return false;
            }
            return true;
        }
        case Type::Object: {
            if (a.obj_.size() != b.obj_.size()) return false;
            for (const auto& [k, v] : a.obj_) {
                const Value* other = b.find(k);
                if (!other || !(v == *other)) return false;
            }
            return true;
        }
    }
    return false;
}

Value parse(std::string_view text, ParseError* err) {
    if (err) *err = ParseError{};
    Parser p(text, err);
    return p.run();
}

}  // namespace simforge::json
