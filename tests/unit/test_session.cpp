#include <filesystem>
#include <string>

#include "framework.hpp"
#include "simforge/json.hpp"
#include "simforge/paths.hpp"
#include "simforge/session.hpp"

using namespace simforge;
using simforge::json::Value;

namespace {

Value object() { return Value::object(); }

Value make_request(const char* id, const std::string& method, Value params,
                   Value meta = Value()) {
    Value req = object();
    req.set("id", id);
    req.set("method", method);
    req.set("params", std::move(params));
    if (!meta.is_null()) req.set("meta", std::move(meta));
    return req;
}

Value call(Session& session, const std::string& method,
           Value params = Value::object()) {
    const std::string text =
        session.execute(make_request("t", method, std::move(params)).dump(0));
    json::ParseError err;
    Value response = json::parse(text, &err);
    if (!err.ok()) {
        ST_FAIL(std::string("resposta inválida: ") + err.message + " em " + text);
        return Value();
    }
    return response;
}

Value call_meta(Session& session, const std::string& method, Value params,
                Value meta) {
    const std::string text = session.execute(
        make_request("t", method, std::move(params), std::move(meta)).dump(0));
    json::ParseError err;
    return json::parse(text, &err);
}

bool ok(const Value& response) {
    const Value* v = response.find("ok");
    return v && v->is_bool() && v->as_bool();
}

std::string error_code(const Value& response) {
    const Value* error = response.find("error");
    if (!error) return {};
    const Value* code = error->find("code");
    return code && code->is_string() ? code->as_string() : std::string{};
}

const Value* result_of(const Value& response) { return response.find("result"); }

std::string first_board_id(Session& session) {
    Value state = call(session, "project.state");
    const Value* result = result_of(state);
    if (!result) return {};
    const Value* document = result->find("document");
    if (!document) return {};
    const Value* boards = document->find("boards");
    if (!boards || boards->size() == 0) return {};
    const Value* id = boards->at(0).find("id");
    return id && id->is_string() ? id->as_string() : std::string{};
}

/// Cria projeto vazio + LED e conecta GPIO2 -> A e GND -> K.
struct LedFixture {
    std::string board_id;
    std::string led_id;
};

LedFixture build_led_fixture(Session& session) {
    Value created = call(session, "project.new", Value::object());
    if (!ok(created)) ST_FAIL(created.dump());

    Value device_params = object();
    device_params.set("type", "simforge.led");
    Value device = call(session, "circuit.addDevice", std::move(device_params));
    if (!ok(device)) ST_FAIL(device.dump());
    const Value* device_result = result_of(device);
    LedFixture fixture;
    fixture.board_id = first_board_id(session);
    if (device_result) {
        const Value* id = device_result->find("device_id");
        if (id && id->is_string()) fixture.led_id = id->as_string();
    }

    const auto connect = [&](const char* board_pin, const char* device_pin) {
        Value pins = object();
        Value a = object();
        a.set("board", fixture.board_id);
        a.set("pin", board_pin);
        Value b = object();
        b.set("device", fixture.led_id);
        b.set("pin", device_pin);
        pins.set("a", std::move(a));
        pins.set("b", std::move(b));
        Value connected = call(session, "circuit.connect", std::move(pins));
        if (!ok(connected)) ST_FAIL(connected.dump());
    };
    connect("GPIO2", "A");
    connect("GND", "K");
    return fixture;
}

bool led_on(Session& session, const std::string& led_id) {
    Value frame = call(session, "sim.frame");
    const Value* result = result_of(frame);
    if (!result) return false;
    const Value* inner = result->find("frame");
    if (!inner) return false;
    const Value* devices = inner->find("devices");
    if (!devices) return false;
    for (const auto& device : devices->as_array()) {
        const Value* id = device.find("id");
        if (id && id->is_string() && id->as_string() == led_id) {
            const Value* on = device.find("on");
            return on && on->is_bool() && on->as_bool();
        }
    }
    return false;
}

}  // namespace

