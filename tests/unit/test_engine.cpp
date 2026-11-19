#include "framework.hpp"
#include "simforge/board.hpp"
#include "simforge/engine.hpp"

using namespace simforge;
using simforge::json::Value;

namespace {

Value object() { return Value::object(); }

void set(Value& obj, const char* key, Value value) {
    obj.set(key, std::move(value));
}

Value board_entry(const char* id, const char* type) {
    Value b = object();
    set(b, "id", Value(id));
    set(b, "type", Value(type));
    set(b, "params", object());
    return b;
}

Value device_entry(const char* id, const char* type) {
    Value d = object();
    set(d, "id", Value(id));
    set(d, "type", Value(type));
    set(d, "params", object());
    return d;
}

Value pin_ref(const char* kind, const char* owner, const char* pin) {
    Value p = object();
    set(p, kind, Value(owner));
    set(p, "pin", Value(pin));
    return p;
}

Value net_entry(const char* id, Value pins) {
    Value n = object();
    set(n, "id", Value(id));
    set(n, "name", Value(id));
    set(n, "kind", Value("digital"));
    set(n, "pins", std::move(pins));
    return n;
}

/// Placa + LED + nets: GPIO2 -> LED.A, GND -> LED.K.
Value led_document() {
    Value doc = object();
    set(doc, "format", Value("simforge.project"));
    set(doc, "schema", Value(1));

    Value boards = Value::array();
    boards.push(board_entry("board1", "simforge.esp32s3.devkitc"));
    set(doc, "boards", std::move(boards));

    Value devices = Value::array();
    devices.push(device_entry("led1", "simforge.led"));
    set(doc, "devices", std::move(devices));

    Value nets = Value::array();
    Value anode = Value::array();
    anode.push(pin_ref("board", "board1", "GPIO2"));
    anode.push(pin_ref("device", "led1", "A"));
    nets.push(net_entry("n_a", std::move(anode)));

    Value cathode = Value::array();
    cathode.push(pin_ref("board", "board1", "GND"));
    cathode.push(pin_ref("device", "led1", "K"));
    nets.push(net_entry("n_k", std::move(cathode)));
    set(doc, "nets", std::move(nets));

    return doc;
}

}  // namespace

TEST(engine_load_requires_core_arrays) {
    Engine engine;
    std::string error;
    Value empty = object();
    CHECK(!engine.load(empty, &error));
    CHECK(!error.empty());
    CHECK(engine.state() == EngineState::Error);
}

TEST(engine_load_rejects_unknown_device) {
    Value doc = led_document();
    doc["devices"].at(0).set("type", Value("simforge.naoexiste"));
    Engine engine;
    std::string error;
    CHECK(!engine.load(doc, &error));
    CHECK(error.find("desconhecido") != std::string::npos);
}

TEST(engine_load_rejects_pin_on_two_nets) {
    Value doc = led_document();
    // Move LED.A também para a net do catodo.
    Value extra = Value::array();
    extra.push(pin_ref("device", "led1", "A"));
    doc["nets"].at(0)["pins"].push(pin_ref("device", "led1", "K"));
    Engine engine;
    std::string error;
    CHECK(!engine.load(doc, &error));
}

TEST(engine_led_turns_on_when_pin_high) {
    Value doc = led_document();
    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));
    if (!error.empty()) ST_FAIL(error);

    CHECK(engine.state() == EngineState::Ready);
    CHECK(engine.set_pin("board1", "GPIO2", Level::High, &error));
    if (!error.empty()) ST_FAIL(error);

    const Value state = engine.frame(0);
    const Value* device = nullptr;
    for (const auto& item : state["devices"].as_array()) {
        const Value* id = item.find("id");
        if (id && id->is_string() && id->as_string() == "led1") device = &item;
    }
    CHECK(device != nullptr);
    if (device) {
        const Value* on = device->find("on");
        CHECK(on != nullptr && on->is_bool() && on->as_bool());
    }

    // Soltar o pino apaga o LED.
    CHECK(engine.release_pin("board1", "GPIO2", &error));
    const Value after = engine.frame(1);
    for (const auto& item : after["devices"].as_array()) {
        const Value* id = item.find("id");
        if (id && id->is_string() && id->as_string() == "led1") {
            const Value* on = item.find("on");
            CHECK(on != nullptr && on->is_bool() && !on->as_bool());
        }
    }
}

