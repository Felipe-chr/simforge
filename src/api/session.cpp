#include "simforge/session.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "simforge/board.hpp"
#include "simforge/circuit.hpp"
#include "simforge/device.hpp"
#include "simforge/engine.hpp"
#include "simforge/ids.hpp"
#include "simforge/paths.hpp"
#include "simforge/project.hpp"

namespace simforge {
namespace {

using json::Value;

json::Value obj() { return Value::object(); }
json::Value arr() { return Value::array(); }

/// Erro de comando com código estável (api-v1 §6).
struct CommandError {
    std::string code;
    std::string message;
    Value details;
    bool retryable = false;

    CommandError(std::string c, std::string m, Value d = Value::object(),
                 bool r = false)
        : code(std::move(c)),
          message(std::move(m)),
          details(std::move(d)),
          retryable(r) {}
};

[[noreturn]] void fail(const char* code, std::string message,
                       Value details = Value::object(), bool retryable = false) {
    throw CommandError(code, std::move(message), std::move(details), retryable);
}

bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool valid_method(std::string_view m) {
    if (m.empty() || !(m[0] >= 'a' && m[0] <= 'z')) return false;
    bool dot = false;
    for (std::size_t i = 1; i < m.size(); ++i) {
        const char c = m[i];
        if (c == '.') {
            if (i + 1 >= m.size() || !is_letter(m[i + 1])) return false;
            dot = true;
        } else if (!(is_letter(c) || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return dot;
}

const Value* opt(const Value& params, const char* key) { return params.find(key); }

std::string get_string(const Value& params, const char* key,
                       const std::string& fallback) {
    const Value* v = opt(params, key);
    if (!v || v->is_null()) return fallback;
    if (!v->is_string()) fail("invalid_params", std::string("'") + key + "' deve ser string");
    return v->as_string();
}

std::string require_string(const Value& params, const char* key) {
    const Value* v = opt(params, key);
    if (!v || !v->is_string()) {
        fail("invalid_params", std::string("'") + key + "' ausente ou não é string");
    }
    return v->as_string();
}

std::int64_t get_int(const Value& params, const char* key, std::int64_t fallback) {
    const Value* v = opt(params, key);
    if (!v || v->is_null()) return fallback;
    if (!v->is_number()) fail("invalid_params", std::string("'") + key + "' deve ser número");
    return v->as_int64();
}

bool get_bool(const Value& params, const char* key, bool fallback) {
    const Value* v = opt(params, key);
    if (!v || v->is_null()) return fallback;
    if (!v->is_bool()) fail("invalid_params", std::string("'") + key + "' deve ser booleano");
    return v->as_bool();
}

struct PinIdent {
    std::string owner;
    std::string pin;
    bool is_board = false;
};

PinIdent parse_pin(const Value& v) {
    if (!v.is_object()) fail("invalid_params", "pin deve ser objeto");
    const Value* pin = v.find("pin");
    if (!pin || !pin->is_string()) fail("invalid_params", "pin sem 'pin'");
    PinIdent out;
    out.pin = pin->as_string();
    const Value* b = v.find("board");
    const Value* d = v.find("device");
    if (b && b->is_string()) {
        out.owner = b->as_string();
        out.is_board = true;
    } else if (d && d->is_string()) {
        out.owner = d->as_string();
        out.is_board = false;
    } else {
        fail("invalid_params", "pin precisa de 'board' ou 'device'");
    }
    return out;
}

Value pin_to_value(const PinIdent& pin) {
    Value v = obj();
    v.set(pin.is_board ? "board" : "device", pin.owner);
    v.set("pin", pin.pin);
    return v;
}

bool same_pin(const Value& ref, const PinIdent& pin) {
    const Value* b = ref.find("board");
    const Value* d = ref.find("device");
    const Value* p = ref.find("pin");
    if (!p || !p->is_string() || p->as_string() != pin.pin) return false;
    const bool is_board = b != nullptr;
    if (is_board != pin.is_board) return false;
    const Value* owner = is_board ? b : d;
    return owner && owner->is_string() && owner->as_string() == pin.owner;
}

Value* find_net_by_id(Value& doc, const std::string& id) {
    Value* nets = doc.find("nets");
    if (!nets || !nets->is_array()) return nullptr;
    for (auto& net : nets->as_array()) {
        const Value* nid = net.find("id");
        if (nid && nid->is_string() && nid->as_string() == id) return &net;
    }
    return nullptr;
}

Value* find_net_of_pin(Value& doc, const PinIdent& pin, std::string* out_id) {
    Value* nets = doc.find("nets");
    if (!nets || !nets->is_array()) return nullptr;
    for (auto& net : nets->as_array()) {
        Value* pins = net.find("pins");
        if (!pins || !pins->is_array()) continue;
        for (const auto& ref : pins->as_array()) {
            if (!same_pin(ref, pin)) continue;
            if (out_id) {
                const Value* nid = net.find("id");
                if (nid && nid->is_string()) *out_id = nid->as_string();
            }
            return &net;
        }
    }
    return nullptr;
}

void attach_pin(Value& doc, const std::string& net_id, const PinIdent& pin) {
    Value* existing = find_net_of_pin(doc, pin, nullptr);
    if (existing) {
        const Value* nid = existing->find("id");
        if (nid && nid->is_string() && nid->as_string() == net_id) return;
        Value details = obj();
        Value pins = arr();
        pins.push(pin_to_value(pin));
        details.set("pins", std::move(pins));
        fail("conflict", "pino já conectado a outra net", std::move(details));
    }
    Value* net = find_net_by_id(doc, net_id);
    if (!net) fail("not_found", "net inexistente: " + net_id);
    Value* pins = net->find("pins");
    if (!pins || !pins->is_array()) {
        net->set("pins", arr());
        pins = net->find("pins");
    }
    pins->push(pin_to_value(pin));
}

bool detach_pin(Value& doc, const PinIdent& pin) {
    bool removed = false;
    Value* nets = doc.find("nets");
    if (!nets || !nets->is_array()) return false;
    for (auto& net : nets->as_array()) {
        Value* pins = net.find("pins");
        if (!pins || !pins->is_array()) continue;
        auto& list = pins->as_array();
        for (auto it = list.begin(); it != list.end();) {
            if (same_pin(*it, pin)) {
                it = list.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
    }
    return removed;
}

bool owner_exists(const Value& doc, const PinIdent& pin) {
    const char* array_name = pin.is_board ? "boards" : "devices";
    const Value* list = doc.find(array_name);
    if (!list || !list->is_array()) return false;
    for (const auto& item : list->as_array()) {
        const Value* id = item.find("id");
        if (id && id->is_string() && id->as_string() == pin.owner) return true;
    }
    return false;
}

Value diagnostics_to_json(const std::vector<Diagnostic>& diagnostics) {
    Value out = arr();
    for (const auto& d : diagnostics) out.push(d.to_json());
    return out;
}

Value make_position(const Value& params) {
    const Value* pos = params.find("position");
    if (pos && pos->is_object()) return *pos;
    Value p = obj();
    p.set("x", 0);
    p.set("y", 0);
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// Implementação
// ---------------------------------------------------------------------------

struct Session::Impl {
    using Handler = std::function<Value(Impl&, const Value&)>;

    mutable std::mutex mutex;
    std::unordered_map<std::string, Handler> handlers;

    // Projeto
    Project project;
    bool has_project = false;
    std::filesystem::path path;
    bool dirty = false;

    // Simulação
    Engine engine;
    bool engine_loaded = false;
    std::uint64_t engine_revision = 0;
    std::string session_id;
    double speed = 1.0;
    std::uint64_t frame_seq = 0;

    // Configuração
    Value settings = Value::object();

    // Eventos: deque porque o descarte é no início (O(1)); vetor custaria
    // O(n) por evento descartado.
    std::deque<Value> events;
    std::uint64_t next_event_seq = 1;
    std::uint64_t dropped = 0;
    static constexpr std::size_t kMaxEvents = 10000;

    // Idempotência (escopo de sessão + método)
    std::unordered_map<std::string, Value> idempotency;

    std::string last_error;

    Impl() { register_handlers(); }

    // --- eventos ----------------------------------------------------------
    void trim_events() {
        while (events.size() > kMaxEvents) {
            events.pop_front();
            ++dropped;
        }
    }

    void emit(const char* type, Value payload) {
        Value ev = obj();
        ev.set("seq", static_cast<long long>(next_event_seq++));
        ev.set("t_ns", static_cast<long long>(engine.now()));
        ev.set("type", type);
        ev.set("payload", std::move(payload));
        events.push_back(std::move(ev));
        trim_events();
    }

    EventBatch poll(std::uint64_t after_seq, std::size_t max) {
        EventBatch batch;
        batch.dropped = dropped;
        batch.next_seq = after_seq + 1;
        if (max == 0) max = 256;
        for (const auto& ev : events) {
            const Value* seq = ev.find("seq");
            if (!seq || !seq->is_number()) continue;
            const std::uint64_t s = static_cast<std::uint64_t>(seq->as_int64());
            if (s <= after_seq) continue;
            if (batch.events.size() >= max) break;
            batch.events.push_back(ev);
            batch.next_seq = s + 1;
        }
        if (batch.events.empty()) batch.next_seq = after_seq + 1;
        return batch;
    }

    // --- projeto ----------------------------------------------------------
    void require_project() {
        if (!has_project) fail("invalid_state", "nenhum projeto aberto");
    }

    void touch(const char* kind) {
        project.bump_revision();
        dirty = true;
        Value payload = obj();
        payload.set("revision", static_cast<long long>(project.revision()));
        Value kinds = arr();
        kinds.push(Value(kind));
        payload.set("kinds", std::move(kinds));
        emit("project.changed", std::move(payload));
    }

    void check_base_revision(const Value& params) {
        if (!has_project) return;
        const Value* base = opt(params, "base_revision");
        if (!base || base->is_null()) return;
        if (!base->is_number()) fail("invalid_params", "base_revision deve ser número");
        const std::uint64_t expected =
            static_cast<std::uint64_t>(base->as_int64());
        if (expected != project.revision()) {
            Value details = obj();
            details.set("expected", static_cast<long long>(expected));
            details.set("current", static_cast<long long>(project.revision()));
            fail("conflict", "A revisão do projeto mudou.", std::move(details));
        }
    }

    void invalidate_engine() { engine_loaded = false; }

    Value project_state() {
        Value state = obj();
        state.set("document", project.document());
        state.set("revision", static_cast<long long>(project.revision()));
        state.set("path", path.string());
        state.set("dirty", dirty);
        return state;
    }

    // --- simulação --------------------------------------------------------
    void ensure_engine() {
        if (!engine_loaded) {
            fail("invalid_state", "simulação não carregada; execute sim.load");
        }
        if (engine_revision != project.revision()) {
            fail("invalid_state", "projeto mudou; execute sim.load novamente");
        }
        if (engine.state() == EngineState::Error) {
            Value details = obj();
            Value errs = arr();
            for (const auto& e : engine.errors()) {
                Value item = obj();
                item.set("code", e.code);
                item.set("message", e.message);
                item.set("details", e.details);
                errs.push(std::move(item));
            }
            details.set("errors", std::move(errs));
            fail("sim_oscillation", "a simulação entrou em estado de erro",
                 std::move(details));
        }
    }

    Value sim_state() {
        Value state = obj();
        state.set("state", to_string(engine.state()));
        state.set("t_ns", static_cast<long long>(engine.now()));
        state.set("speed", speed);
        state.set("revision", static_cast<long long>(project.revision()));
        state.set("runtime", "native");
        Value stats = obj();
        stats.set("events", static_cast<long long>(engine.event_count()));
        stats.set("trace_hash", static_cast<long long>(engine.trace_hash()));
        state.set("stats", std::move(stats));
        return state;
    }

    void load_engine() {
        require_project();
        const auto diagnostics = project.validate();
        if (Project::has_errors(diagnostics)) {
            Value details = obj();
            details.set("diagnostics", diagnostics_to_json(diagnostics));
            fail("project_invalid", "o projeto tem erros de validação",
                 std::move(details));
        }
        std::string error;
        if (!engine.load(project.document(), &error)) {
            fail("project_invalid", error);
        }
        if (engine.state() == EngineState::Error) {
            Value details = obj();
            Value errs = arr();
            for (const auto& e : engine.errors()) {
                Value item = obj();
                item.set("code", e.code);
                item.set("message", e.message);
                item.set("details", e.details);
                errs.push(std::move(item));
            }
            details.set("errors", std::move(errs));
            fail("sim_oscillation",
                 "realimentação combinacional não convergiu", std::move(details));
        }
        invalidate_engine();
        engine_loaded = true;
        engine_revision = project.revision();
        session_id = new_ulid();
        Value payload = obj();
        payload.set("state", to_string(engine.state()));
        payload.set("t_ns", static_cast<long long>(engine.now()));
        payload.set("revision", static_cast<long long>(project.revision()));
        emit("sim.state", std::move(payload));
    }

    // --- dispatch ---------------------------------------------------------
    Value dispatch(const Value& request);

    void register_handlers();
};

Value Session::Impl::dispatch(const Value& request) {
    Value response = obj();
    std::string id;
    std::string method;

    try {
        if (!request.is_object()) {
            fail("invalid_request", "envelope deve ser um objeto");
        }
        const Value* idv = request.find("id");
        if (idv && idv->is_string()) id = idv->as_string();
        response.set("id", id);

        const Value* mv = request.find("method");
        if (!mv || !mv->is_string()) fail("invalid_request", "campo 'method' ausente");
        method = mv->as_string();
        if (!valid_method(method)) {
            fail("invalid_request", "método inválido: " + method);
        }

        const Value* params = request.find("params");
        Value empty_params = Value::object();
        const Value& p =
            (params && params->is_object()) ? *params : empty_params;

        const Value* meta = request.find("meta");
        std::string idempotency_key;
        if (meta && meta->is_object()) {
            const Value* api = meta->find("api_version");
            if (api && api->is_number() && api->as_int64() != 1) {
                fail("schema_unsupported", "api_version incompatível");
            }
            const Value* key = meta->find("idempotency_key");
            if (key && key->is_string()) idempotency_key = key->as_string();
        }
        if (!idempotency_key.empty()) {
            const auto cached = idempotency.find(method + "|" + idempotency_key);
            if (cached != idempotency.end()) {
                Value cached_response = cached->second;
                cached_response.set("id", id);
                return cached_response;
            }
        }

        const auto it = handlers.find(method);
        if (it == handlers.end()) {
            fail("unknown_method", "método não registrado: " + method);
        }

        Value result = it->second(*this, p);

        response.set("ok", true);
        response.set("result", std::move(result));
        Value rmeta = obj();
        if (has_project) {
            rmeta.set("revision", static_cast<long long>(project.revision()));
        }
        rmeta.set("t_ns", static_cast<long long>(engine.now()));
        response.set("meta", std::move(rmeta));

        if (!idempotency_key.empty()) {
            idempotency[method + "|" + idempotency_key] = response;
        }
    } catch (const CommandError& e) {
        response.set("ok", false);
        Value error = obj();
        error.set("code", e.code);
        error.set("message", e.message);
        if (!e.details.is_null()) error.set("details", e.details);
        error.set("retryable", e.retryable);
        response.set("error", std::move(error));
        last_error = e.code + ": " + e.message;
    } catch (const json::TypeError& e) {
        response.set("ok", false);
        Value error = obj();
        error.set("code", "invalid_params");
        error.set("message", e.what());
        error.set("retryable", false);
        response.set("error", std::move(error));
        last_error = std::string("invalid_params: ") + e.what();
    } catch (const std::exception& e) {
        response.set("ok", false);
        Value error = obj();
        error.set("code", "internal_error");
        error.set("message", e.what());
        error.set("retryable", true);
        response.set("error", std::move(error));
        last_error = std::string("internal_error: ") + e.what();
    }
    return response;
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

void Session::Impl::register_handlers() {
    // --- app.* ------------------------------------------------------------
    handlers["app.ping"] = [](Impl& impl, const Value& params) {
        Value out = obj();
        const Value* nonce = opt(params, "nonce");
        if (nonce) out.set("nonce", *nonce);
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["app.info"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("product", "SimForge");
        out.set("version", Session::product_version());
        out.set("api_version", Session::api_version());
        out.set("engine_version", Session::engine_version());
#if defined(_WIN32)
        out.set("platform", "windows-x64");
#elif defined(__APPLE__)
        out.set("platform", "macos");
#else
        out.set("platform", "linux-x64");
#endif
        Value caps = arr();
        caps.push(Value("c_api"));
        caps.push(Value("cli"));
        caps.push(Value("sim.logic"));
        out.set("capabilities", std::move(caps));
        return out;
    };
    handlers["app.authenticate"] = [](Impl& impl, const Value& params) {
        (void)require_string(params, "token");
        if (impl.session_id.empty()) impl.session_id = new_ulid();
        Value out = obj();
        out.set("session_id", impl.session_id);
        Value perms = arr();
        perms.push(Value("*"));
        out.set("permissions", std::move(perms));
        return out;
    };
    handlers["app.settings.get"] = [](Impl& impl, const Value& params) {
        Value values = obj();
        const Value* key = opt(params, "key");
        if (key && key->is_string()) {
            const Value* v = impl.settings.find(key->as_string());
            if (v) values.set(key->as_string(), *v);
        } else {
            values = impl.settings;
        }
        Value out = obj();
        out.set("values", std::move(values));
        return out;
    };
    handlers["app.settings.set"] = [](Impl& impl, const Value& params) {
        const Value* values = opt(params, "values");
        if (!values || !values->is_object()) {
            fail("invalid_params", "'values' deve ser objeto");
        }
        for (const auto& [k, v] : values->as_object()) impl.settings.set(k, v);
        Value out = obj();
        out.set("values", impl.settings);
        return out;
    };
    handlers["app.shutdown"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("ok", true);
        return out;
    };

    // --- project.* --------------------------------------------------------
    handlers["project.new"] = [](Impl& impl, const Value& params) {
        const std::string name = get_string(params, "name", "Sem titulo");
        const std::string board =
            get_string(params, "board", "simforge.esp32s3.devkitc");
        std::string error;
        Project created = Project::create(name, board, &error);
        if (!error.empty()) fail("invalid_params", error);
        impl.project = std::move(created);
        impl.has_project = true;
        impl.path.clear();
        impl.dirty = true;
        impl.invalidate_engine();
        Value changed = obj();
        changed.set("revision", static_cast<long long>(impl.project.revision()));
        Value kinds = arr();
        kinds.push(Value("project"));
        changed.set("kinds", std::move(kinds));
        impl.emit("project.changed", std::move(changed));
        Value out = obj();
        out.set("project_id", impl.project.id());
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["project.open"] = [](Impl& impl, const Value& params) {
        const std::string p = require_string(params, "path");
        std::string error;
        auto loaded = Project::load(path_from_utf8(p), &error);
        if (!loaded) fail("project_invalid", error);
        impl.project = std::move(*loaded);
        impl.has_project = true;
        impl.path = path_from_utf8(p);
        impl.dirty = false;
        impl.invalidate_engine();
        const auto diagnostics = impl.project.validate();
        Value out = obj();
        out.set("project_id", impl.project.id());
        out.set("revision", static_cast<long long>(impl.project.revision()));
        out.set("name", impl.project.name());
        out.set("diagnostics", diagnostics_to_json(diagnostics));
        return out;
    };
    handlers["project.save"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        const std::string p = get_string(params, "path", "");
        if (!p.empty()) impl.path = path_from_utf8(p);
        if (impl.path.empty()) fail("io_error", "nenhum caminho definido para salvar");
        std::string error;
        if (!impl.project.save(impl.path, &error)) fail("io_error", error);
        impl.dirty = false;
        Value out = obj();
        out.set("path", utf8_from_path(impl.path));
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["project.saveAs"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.path = path_from_utf8(require_string(params, "path"));
        std::string error;
        if (!impl.project.save(impl.path, &error)) fail("io_error", error);
        impl.dirty = false;
        Value out = obj();
        out.set("path", utf8_from_path(impl.path));
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["project.close"] = [](Impl& impl, const Value& params) {
        const bool discard = get_bool(params, "discard", false);
        if (impl.has_project && impl.dirty && !discard) {
            fail("conflict", "há alterações não salvas");
        }
        impl.has_project = false;
        impl.path.clear();
        impl.dirty = false;
        impl.invalidate_engine();
        Value out = obj();
        out.set("closed", true);
        return out;
    };
    handlers["project.state"] = [](Impl& impl, const Value&) {
        impl.require_project();
        return impl.project_state();
    };
    handlers["project.validate"] = [](Impl& impl, const Value&) {
        impl.require_project();
        Value out = obj();
        out.set("diagnostics", diagnostics_to_json(impl.project.validate()));
        return out;
    };
    handlers["project.recent"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("items", arr());
        return out;
    };

    // --- circuit.* --------------------------------------------------------
    handlers["circuit.addBoard"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string type = require_string(params, "type");
        const DeviceType* info = DeviceRegistry::instance().find(type);
        if (!info || info->category != "board") {
            fail("invalid_params", "tipo de placa desconhecido: " + type);
        }
        Value& doc = impl.project.document();
        Value* boards = doc.find("boards");
        if (!boards || !boards->is_array()) {
            doc.set("boards", arr());
            boards = doc.find("boards");
        }
        const std::string id = get_string(params, "id", new_ulid());
        Value board = obj();
        board.set("id", id);
        board.set("type", type);
        board.set("type_version", 1);
        board.set("position", make_position(params));
        board.set("rotation", static_cast<long long>(get_int(params, "rotation", 0)));
        const Value* p = opt(params, "params");
        board.set("params", p && p->is_object() ? *p : obj());
        board.set("pins", obj());
        boards->push(std::move(board));
        impl.touch("boards");
        Value out = obj();
        out.set("board_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.removeBoard"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "board_id");
        Value& doc = impl.project.document();
        Value* boards = doc.find("boards");
        if (!boards || !boards->is_array()) fail("not_found", "placa inexistente: " + id);
        auto& list = boards->as_array();
        bool removed = false;
        for (auto it = list.begin(); it != list.end(); ++it) {
            const Value* bid = it->find("id");
            if (bid && bid->is_string() && bid->as_string() == id) {
                list.erase(it);
                removed = true;
                break;
            }
        }
        if (!removed) fail("not_found", "placa inexistente: " + id);
        PinIdent pin;
        pin.owner = id;
        pin.is_board = true;
        pin.pin.clear();
        // Remove referências da placa em todas as nets.
        Value* nets = doc.find("nets");
        if (nets && nets->is_array()) {
            for (auto& net : nets->as_array()) {
                Value* pins = net.find("pins");
                if (!pins || !pins->is_array()) continue;
                auto& refs = pins->as_array();
                for (auto it = refs.begin(); it != refs.end();) {
                    const Value* b = it->find("board");
                    if (b && b->is_string() && b->as_string() == id) {
                        it = refs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        impl.touch("boards");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.addDevice"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string type = require_string(params, "type");
        if (!DeviceRegistry::instance().find(type)) {
            fail("invalid_params", "tipo de dispositivo desconhecido: " + type);
        }
        Value& doc = impl.project.document();
        Value* devices = doc.find("devices");
        if (!devices || !devices->is_array()) {
            doc.set("devices", arr());
            devices = doc.find("devices");
        }
        const std::string id = get_string(params, "id", new_ulid());
        Value device = obj();
        device.set("id", id);
        device.set("type", type);
        device.set("type_version", 1);
        device.set("position", make_position(params));
        device.set("rotation", static_cast<long long>(get_int(params, "rotation", 0)));
        const Value* p = opt(params, "params");
        device.set("params", p && p->is_object() ? *p : obj());
        device.set("pins", obj());
        devices->push(std::move(device));
        impl.touch("devices");
        Value out = obj();
        out.set("device_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.removeDevice"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "device_id");
        Value& doc = impl.project.document();
        Value* devices = doc.find("devices");
        if (!devices || !devices->is_array()) {
            fail("not_found", "dispositivo inexistente: " + id);
        }
        auto& list = devices->as_array();
        bool removed = false;
        for (auto it = list.begin(); it != list.end(); ++it) {
            const Value* did = it->find("id");
            if (did && did->is_string() && did->as_string() == id) {
                list.erase(it);
                removed = true;
                break;
            }
        }
        if (!removed) fail("not_found", "dispositivo inexistente: " + id);
        Value* nets = doc.find("nets");
        if (nets && nets->is_array()) {
            for (auto& net : nets->as_array()) {
                Value* pins = net.find("pins");
                if (!pins || !pins->is_array()) continue;
                auto& refs = pins->as_array();
                for (auto it = refs.begin(); it != refs.end();) {
                    const Value* d = it->find("device");
                    if (d && d->is_string() && d->as_string() == id) {
                        it = refs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        impl.touch("devices");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.setDevice"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "device_id");
        Value* obj_ptr = impl.project.find_object("devices", id);
        if (!obj_ptr) fail("not_found", "dispositivo inexistente: " + id);
        if (const Value* pos = opt(params, "position"); pos && pos->is_object()) {
            obj_ptr->set("position", *pos);
        }
        if (const Value* rot = opt(params, "rotation"); rot && rot->is_number()) {
            obj_ptr->set("rotation", *rot);
        }
        if (const Value* p = opt(params, "params"); p && p->is_object()) {
            const bool merge = get_bool(params, "merge", false);
            Value* current = obj_ptr->find("params");
            if (merge && current && current->is_object()) {
                for (const auto& [k, v] : p->as_object()) current->set(k, v);
            } else {
                obj_ptr->set("params", *p);
            }
        }
        impl.touch("devices");
        Value out = obj();
        out.set("device_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.connect"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const Value* av = opt(params, "a");
        const Value* bv = opt(params, "b");
        if (!av || !bv) fail("invalid_params", "'a' e 'b' são obrigatórios");
        const PinIdent a = parse_pin(*av);
        const PinIdent b = parse_pin(*bv);
        Value& doc = impl.project.document();
        if (!owner_exists(doc, a)) fail("not_found", "pino inexistente: " + a.owner);
        if (!owner_exists(doc, b)) fail("not_found", "pino inexistente: " + b.owner);

        std::string net_id;
        const Value* netp = opt(params, "net");
        if (netp && netp->is_object()) {
            const Value* nid = netp->find("id");
            if (nid && nid->is_string()) net_id = nid->as_string();
        }

        std::string id_a;
        std::string id_b;
        Value* na = find_net_of_pin(doc, a, &id_a);
        Value* nb = find_net_of_pin(doc, b, &id_b);
        if (na && nb && id_a != id_b) {
            Value details = obj();
            Value pins = arr();
            pins.push(pin_to_value(a));
            pins.push(pin_to_value(b));
            details.set("pins", std::move(pins));
            fail("conflict", "pinos já pertencem a nets diferentes", std::move(details));
        }
        if (net_id.empty()) {
            if (na) net_id = id_a;
            else if (nb) net_id = id_b;
        }
        if (net_id.empty() || !find_net_by_id(doc, net_id)) {
            if (net_id.empty()) net_id = new_ulid();
            Value* nets = doc.find("nets");
            if (!nets || !nets->is_array()) {
                doc.set("nets", arr());
                nets = doc.find("nets");
            }
            std::string name = net_id;
            NetKind kind = NetKind::Digital;
            if (netp && netp->is_object()) {
                const Value* n = netp->find("name");
                if (n && n->is_string()) name = n->as_string();
                const Value* k = netp->find("kind");
                if (k && k->is_string() && !parse_net_kind(k->as_string(), kind)) {
                    fail("invalid_params", "kind de net inválido");
                }
            }
            Value net = obj();
            net.set("id", net_id);
            net.set("name", name);
            net.set("kind", to_string(kind));
            net.set("pins", arr());
            nets->push(std::move(net));
        }
        attach_pin(doc, net_id, a);
        attach_pin(doc, net_id, b);
        impl.touch("nets");
        Value out = obj();
        out.set("net_id", net_id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.disconnect"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const Value* av = opt(params, "a");
        if (!av) fail("invalid_params", "'a' é obrigatório");
        const PinIdent a = parse_pin(*av);
        Value& doc = impl.project.document();
        const bool removed = detach_pin(doc, a);
        if (removed) impl.touch("nets");
        Value out = obj();
        out.set("removed", removed);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.addNet"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string kind_text = get_string(params, "kind", "digital");
        NetKind kind = NetKind::Digital;
        if (!parse_net_kind(kind_text, kind)) {
            fail("invalid_params", "kind de net inválido: " + kind_text);
        }
        const std::string id = get_string(params, "id", new_ulid());
        const std::string name = get_string(params, "name", id);
        Value& doc = impl.project.document();
        Value* nets = doc.find("nets");
        if (!nets || !nets->is_array()) {
            doc.set("nets", arr());
            nets = doc.find("nets");
        }
        Value net = obj();
        net.set("id", id);
        net.set("name", name);
        net.set("kind", to_string(kind));
        net.set("pins", arr());
        nets->push(std::move(net));
        impl.touch("nets");
        Value out = obj();
        out.set("net_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.removeNet"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "net_id");
        Value& doc = impl.project.document();
        Value* nets = doc.find("nets");
        if (!nets || !nets->is_array()) fail("not_found", "net inexistente: " + id);
        auto& list = nets->as_array();
        bool removed = false;
        for (auto it = list.begin(); it != list.end(); ++it) {
            const Value* nid = it->find("id");
            if (nid && nid->is_string() && nid->as_string() == id) {
                list.erase(it);
                removed = true;
                break;
            }
        }
        if (!removed) fail("not_found", "net inexistente: " + id);
        impl.touch("nets");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.setNet"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "net_id");
        Value& doc = impl.project.document();
        Value* net = find_net_by_id(doc, id);
        if (!net) fail("not_found", "net inexistente: " + id);
        if (const Value* name = opt(params, "name"); name && name->is_string()) {
            net->set("name", *name);
        }
        if (const Value* k = opt(params, "kind"); k && k->is_string()) {
            NetKind kind = NetKind::Digital;
            if (!parse_net_kind(k->as_string(), kind)) fail("invalid_params", "kind inválido");
            net->set("kind", to_string(kind));
        }
        impl.touch("nets");
        Value out = obj();
        out.set("net_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.attachPin"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string net_id = require_string(params, "net_id");
        const Value* pinv = opt(params, "pin");
        if (!pinv) fail("invalid_params", "'pin' é obrigatório");
        const PinIdent pin = parse_pin(*pinv);
        Value& doc = impl.project.document();
        if (!owner_exists(doc, pin)) fail("not_found", "pino inexistente: " + pin.owner);
        attach_pin(doc, net_id, pin);
        impl.touch("nets");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.detachPin"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const Value* pinv = opt(params, "pin");
        if (!pinv) fail("invalid_params", "'pin' é obrigatório");
        const PinIdent pin = parse_pin(*pinv);
        Value& doc = impl.project.document();
        const bool removed = detach_pin(doc, pin);
        if (removed) impl.touch("nets");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.addWire"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string net_id = require_string(params, "net_id");
        Value& doc = impl.project.document();
        if (!find_net_by_id(doc, net_id)) fail("not_found", "net inexistente: " + net_id);
        Value* wires = doc.find("wires");
        if (!wires || !wires->is_array()) {
            doc.set("wires", arr());
            wires = doc.find("wires");
        }
        const std::string id = get_string(params, "wire_id", new_ulid());
        Value wire = obj();
        wire.set("id", id);
        wire.set("net", net_id);
        const Value* points = opt(params, "points");
        wire.set("points", points && points->is_array() ? *points : arr());
        if (const Value* color = opt(params, "color"); color && color->is_string()) {
            wire.set("color", *color);
        }
        wires->push(std::move(wire));
        impl.touch("wires");
        Value out = obj();
        out.set("wire_id", id);
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.removeWire"] = [](Impl& impl, const Value& params) {
        impl.require_project();
        impl.check_base_revision(params);
        const std::string id = require_string(params, "wire_id");
        Value& doc = impl.project.document();
        Value* wires = doc.find("wires");
        if (!wires || !wires->is_array()) fail("not_found", "fio inexistente: " + id);
        auto& list = wires->as_array();
        bool removed = false;
        for (auto it = list.begin(); it != list.end(); ++it) {
            const Value* wid = it->find("id");
            if (wid && wid->is_string() && wid->as_string() == id) {
                list.erase(it);
                removed = true;
                break;
            }
        }
        if (!removed) fail("not_found", "fio inexistente: " + id);
        impl.touch("wires");
        Value out = obj();
        out.set("revision", static_cast<long long>(impl.project.revision()));
        return out;
    };
    handlers["circuit.erc"] = [](Impl& impl, const Value&) {
        impl.require_project();
        const auto diagnostics = impl.project.validate();
        Value out = obj();
        out.set("diagnostics", diagnostics_to_json(diagnostics));
        out.set("clean", !Project::has_errors(diagnostics));
        return out;
    };
    handlers["circuit.pick"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("hit", Value());
        return out;
    };
    handlers["circuit.bounds"] = [](Impl&, const Value&) {
        Value out = obj();
        Value minv = obj();
        minv.set("x", 0);
        minv.set("y", 0);
        Value maxv = obj();
        maxv.set("x", 0);
        maxv.set("y", 0);
        out.set("min", std::move(minv));
        out.set("max", std::move(maxv));
        return out;
    };

    // --- sim.* ------------------------------------------------------------
    handlers["sim.load"] = [](Impl& impl, const Value&) {
        impl.load_engine();
        Value diagnostics = diagnostics_to_json(impl.project.validate());
        Value out = obj();
        out.set("session_id", impl.session_id);
        out.set("state", to_string(impl.engine.state()));
        out.set("diagnostics", std::move(diagnostics));
        return out;
    };
    handlers["sim.run"] = [](Impl& impl, const Value& params) {
        impl.ensure_engine();
        impl.engine.start();
        if (const Value* until = opt(params, "until_ns");
            until && until->is_number()) {
            impl.engine.run_until(until->as_int64());
        }
        impl.ensure_engine();
        Value payload = obj();
        payload.set("state", to_string(impl.engine.state()));
        payload.set("t_ns", static_cast<long long>(impl.engine.now()));
        payload.set("revision", static_cast<long long>(impl.project.revision()));
        impl.emit("sim.state", payload);
        Value out = obj();
        out.set("state", to_string(impl.engine.state()));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.pause"] = [](Impl& impl, const Value&) {
        impl.ensure_engine();
        impl.engine.pause();
        Value out = obj();
        out.set("state", to_string(impl.engine.state()));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.resume"] = [](Impl& impl, const Value&) {
        impl.ensure_engine();
        impl.engine.resume();
        Value out = obj();
        out.set("state", to_string(impl.engine.state()));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.stop"] = [](Impl& impl, const Value&) {
        impl.ensure_engine();
        impl.engine.stop();
        Value out = obj();
        out.set("state", to_string(impl.engine.state()));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.reset"] = [](Impl& impl, const Value&) {
        impl.ensure_engine();
        impl.engine.reset();
        Value out = obj();
        out.set("state", to_string(impl.engine.state()));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.step"] = [](Impl& impl, const Value& params) {
        impl.ensure_engine();
        const std::size_t before = impl.engine.event_count();
        if (const Value* ns = opt(params, "ns"); ns && ns->is_number()) {
            impl.engine.run_for(ns->as_int64());
        } else if (const Value* ev = opt(params, "events"); ev && ev->is_number()) {
            impl.engine.step_events(static_cast<std::size_t>(ev->as_int64()));
        } else {
            impl.engine.step_events(1);
        }
        impl.ensure_engine();
        Value out = obj();
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        out.set("events_processed",
                static_cast<long long>(impl.engine.event_count() - before));
        return out;
    };
    handlers["sim.state"] = [](Impl& impl, const Value&) {
        impl.require_project();
        return impl.sim_state();
    };
    handlers["sim.frame"] = [](Impl& impl, const Value&) {
        impl.ensure_engine();
        const std::uint64_t seq = impl.frame_seq++;
        Value out = obj();
        out.set("seq", static_cast<long long>(seq));
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        out.set("frame", impl.engine.frame(seq));
        return out;
    };
    handlers["sim.setSpeed"] = [](Impl& impl, const Value& params) {
        const std::int64_t factor = get_int(params, "factor", 1);
        if (factor <= 0) fail("invalid_params", "factor deve ser positivo");
        impl.speed = static_cast<double>(factor);
        Value out = obj();
        out.set("speed", impl.speed);
        return out;
    };
    handlers["sim.setSeed"] = [](Impl& impl, const Value& params) {
        const std::int64_t seed = get_int(params, "seed", 0);
        impl.project.document().set("simulation", impl.project.document().get(
            "simulation", Value::object()));
        impl.project.document()["simulation"].set("seed", seed);
        Value out = obj();
        out.set("seed", static_cast<long long>(seed));
        return out;
    };
    handlers["sim.debug.setPin"] = [](Impl& impl, const Value& params) {
        impl.ensure_engine();
        const Value* target = opt(params, "target");
        if (!target) fail("invalid_params", "'target' é obrigatório");
        const PinIdent pin = parse_pin(*target);
        if (!pin.is_board) fail("invalid_params", "target deve ser um pino de placa");
        const std::string level_text = require_string(params, "level");
        Level level = Level::Z;
        if (!parse_level(level_text, level)) {
            fail("invalid_params", "level inválido: " + level_text);
        }
        std::string error;
        if (!impl.engine.set_pin(pin.owner, pin.pin, level, &error)) {
            fail("invalid_params", error);
        }
        Value out = obj();
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };
    handlers["sim.debug.releasePin"] = [](Impl& impl, const Value& params) {
        impl.ensure_engine();
        const Value* target = opt(params, "target");
        if (!target) fail("invalid_params", "'target' é obrigatório");
        const PinIdent pin = parse_pin(*target);
        if (!pin.is_board) fail("invalid_params", "target deve ser um pino de placa");
        std::string error;
        if (!impl.engine.release_pin(pin.owner, pin.pin, &error)) {
            fail("invalid_params", error);
        }
        Value out = obj();
        out.set("t_ns", static_cast<long long>(impl.engine.now()));
        return out;
    };

    // --- events.* ---------------------------------------------------------
    handlers["events.poll"] = [](Impl& impl, const Value& params) {
        const std::uint64_t after =
            static_cast<std::uint64_t>(get_int(params, "after_seq", 0));
        std::size_t max = static_cast<std::size_t>(get_int(params, "max", 256));
        if (max == 0) max = 256;
        const EventBatch batch = impl.poll(after, max);
        Value events = arr();
        for (const auto& ev : batch.events) events.push(ev);
        Value out = obj();
        out.set("events", std::move(events));
        out.set("next_seq", static_cast<long long>(batch.next_seq));
        out.set("dropped", static_cast<long long>(batch.dropped));
        return out;
    };
    handlers["events.subscribe"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("subscription_id", new_ulid());
        return out;
    };
    handlers["events.unsubscribe"] = [](Impl&, const Value&) {
        Value out = obj();
        out.set("removed", true);
        return out;
    };

    // --- catálogos --------------------------------------------------------
    handlers["device.catalog"] = [](Impl&, const Value& params) {
        const std::string category = get_string(params, "category", "");
        const std::string query = get_string(params, "query", "");
        Value devices = arr();
        for (const DeviceType* type : DeviceRegistry::instance().all()) {
            if (type->category == "board") continue;
            if (!category.empty() && type->category != category) continue;
            if (!query.empty() && type->id.find(query) == std::string::npos &&
                type->name.find(query) == std::string::npos) {
                continue;
            }
            Value d = obj();
            d.set("type", type->id);
            d.set("type_version", 1);
            d.set("name", type->name);
            d.set("category", type->category);
            Value pins = arr();
            for (const auto& pin : type->pins) {
                Value p = obj();
                p.set("name", pin.name);
                p.set("direction", to_string(pin.direction));
                pins.push(std::move(p));
            }
            d.set("pins", std::move(pins));
            devices.push(std::move(d));
        }
        Value out = obj();
        out.set("devices", std::move(devices));
        return out;
    };
    handlers["device.describe"] = [](Impl&, const Value& params) {
        const std::string type_id = require_string(params, "type");
        const DeviceType* type = DeviceRegistry::instance().find(type_id);
        if (!type) fail("not_found", "tipo não registrado: " + type_id);
        Value d = obj();
        d.set("type", type->id);
        d.set("type_version", 1);
        d.set("name", type->name);
        d.set("category", type->category);
        Value pins = arr();
        for (const auto& pin : type->pins) {
            Value p = obj();
            p.set("name", pin.name);
            p.set("direction", to_string(pin.direction));
            pins.push(std::move(p));
        }
        d.set("pins", std::move(pins));
        Value out = obj();
        out.set("descriptor", std::move(d));
        return out;
    };
    handlers["board.catalog"] = [](Impl&, const Value&) {
        Value boards = arr();
        for (const DeviceType* type : DeviceRegistry::instance().all()) {
            if (type->category != "board") continue;
            Value b = obj();
            b.set("type", type->id);
            b.set("type_version", 1);
            b.set("name", type->name);
            auto instance = DeviceRegistry::instance().create(
                type->id, "catalog", Value::object());
            Value pins = arr();
            if (instance) {
                for (const auto& pin : instance->pins()) {
                    Value p = obj();
                    p.set("name", pin.name);
                    p.set("direction", to_string(pin.direction));
                    pins.push(std::move(p));
                }
            }
            b.set("pins", std::move(pins));
            Value caps = arr();
            caps.push(Value("gpio"));
            b.set("capabilities", std::move(caps));
            boards.push(std::move(b));
        }
        Value out = obj();
        out.set("boards", std::move(boards));
        return out;
    };
    handlers["board.describe"] = [](Impl&, const Value& params) {
        const std::string type_id = require_string(params, "type");
        const DeviceType* type = DeviceRegistry::instance().find(type_id);
        if (!type || type->category != "board") {
            fail("not_found", "placa não registrada: " + type_id);
        }
        Value b = obj();
        b.set("type", type->id);
        b.set("type_version", 1);
        b.set("name", type->name);
        auto instance = DeviceRegistry::instance().create(
            type->id, "catalog", Value::object());
        Value pins = arr();
        if (instance) {
            for (const auto& pin : instance->pins()) {
                Value p = obj();
                p.set("name", pin.name);
                p.set("direction", to_string(pin.direction));
                pins.push(std::move(p));
            }
        }
        b.set("pins", std::move(pins));
        Value out = obj();
        out.set("descriptor", std::move(b));
        return out;
    };
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

Session::Session() : impl_(std::make_unique<Impl>()) {}
Session::~Session() = default;

std::string Session::execute(std::string_view request_json) {
    json::ParseError parse_error;
    json::Value request = json::parse(request_json, &parse_error);
    if (!parse_error.ok()) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        Value response = Value::object();
        response.set("id", "");
        response.set("ok", false);
        Value error = Value::object();
        error.set("code", "invalid_request");
        error.set("message", "JSON inválido na linha " +
                                 std::to_string(parse_error.line) + ":" +
                                 std::to_string(parse_error.column) + ": " +
                                 parse_error.message);
        error.set("retryable", false);
        response.set("error", std::move(error));
        impl_->last_error = "invalid_request: parse";
        return response.dump(0);
    }
    return execute_request(request).dump(0);
}

json::Value Session::execute_request(const json::Value& request) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->dispatch(request);
}

EventBatch Session::poll_events(std::uint64_t after_seq, std::size_t max) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->poll(after_seq, max);
}

std::string Session::last_error() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->last_error;
}

const char* Session::product_version() { return "0.1.0"; }
const char* Session::engine_version() { return "0.1.0"; }
int Session::api_version() { return 1; }

}  // namespace simforge