TEST(session_app_ping_and_info) {
    Session session;
    Value ping = call(session, "app.ping");
    CHECK(ok(ping));
    const Value* result = result_of(ping);
    CHECK(result != nullptr);
    if (result) CHECK(result->find("t_ns") != nullptr);

    Value info = call(session, "app.info");
    CHECK(ok(info));
    if (const Value* r = result_of(info)) {
        CHECK_EQ(r->find("product")->as_string(), std::string("SimForge"));
        CHECK_EQ(r->find("api_version")->as_int64(), std::int64_t(1));
        const Value* caps = r->find("capabilities");
        CHECK(caps != nullptr && caps->is_array() && caps->size() >= 1);
    }
}

TEST(session_unknown_and_invalid_methods) {
    Session session;
    CHECK_EQ(error_code(call(session, "nao.existe")), std::string("unknown_method"));
    CHECK_EQ(error_code(call(session, "Ping")), std::string("invalid_request"));
    CHECK_EQ(error_code(call(session, "app")), std::string("invalid_request"));
    CHECK_EQ(error_code(call(session, "app.")), std::string("invalid_request"));
}

TEST(session_invalid_json_envelope) {
    Session session;
    json::ParseError err;
    Value response = json::parse(session.execute("{ isto nao e json }"), &err);
    CHECK(err.ok());
    CHECK(!ok(response));
    CHECK_EQ(error_code(response), std::string("invalid_request"));

    json::ParseError err2;
    Value not_object = json::parse(session.execute("[1,2,3]"), &err2);
    CHECK(err2.ok());
    CHECK(!ok(not_object));
    CHECK_EQ(error_code(not_object), std::string("invalid_request"));
}

TEST(session_project_lifecycle) {
    Session session;
    Value created = call(session, "project.new");
    CHECK(ok(created));
    if (const Value* r = result_of(created)) {
        CHECK(r->find("project_id") != nullptr);
        CHECK_EQ(r->find("revision")->as_int64(), std::int64_t(1));
    }

    Value state = call(session, "project.state");
    CHECK(ok(state));
    if (const Value* r = result_of(state)) {
        CHECK(r->find("document") != nullptr);
        CHECK_EQ(r->find("dirty")->as_bool(), true);
        CHECK_EQ(r->find("revision")->as_int64(), std::int64_t(1));
    }

    Value validated = call(session, "project.validate");
    CHECK(ok(validated));
    if (const Value* r = result_of(validated)) {
        const Value* diagnostics = r->find("diagnostics");
        CHECK(diagnostics != nullptr && diagnostics->is_array());
        CHECK_EQ(diagnostics->size(), std::size_t(0));
    }
}

TEST(session_project_close_requires_discard) {
    Session session;
    CHECK(ok(call(session, "project.new")));
    CHECK_EQ(error_code(call(session, "project.close")), std::string("conflict"));

    Value discard = object();
    discard.set("discard", true);
    CHECK(ok(call(session, "project.close", std::move(discard))));
    // Sem projeto, project.state falha.
    CHECK_EQ(error_code(call(session, "project.state")), std::string("invalid_state"));
}

TEST(session_circuit_build_and_erc) {
    Session session;
    const LedFixture fixture = build_led_fixture(session);
    CHECK(!fixture.board_id.empty());
    CHECK(!fixture.led_id.empty());

    Value erc = call(session, "circuit.erc");
    CHECK(ok(erc));
    if (const Value* r = result_of(erc)) {
        const Value* clean = r->find("clean");
        CHECK(clean != nullptr && clean->is_bool() && clean->as_bool());
    }
}

TEST(session_circuit_rejects_unknown_device) {
    Session session;
    CHECK(ok(call(session, "project.new")));
    Value params = object();
    params.set("type", "simforge.inexistente");
    CHECK_EQ(error_code(call(session, "circuit.addDevice", std::move(params))),
             std::string("invalid_params"));
}