TEST(engine_set_pin_validates_target) {
    Value doc = led_document();
    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));

    CHECK(!engine.set_pin("board1", "GPIO99", Level::High, &error));
    CHECK(!engine.set_pin("nao_existe", "GPIO2", Level::High, &error));
    CHECK(!engine.set_pin("led1", "A", Level::High, &error));
    // Pino fixo de alimentação não pode ser dirigido.
    CHECK(!engine.set_pin("board1", "GND", Level::High, &error));
}

TEST(engine_deterministic_hash) {
    const auto scenario = [](std::uint64_t& hash, std::size_t& events) {
        Value doc = led_document();
        Engine engine;
        std::string error;
        if (!engine.load(doc, &error)) return false;
        for (int i = 0; i < 10; ++i) {
            const Level level = (i % 2 == 0) ? Level::High : Level::Low;
            if (!engine.set_pin("board1", "GPIO2", level, &error)) return false;
            engine.run_for(1'000'000);  // 1 ms
        }
        hash = engine.trace_hash();
        events = engine.event_count();
        return true;
    };

    std::uint64_t hash1 = 0;
    std::uint64_t hash2 = 0;
    std::size_t events1 = 0;
    std::size_t events2 = 0;
    CHECK(scenario(hash1, events1));
    CHECK(scenario(hash2, events2));
    CHECK_EQ(hash1, hash2);
    CHECK_EQ(events1, events2);
}

TEST(engine_reset_restores_initial_hash) {
    Value doc = led_document();
    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));
    const std::uint64_t initial = engine.trace_hash();

    CHECK(engine.set_pin("board1", "GPIO2", Level::High, &error));
    engine.run_for(5'000'000);
    CHECK(engine.now() == 5'000'000);

    engine.reset();
    CHECK_EQ(engine.now(), SimTime(0));
    CHECK(engine.state() == EngineState::Ready);
    CHECK_EQ(engine.trace_hash(), initial);
}

TEST(engine_contention_resolves_to_x) {
    // Dois pinos fortes opostos na mesma net: o solver marca X.
    Value doc = object();
    set(doc, "format", Value("simforge.project"));
    set(doc, "schema", Value(1));

    Value boards = Value::array();
    boards.push(board_entry("board1", "simforge.esp32s3.devkitc"));
    boards.push(board_entry("board2", "simforge.esp32s3.devkitc"));
    set(doc, "boards", std::move(boards));
    set(doc, "devices", Value::array());

    Value nets = Value::array();
    Value pins = Value::array();
    pins.push(pin_ref("board", "board1", "GPIO4"));
    pins.push(pin_ref("board", "board2", "GPIO4"));
    nets.push(net_entry("n_bus", std::move(pins)));
    set(doc, "nets", std::move(nets));

    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));

    CHECK(engine.set_pin("board1", "GPIO4", Level::High, &error));
    CHECK(engine.set_pin("board2", "GPIO4", Level::Low, &error));

    const Value net_states = engine.nets_state();
    bool found_x = false;
    for (const auto& net : net_states.as_array()) {
        const Value* id = net.find("id");
        const Value* level = net.find("level");
        if (id && level && id->is_string() && id->as_string() == "n_bus") {
            found_x = level->as_string() == "x";
        }
    }
    CHECK(found_x);
}

TEST(engine_button_param_press) {
    Value doc = object();
    set(doc, "format", Value("simforge.project"));
    set(doc, "schema", Value(1));
    Value boards = Value::array();
    boards.push(board_entry("board1", "simforge.esp32s3.devkitc"));
    set(doc, "boards", std::move(boards));
    Value devices = Value::array();
    devices.push(device_entry("btn1", "simforge.button"));
    set(doc, "devices", std::move(devices));
    Value nets = Value::array();
    Value pins = Value::array();
    pins.push(pin_ref("board", "board1", "GPIO5"));
    pins.push(pin_ref("device", "btn1", "1"));
    nets.push(net_entry("n_btn", std::move(pins)));
    set(doc, "nets", std::move(nets));

    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));
    CHECK(engine.set_device_param("btn1", "pressed", Value(true), &error));
    if (!error.empty()) ST_FAIL(error);

    const Value state = engine.frame(0);
    bool pressed = false;
    for (const auto& item : state["devices"].as_array()) {
        const Value* id = item.find("id");
        if (id && id->is_string() && id->as_string() == "btn1") {
            const Value* p = item.find("pressed");
            pressed = p != nullptr && p->is_bool() && p->as_bool();
        }
    }
    CHECK(pressed);
}

