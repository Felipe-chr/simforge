#include "framework.hpp"
#include "simforge/json.hpp"

using simforge::json::ParseError;
using simforge::json::Type;
using simforge::json::Value;

TEST(json_parse_scalars) {
    CHECK(Value(1) == Value(1));
    CHECK(Value(1) != Value(2));
    CHECK(Value("a") == Value("a"));
    CHECK(Value(true).as_bool());

    ParseError err;
    Value v = simforge::json::parse(R"({"a": 1, "b": "x", "c": true, "d": null})", &err);
    CHECK(err.ok());
    CHECK_EQ(v["a"].as_int64(), 1);
    CHECK_EQ(v["b"].as_string(), std::string("x"));
    CHECK(v["c"].as_bool());
    CHECK(v["d"].is_null());
}

TEST(json_numbers_preserve_precision) {
    ParseError err;
    Value v = simforge::json::parse(
        R"({"big": 18446744073709551615, "frac": 0.1, "exp": 1.5e-7})", &err);
    CHECK(err.ok());
    CHECK_EQ(v["big"].dump(), std::string("18446744073709551615"));
    CHECK_EQ(v["frac"].dump(), std::string("0.1"));
    CHECK_EQ(v["exp"].dump(), std::string("1.5e-7"));
    CHECK_NEAR(v["frac"].as_double(), 0.1, 1e-12);
    CHECK_THROWS(v["frac"].as_int64());
}

TEST(json_object_preserves_insertion_order) {
    Value obj = Value::object();
    obj.set("z", 1);
    obj.set("a", 2);
    obj.set("m", 3);
    const std::string compact = obj.dump(0);
    CHECK_EQ(compact, std::string(R"({"z":1,"a":2,"m":3})"));
    // Ordem canônica é ordenada por chave.
    CHECK_EQ(obj.dump_canonical(), std::string(R"({"a":2,"m":3,"z":1})"));
}

TEST(json_roundtrip_document) {
    const std::string text = R"({
  "format": "simforge.project",
  "schema": 1,
  "project": {"id": "01J9Z6K3V8Q2M4T7R1C5B0N8XA", "name": "Teste", "revision": 3},
  "devices": [],
  "extensions": {"acme.plugin": {"custom": [1, 2, 3]}}
})";
    ParseError err;
    Value v = simforge::json::parse(text, &err);
    CHECK(err.ok());
    const std::string dumped = v.dump(2);
    ParseError err2;
    Value again = simforge::json::parse(dumped, &err2);
    CHECK(err2.ok());
    CHECK(v == again);
    // Campos desconhecidos sobrevivem.
    CHECK(again["extensions"]["acme.plugin"]["custom"].size() == 3);
}

TEST(json_string_escapes) {
    ParseError err;
    Value v = simforge::json::parse(R"("a\"b\\c\n\t\u00e9\ud83d\ude00")", &err);
    CHECK(err.ok());
    CHECK_EQ(v.as_string(), std::string("a\"b\\c\n\t\xC3\xA9\xF0\x9F\x98\x80"));
    const std::string dumped = v.dump(0);
    ParseError err2;
    CHECK(simforge::json::parse(dumped, &err2) == v);
}

TEST(json_rejects_invalid) {
    ParseError err;
    simforge::json::parse("{", &err);
    CHECK(!err.ok());
    CHECK_EQ(err.line, 1);

    ParseError err2;
    simforge::json::parse("{\"a\": 1,}", &err2);
    CHECK(!err2.ok());

    ParseError err3;
    simforge::json::parse("[1, 2] extra", &err3);
    CHECK(!err3.ok());

    ParseError err4;
    simforge::json::parse("01", &err4);
    CHECK(!err4.ok());
}

TEST(json_rejects_deep_nesting) {
    std::string deep;
    for (int i = 0; i <= simforge::json::kMaxDepth + 1; ++i) deep.push_back('[');
    for (int i = 0; i <= simforge::json::kMaxDepth + 1; ++i) deep.push_back(']');
    ParseError err;
    simforge::json::parse(deep, &err);
    CHECK(!err.ok());
    CHECK(err.message.find("profundidade") != std::string::npos);
}

TEST(json_error_reports_line_and_column) {
    ParseError err;
    simforge::json::parse("{\n  \"a\": 1,\n  \"b\": oops\n}", &err);
    CHECK(!err.ok());
    CHECK_EQ(err.line, 3);
    CHECK(err.column > 1);
}

TEST(json_helpers) {
    Value obj = Value::object();
    obj["name"] = Value("x");
    CHECK(obj.contains("name"));
    CHECK(!obj.contains("missing"));
    CHECK_EQ(obj.get("missing", Value(7)).as_int64(), 7);
    CHECK(obj.erase("name"));
    CHECK(!obj.contains("name"));

    Value arr = Value::array();
    arr.push(1);
    arr.push(2);
    CHECK_EQ(arr.size(), std::size_t(2));
    CHECK_EQ(arr.at(1).as_int64(), 2);
    CHECK_THROWS(arr.at(5));
}