TEST(session_circuit_connect_conflict) {
    Session session;
    const LedFixture fixture = build_led_fixture(session);

    // Resistor conectado a outra net (GPIO4). Conectar GPIO2 -> resistor.A
    // deve conflitar porque os dois pinos estão em nets diferentes.
    Value device_params = object();
    device_params.set("type", "simforge.resistor");
    Value resistor = call(session, "circuit.addDevice", std::move(device_params));
    CHECK(ok(resistor));
    std::string resistor_id;
    if (const Value* r = result_of(resistor)) {
        const Value* id = r->find("device_id");
        if (id && id->is_string()) resistor_id = id->as_string();
    }

    Value first = object();
    Value a1 = object();
    a1.set("board", fixture.board_id);
    a1.set("pin", "GPIO4");
    Value b1 = object();
    b1.set("device", resistor_id);
    b1.set("pin", "A");
    first.set("a", std::move(a1));
    first.set("b", std::move(b1));
    CHECK(ok(call(session, "circuit.connect", std::move(first))));

    Value pins = object();
    Value a = object();
    a.set("board", fixture.board_id);
    a.set("pin", "GPIO2");
    Value b = object();
    b.set("device", resistor_id);
    b.set("pin", "A");
    pins.set("a", std::move(a));
    pins.set("b", std::move(b));
    CHECK_EQ(error_code(call(session, "circuit.connect", std::move(pins))),
             std::string("conflict"));
}

TEST(session_base_revision_enforced) {
    Session session;
    CHECK(ok(call(session, "project.new")));
    Value params = object();
    params.set("type", "simforge.resistor");
    params.set("base_revision", 99);
    CHECK_EQ(error_code(call(session, "circuit.addDevice", std::move(params))),
             std::string("conflict"));
}

TEST(session_idempotency_key_replays) {
    Session session;
    Value meta = object();
    meta.set("api_version", 1);
    meta.set("idempotency_key", "novo-1");

    Value first = call_meta(session, "project.new", Value::object(), meta);
    Value second = call_meta(session, "project.new", Value::object(), meta);
    CHECK(ok(first));
    CHECK(ok(second));
    if (result_of(first) && result_of(second)) {
        CHECK_EQ(result_of(first)->find("project_id")->as_string(),
                 result_of(second)->find("project_id")->as_string());
    }
}

TEST(session_sim_load_requires_project) {
    Session session;
    CHECK_EQ(error_code(call(session, "sim.load")), std::string("invalid_state"));
}

TEST(session_sim_led_flow) {
    Session session;
    const LedFixture fixture = build_led_fixture(session);

    Value loaded = call(session, "sim.load");
    CHECK(ok(loaded));
    if (!ok(loaded)) {
        ST_FAIL(loaded.dump());
        return;
    }
    if (const Value* r = result_of(loaded)) {
        const Value* state = r->find("state");
        CHECK(state != nullptr && state->is_string());
    }

    CHECK(!led_on(session, fixture.led_id));

    Value pin = object();
    Value target = object();
    target.set("board", fixture.board_id);
    target.set("pin", "GPIO2");
    pin.set("target", std::move(target));
    pin.set("level", "high");
    Value driven = call(session, "sim.debug.setPin", std::move(pin));
    CHECK(ok(driven));
    if (!ok(driven)) ST_FAIL(driven.dump());

    CHECK(led_on(session, fixture.led_id));

    Value release = object();
    Value target2 = object();
    target2.set("board", fixture.board_id);
    target2.set("pin", "GPIO2");
    release.set("target", std::move(target2));
    CHECK(ok(call(session, "sim.debug.releasePin", std::move(release))));
    CHECK(!led_on(session, fixture.led_id));
}

TEST(session_sim_states_and_step) {
    Session session;
    build_led_fixture(session);
    CHECK(ok(call(session, "sim.load")));

    Value run = call(session, "sim.run");
    CHECK(ok(run));
    if (const Value* r = result_of(run)) {
        CHECK_EQ(r->find("state")->as_string(), std::string("running"));
    }
    CHECK(ok(call(session, "sim.pause")));
    CHECK(ok(call(session, "sim.resume")));
    CHECK(ok(call(session, "sim.stop")));

    Value step = call(session, "sim.reset");
    CHECK(ok(step));
    if (const Value* r = result_of(step)) {
        CHECK_EQ(r->find("t_ns")->as_int64(), std::int64_t(0));
    }
}