TEST(engine_start_pause_resume_states) {
    Value doc = led_document();
    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));
    CHECK(engine.state() == EngineState::Ready);

    engine.start();
    CHECK(engine.state() == EngineState::Running);
    engine.pause();
    CHECK(engine.state() == EngineState::Paused);
    engine.resume();
    CHECK(engine.state() == EngineState::Running);
    engine.stop();
    CHECK(engine.state() == EngineState::Stopped);
}

TEST(engine_bench_queue_hooks_are_isolated) {
    // Os hooks de S0.1 mexem só na fila: tempo, hash e contadores não mudam,
    // senão a medição contaminaria o estado observável do motor.
    Value doc = led_document();
    Engine engine;
    std::string error;
    CHECK(engine.load(doc, &error));
    CHECK(engine.set_pin("board1", "GPIO2", Level::High, &error));
    engine.run_for(1'000'000);

    const SimTime now = engine.now();
    const std::uint64_t hash = engine.trace_hash();
    const std::size_t events = engine.event_count();
    CHECK(engine.state() == EngineState::Ready);

    engine.bench_enqueue(1000);
    CHECK_EQ(engine.bench_queue_size(), std::size_t(1000));
    CHECK_EQ(engine.bench_dequeue(400), std::size_t(400));
    CHECK_EQ(engine.bench_queue_size(), std::size_t(600));
    CHECK_EQ(engine.bench_dequeue(10'000), std::size_t(600));
    CHECK_EQ(engine.bench_queue_size(), std::size_t(0));

    CHECK_EQ(engine.now(), now);
    CHECK_EQ(engine.trace_hash(), hash);
    CHECK_EQ(engine.event_count(), events);
    CHECK(engine.state() == EngineState::Ready);
}

namespace {

/// Documento mínimo com um clock alimentando uma net.
Value clock_document(double freq_hz, double duty) {
    Value doc = object();
    set(doc, "format", Value("simforge.project"));
    set(doc, "schema", Value(1));
    set(doc, "boards", Value::array());

    Value params = object();
    set(params, "freq_hz", Value(freq_hz));
    set(params, "duty", Value(duty));

    Value device = object();
    set(device, "id", Value("clk1"));
    set(device, "type", Value("simforge.clock"));
    set(device, "params", std::move(params));

    Value devices = Value::array();
    devices.push(std::move(device));
    set(doc, "devices", std::move(devices));

    Value pins = Value::array();
    pins.push(pin_ref("device", "clk1", "OUT"));
    Value nets = Value::array();
    nets.push(net_entry("n_clk", std::move(pins)));
    set(doc, "nets", std::move(nets));
    return doc;
}

std::string net_level(const Engine& engine, const char* id) {
    const Value states = engine.nets_state();
    for (const auto& net : states.as_array()) {
        const Value* net_id = net.find("id");
        if (net_id && net_id->is_string() && net_id->as_string() == id) {
            const Value* level = net.find("level");
            return level && level->is_string() ? level->as_string() : "";
        }
    }
    return "";
}

}  // namespace

TEST(engine_clock_generates_edges) {
    Engine engine;
    std::string error;
    CHECK(engine.load(clock_document(1000.0, 0.5), &error));
    if (!error.empty()) ST_FAIL(error);

    // 1 kHz, 50%: alto em [0, 500 µs), baixo em [500 µs, 1 ms).
    CHECK_EQ(net_level(engine, "n_clk"), std::string("high"));
    engine.run_for(500'001);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("low"));
    engine.run_for(500'000);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("high"));
    engine.run_for(250'000);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("high"));
    engine.run_for(250'000);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("low"));
    CHECK(engine.event_count() >= 3);
}

TEST(engine_clock_respects_duty_cycle) {
    Engine engine;
    std::string error;
    CHECK(engine.load(clock_document(1000.0, 0.25), &error));

    CHECK_EQ(net_level(engine, "n_clk"), std::string("high"));
    engine.run_for(250'001);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("low"));
    engine.run_for(749'999);
    CHECK_EQ(net_level(engine, "n_clk"), std::string("high"));
    CHECK(engine.errors().empty());
}

TEST(engine_clock_is_deterministic) {
    Value doc = clock_document(2000.0, 0.3);
    Engine a;
    Engine b;
    std::string error;
    CHECK(a.load(doc, &error));
    CHECK(b.load(doc, &error));
    a.run_for(3'000'000);
    b.run_for(3'000'000);
    CHECK_EQ(a.trace_hash(), b.trace_hash());
    CHECK_EQ(a.event_count(), b.event_count());
    CHECK_EQ(a.now(), b.now());
    CHECK(a.event_count() >= 10);
}