TEST(session_events_poll) {
    Session session;
    CHECK(ok(call(session, "project.new")));

    Value poll = call(session, "events.poll");
    CHECK(ok(poll));
    if (const Value* r = result_of(poll)) {
        const Value* events = r->find("events");
        CHECK(events != nullptr && events->is_array());
        CHECK(events->size() >= 1);  // project.changed
        CHECK(r->find("next_seq") != nullptr);
        CHECK(r->find("dropped") != nullptr);
        CHECK_EQ(r->find("dropped")->as_int64(), std::int64_t(0));
    }
}

TEST(session_catalogs) {
    Session session;
    Value devices = call(session, "device.catalog");
    CHECK(ok(devices));
    bool has_led = false;
    if (const Value* r = result_of(devices)) {
        const Value* list = r->find("devices");
        CHECK(list != nullptr && list->is_array());
        for (const auto& d : list->as_array()) {
            const Value* type = d.find("type");
            if (type && type->is_string() && type->as_string() == "simforge.led") {
                has_led = true;
            }
        }
    }
    CHECK(has_led);

    Value boards = call(session, "board.catalog");
    CHECK(ok(boards));
    bool has_board = false;
    if (const Value* r = result_of(boards)) {
        const Value* list = r->find("boards");
        CHECK(list != nullptr && list->is_array());
        for (const auto& b : list->as_array()) {
            const Value* type = b.find("type");
            if (type && type->is_string() &&
                type->as_string() == "simforge.esp32s3.devkitc") {
                has_board = true;
                const Value* pins = b.find("pins");
                CHECK(pins != nullptr && pins->size() > 0);
            }
        }
    }
    CHECK(has_board);
}

TEST(session_project_save_and_reopen) {
    Session session;
    build_led_fixture(session);

    const auto path =
        std::filesystem::temp_directory_path() / "simforge_session_save.sforge";
    Value save_params = object();
    save_params.set("path", path.string());
    Value saved = call(session, "project.save", std::move(save_params));
    CHECK(ok(saved));
    if (!ok(saved)) ST_FAIL(saved.dump());
    CHECK(std::filesystem::exists(path));

    Session other;
    Value open_params = object();
    open_params.set("path", path.string());
    Value opened = call(other, "project.open", std::move(open_params));
    CHECK(ok(opened));
    if (const Value* r = result_of(opened)) {
        const Value* diagnostics = r->find("diagnostics");
        CHECK(diagnostics != nullptr && diagnostics->is_array());
    }

    std::filesystem::remove(path);
}

TEST(session_project_opens_from_utf8_path) {
    // O caminho chega em UTF-8 pela API, mas a pasta real tem acento (por
    // exemplo "Área de Trabalho"). No Windows o caminho precisa virar wchar_t,
    // senão o arquivo nunca é encontrado.
    Session session;
    build_led_fixture(session);

    const auto dir = std::filesystem::temp_directory_path() /
                     std::filesystem::path(std::u8string(u8"simforge-teste-ação"));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    CHECK(!ec);
    const auto path = dir / "acentuado.sforge";
    const std::string utf8_path = utf8_from_path(path);

    Value save_params = object();
    save_params.set("path", utf8_path);
    Value saved = call(session, "project.save", std::move(save_params));
    CHECK(ok(saved));
    if (!ok(saved)) ST_FAIL(saved.dump());
    CHECK(std::filesystem::exists(path));

    Session other;
    Value open_params = object();
    open_params.set("path", utf8_path);
    Value opened = call(other, "project.open", std::move(open_params));
    CHECK(ok(opened));
    if (!ok(opened)) ST_FAIL(opened.dump());

    std::filesystem::remove_all(dir, ec);
    CHECK(!ec);
}

TEST(session_meta_api_version_mismatch) {
    Session session;
    Value meta = object();
    meta.set("api_version", 2);
    Value response = call_meta(session, "app.ping", Value::object(), std::move(meta));
    CHECK(!ok(response));
    CHECK_EQ(error_code(response), std::string("schema_unsupported"));
}
