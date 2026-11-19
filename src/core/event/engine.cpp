#include "simforge/engine.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <utility>

#include "simforge/board.hpp"

namespace simforge {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
/// Evento interno de fonte periódica (clock/PWM): só acorda o solver. Os bits
/// baixos carregam o índice do dispositivo para reagendar só ele.
constexpr std::uint32_t kWakeBase = 0x40000000u;
constexpr std::uint32_t kWakeMask = 0xFF000000u;

enum class Phase : std::uint8_t {
    PreInput = 0,
    Input = 1,
    Evaluate = 2,
    Settle = 3,
    Commit = 4,
    PostCommit = 5
};

struct Event {
    SimTime t_ns = 0;
    Phase phase = Phase::Input;
    std::uint64_t seq = 0;
    std::uint32_t kind = 0;
};

struct EventGreater {
    bool operator()(const Event& a, const Event& b) const {
        if (a.t_ns != b.t_ns) return a.t_ns > b.t_ns;
        if (a.phase != b.phase) return a.phase > b.phase;
        return a.seq > b.seq;
    }
};

struct NetRuntime {
    std::string id;
    std::string name;
    NetKind kind = NetKind::Digital;
    std::vector<std::uint32_t> pins;
    PinState resolved;
    Level level = Level::Z;
};

struct DeviceEntry {
    std::unique_ptr<Device> device;
    std::vector<std::uint32_t> pins;  // índices globais, na ordem de PinDesc
    Board* board = nullptr;
};

std::string path_of(const char* array, std::size_t index) {
    return std::string("/") + array + "/" + std::to_string(index);
}

}  // namespace

const char* to_string(EngineState state) {
    switch (state) {
        case EngineState::Idle: return "idle";
        case EngineState::Ready: return "ready";
        case EngineState::Running: return "running";
        case EngineState::Paused: return "paused";
        case EngineState::Stopped: return "stopped";
        case EngineState::Error: return "error";
    }
    return "?";
}

struct Engine::Impl {
    explicit Impl(EngineOptions opts) : options(opts) {
        register_builtin_devices();
        register_builtin_boards();
    }

    // --- configuração -----------------------------------------------------
    EngineOptions options;
    bool loaded = false;

    // --- tempo e estado ---------------------------------------------------
    SimTime now = 0;
    EngineState state = EngineState::Idle;
    std::uint64_t trace_hash = kFnvOffset;
    std::uint64_t events_processed = 0;
    std::uint32_t last_delta_cycles = 0;

    // --- topologia --------------------------------------------------------
    std::vector<NetRuntime> nets;
    std::unordered_map<std::string, std::uint32_t> net_index;
    std::vector<DeviceEntry> devices;
    std::unordered_map<std::string, std::uint32_t> device_index;
    std::vector<PinState> pin_states;
    std::vector<int> pin_net;  // -1 = sem net
    std::vector<PinRef> pin_refs;
    std::vector<PinState> net_states;  // scratch do solver (sem alocação por ciclo)

    // --- eventos ----------------------------------------------------------
    std::priority_queue<Event, std::vector<Event>, EventGreater> queue;
    std::uint64_t next_seq = 1;

    std::vector<EngineError> errors;

    void mix(std::uint64_t value) {
        trace_hash ^= value;
        trace_hash *= kFnvPrime;
    }

    void schedule(SimTime t, Phase phase, std::uint32_t kind) {
        queue.push(Event{t, phase, next_seq++, kind});
    }

    /// Agenda a próxima borda de uma fonte periódica. Chamado depois de
    /// carregar/resetar e a cada borda, mantendo o PWM avançando sem que o
    /// host precise enviar `sim.step` para cada transição.
    void schedule_wakeup(std::uint32_t index) {
        const SimTime t = devices[index].device->next_edge(now);
        if (t > now) schedule(t, Phase::Input, kWakeBase | index);
    }

    void schedule_wakeups() {
        for (std::uint32_t index = 0; index < devices.size(); ++index) {
            schedule_wakeup(index);
        }
    }

    void clear_runtime() {
        nets.clear();
        net_index.clear();
        devices.clear();
        device_index.clear();
        pin_states.clear();
        pin_net.clear();
        pin_refs.clear();
        while (!queue.empty()) queue.pop();
        next_seq = 1;
        now = 0;
        loaded = false;
        state = EngineState::Idle;
        trace_hash = kFnvOffset;
        events_processed = 0;
        last_delta_cycles = 0;
        errors.clear();
    }

    void error(std::string code, std::string message, json::Value details = {}) {
        errors.push_back(EngineError{std::move(code), std::move(message),
                                     std::move(details)});
        state = EngineState::Error;
    }

    bool build(const json::Value& document, std::string* out_error);
    void solve();
    std::uint32_t global_pin(const std::string& owner, std::string_view pin) const;
    std::size_t drain_at(SimTime t);
};

// ---------------------------------------------------------------------------
// Construção do runtime a partir do documento
// ---------------------------------------------------------------------------

namespace {

struct OwnerPins {
    std::string id;
    std::vector<PinDesc> pins;
    std::unique_ptr<Device> device;
};

}  // namespace

bool Engine::Impl::build(const json::Value& doc, std::string* out_error) {
    const auto fail = [&](std::string message) {
        if (out_error) *out_error = message;
        return false;
    };

    const json::Value* boards = doc.find("boards");
    const json::Value* device_array = doc.find("devices");
    const json::Value* net_array = doc.find("nets");
    if (!boards || !boards->is_array()) return fail("documento sem 'boards'");
    if (!device_array || !device_array->is_array())
        return fail("documento sem 'devices'");
    if (!net_array || !net_array->is_array()) return fail("documento sem 'nets'");

    // 1. Instancia placas e dispositivos, ordenados por ID (determinismo).
    std::vector<OwnerPins> owners;
    const auto add_owner = [&](const json::Value& obj, bool is_board) -> bool {
        const json::Value* id = obj.find("id");
        const json::Value* type = obj.find("type");
        if (!id || !id->is_string() || !type || !type->is_string()) {
            return fail("placa/dispositivo sem 'id' e 'type'");
        }
        const DeviceType* info = DeviceRegistry::instance().find(type->as_string());
        if (!info) {
            return fail("tipo desconhecido: " + type->as_string());
        }
        if (is_board && info->category != "board") {
            return fail("tipo de placa inválido: " + type->as_string());
        }
        auto created = DeviceRegistry::instance().create(
            type->as_string(), id->as_string(),
            obj.get("params", json::Value::object()));
        if (!created) return fail("falha ao criar " + type->as_string());
        const auto pin_span = created->pins();
        owners.push_back(OwnerPins{
            id->as_string(),
            std::vector<PinDesc>(pin_span.begin(), pin_span.end()),
            std::move(created)});
        return true;
    };

    for (const auto& obj : boards->as_array()) {
        if (!add_owner(obj, true)) return false;
    }
    for (const auto& obj : device_array->as_array()) {
        if (!add_owner(obj, false)) return false;
    }

    std::sort(owners.begin(), owners.end(),
              [](const OwnerPins& a, const OwnerPins& b) { return a.id < b.id; });
    for (std::size_t i = 1; i < owners.size(); ++i) {
        if (owners[i].id == owners[i - 1].id) {
            return fail("ID duplicado: " + owners[i].id);
        }
    }

    // 2. Índice global de pinos.
    for (auto& owner : owners) {
        std::vector<std::uint32_t> indices;
        indices.reserve(owner.pins.size());
        for (const auto& pin : owner.pins) {
            const auto index = static_cast<std::uint32_t>(pin_refs.size());
            pin_refs.push_back(PinRef{owner.id, pin.name});
            pin_net.push_back(-1);
            pin_states.push_back(PinState{});
            indices.push_back(index);
        }
        DeviceEntry entry;
        entry.board = dynamic_cast<Board*>(owner.device.get());
        entry.device = std::move(owner.device);
        entry.pins = std::move(indices);
        device_index[owner.id] = static_cast<std::uint32_t>(devices.size());
        devices.push_back(std::move(entry));
    }

    // 3. Nets e resolução das referências.
    for (const auto& net_obj : net_array->as_array()) {
        const json::Value* id = net_obj.find("id");
        if (!id || !id->is_string()) return fail("net sem 'id'");
        if (net_index.count(id->as_string()))
            return fail("net duplicada: " + id->as_string());

        NetRuntime net;
        net.id = id->as_string();
        net.name = net_obj.get("name", json::Value(net.id)).is_string()
                       ? net_obj.get("name", json::Value(net.id)).as_string()
                       : net.id;
        const json::Value kind = net_obj.get("kind", json::Value("digital"));
        if (!kind.is_string() || !parse_net_kind(kind.as_string(), net.kind)) {
            return fail("net " + net.id + " com kind inválido");
        }

        const json::Value* pins = net_obj.find("pins");
        if (pins && pins->is_array()) {
            for (const auto& pin_ref : pins->as_array()) {
                const json::Value* owner =
                    pin_ref.find("board") ? pin_ref.find("board") : pin_ref.find("device");
                const json::Value* pin_name = pin_ref.find("pin");
                if (!owner || !pin_name || !pin_name->is_string()) {
                    return fail("net " + net.id + " com referência de pino inválida");
                }
                const std::string& owner_id = owner->as_string();
                const auto it = device_index.find(owner_id);
                if (it == device_index.end()) {
                    return fail("net " + net.id + " referencia dono inexistente: " +
                                owner_id);
                }
                std::uint32_t index = 0;
                bool found = false;
                const auto& entry = devices[it->second];
                for (std::size_t i = 0; i < entry.pins.size(); ++i) {
                    if (pin_refs[entry.pins[i]].pin == pin_name->as_string()) {
                        index = entry.pins[i];
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return fail("pino inexistente: " + owner_id + "." +
                                pin_name->as_string());
                }
                if (pin_net[index] >= 0) {
                    return fail("pino " + owner_id + "." +
                                pin_name->as_string() + " em mais de uma net");
                }
                pin_net[index] = static_cast<int>(nets.size());
                net.pins.push_back(index);
            }
        }
        net_index[net.id] = static_cast<std::uint32_t>(nets.size());
        nets.push_back(std::move(net));
    }

    pin_states.assign(pin_refs.size(), PinState{});

    // Buffer reutilizado pelo solver: evita uma alocação por net por ciclo.
    std::size_t widest = 0;
    for (const auto& net : nets) widest = std::max(widest, net.pins.size());
    net_states.clear();
    net_states.reserve(widest);

    loaded = true;
    state = EngineState::Ready;
    return true;
}

// ---------------------------------------------------------------------------
// Solver
// ---------------------------------------------------------------------------

namespace {

class ContextImpl final : public DeviceContext {
public:
    ContextImpl(Engine::Impl& impl, const std::vector<std::uint32_t>& pins)
        : impl_(impl), pins_(pins) {}

    SimTime now() const override { return impl_.now; }

    Level net_level(std::size_t local) const override {
        if (local >= pins_.size()) return Level::Z;
        const int net = impl_.pin_net[pins_[local]];
        if (net < 0) return Level::Z;
        return impl_.nets[static_cast<std::size_t>(net)].level;
    }

    void drive(std::size_t local, PinState state) override {
        if (local >= pins_.size()) return;
        impl_.pin_states[pins_[local]] = state;
    }

private:
    Engine::Impl& impl_;
    const std::vector<std::uint32_t>& pins_;
};

}  // namespace

void Engine::Impl::solve() {
    const std::uint32_t limit = options.delta_cycle_limit;
    bool converged = false;
    std::uint32_t cycles = 0;

    for (cycles = 0; cycles < limit; ++cycles) {
        std::fill(pin_states.begin(), pin_states.end(), PinState{});
        for (auto& entry : devices) {
            ContextImpl ctx(*this, entry.pins);
            entry.device->evaluate(ctx);
        }

        bool changed = false;
        for (auto& net : nets) {
            net_states.clear();
            for (const auto index : net.pins) net_states.push_back(pin_states[index]);
            const PinState resolved =
                resolve_pin_states(net_states.data(), net_states.size());
            if (resolved.level != net.level) {
                net.level = resolved.level;
                changed = true;
            }
            net.resolved = resolved;
        }

        mix(static_cast<std::uint64_t>(now));
        for (const auto& net : nets) mix(static_cast<std::uint64_t>(net.level));

        if (!changed) {
            converged = true;
            break;
        }
    }

    last_delta_cycles = cycles;
    if (!converged) {
        json::Value details = json::Value::object();
        json::Value unstable = json::Value::array();
        for (const auto& net : nets) {
            if (net.level == Level::X) unstable.push(net.id);
        }
        details.set("nets", std::move(unstable));
        details.set("delta_cycles", static_cast<long long>(cycles));
        error("sim_oscillation", "realimentação combinacional não convergiu",
              std::move(details));
    }
}

std::uint32_t Engine::Impl::global_pin(const std::string& owner,
                                       std::string_view pin) const {
    const auto it = device_index.find(owner);
    if (it == device_index.end()) return UINT32_MAX;
    for (const auto index : devices[it->second].pins) {
        if (pin_refs[index].pin == pin) return index;
    }
    return UINT32_MAX;
}

std::size_t Engine::Impl::drain_at(SimTime t) {
    std::size_t processed = 0;
    std::vector<std::uint32_t> woke;
    while (!queue.empty() && queue.top().t_ns == t) {
        const std::uint32_t kind = queue.top().kind;
        if ((kind & kWakeMask) == kWakeBase) woke.push_back(kind & ~kWakeMask);
        queue.pop();
        ++processed;
        ++events_processed;
    }
    // Cada borda agenda a próxima daquele dispositivo: o PWM continua mesmo
    // sem novos comandos, e não há eventos duplicados.
    for (const std::uint32_t index : woke) {
        if (index < devices.size()) schedule_wakeup(index);
    }
    return processed;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

Engine::Engine(EngineOptions options) : impl_(std::make_unique<Impl>(options)) {}

Engine::~Engine() = default;

bool Engine::load(const json::Value& document, std::string* error) {
    impl_->clear_runtime();
    if (!impl_->build(document, error)) {
        impl_->loaded = false;
        impl_->state = EngineState::Error;
        return false;
    }
    impl_->solve();
    if (impl_->state != EngineState::Error) impl_->schedule_wakeups();
    return impl_->state != EngineState::Error;
}

void Engine::reset() {
    const bool was_loaded = impl_->loaded;
    if (!was_loaded) return;
    for (auto& entry : impl_->devices) entry.device->reset();
    std::fill(impl_->pin_states.begin(), impl_->pin_states.end(), PinState{});
    for (auto& net : impl_->nets) {
        net.resolved = PinState{};
        net.level = Level::Z;
    }
    while (!impl_->queue.empty()) impl_->queue.pop();
    impl_->now = 0;
    impl_->events_processed = 0;
    impl_->trace_hash = kFnvOffset;
    impl_->state = EngineState::Ready;
    impl_->errors.clear();
    impl_->solve();
    impl_->schedule_wakeups();
}

void Engine::start() {
    if (!impl_->loaded) return;
    impl_->state = EngineState::Running;
}

void Engine::pause() {
    if (impl_->state == EngineState::Running) impl_->state = EngineState::Paused;
}

void Engine::resume() {
    if (impl_->state == EngineState::Paused) impl_->state = EngineState::Running;
}

void Engine::stop() {
    if (impl_->loaded) impl_->state = EngineState::Stopped;
}

void Engine::run_until(SimTime target) {
    if (!impl_->loaded || impl_->state == EngineState::Error) return;
    if (target <= impl_->now) {
        if (impl_->state == EngineState::Paused) return;
        return;
    }
    if (target > kMaxSimTime) target = kMaxSimTime;

    while (impl_->now < target) {
        SimTime next = target;
        if (!impl_->queue.empty() && impl_->queue.top().t_ns < target) {
            next = impl_->queue.top().t_ns;
        }
        impl_->now = next;
        impl_->drain_at(impl_->now);
        impl_->solve();
        if (impl_->state == EngineState::Error) return;
    }
}

void Engine::run_for(SimTime delta) {
    if (delta <= 0) return;
    run_until(impl_->now + delta);
}

std::size_t Engine::step_events(std::size_t max_events) {
    if (!impl_->loaded) return 0;
    std::size_t processed = 0;
    while (!impl_->queue.empty() && processed < max_events) {
        const SimTime t = impl_->queue.top().t_ns;
        impl_->now = t;
        processed += impl_->drain_at(t);
        impl_->solve();
        if (impl_->state == EngineState::Error) break;
    }
    return processed;
}

SimTime Engine::now() const { return impl_->now; }
EngineState Engine::state() const { return impl_->state; }
std::uint64_t Engine::trace_hash() const { return impl_->trace_hash; }
std::size_t Engine::event_count() const { return impl_->events_processed; }

void Engine::bench_enqueue(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        impl_->schedule(impl_->now + static_cast<SimTime>(i), Phase::Input, 0);
    }
}

std::size_t Engine::bench_dequeue(std::size_t count) {
    std::size_t removed = 0;
    while (!impl_->queue.empty() && removed < count) {
        impl_->queue.pop();
        ++removed;
    }
    return removed;
}

std::size_t Engine::bench_queue_size() const { return impl_->queue.size(); }

const std::vector<EngineError>& Engine::errors() const { return impl_->errors; }
void Engine::clear_errors() { impl_->errors.clear(); }

bool Engine::set_pin(std::string_view owner_id, std::string_view pin, Level level,
                     std::string* error) {
    const auto it = impl_->device_index.find(std::string(owner_id));
    if (it == impl_->device_index.end()) {
        if (error) *error = "placa/dispositivo inexistente: " + std::string(owner_id);
        return false;
    }
    Board* board = impl_->devices[it->second].board;
    if (!board) {
        if (error) *error = "objeto não é placa: " + std::string(owner_id);
        return false;
    }
    if (!board->set_pin(pin, level, error)) return false;
    impl_->schedule(impl_->now, Phase::Input, 1);
    impl_->solve();
    return true;
}

bool Engine::release_pin(std::string_view owner_id, std::string_view pin,
                         std::string* error) {
    const auto it = impl_->device_index.find(std::string(owner_id));
    if (it == impl_->device_index.end()) {
        if (error) *error = "placa/dispositivo inexistente: " + std::string(owner_id);
        return false;
    }
    Board* board = impl_->devices[it->second].board;
    if (!board) {
        if (error) *error = "objeto não é placa: " + std::string(owner_id);
        return false;
    }
    if (!board->release_pin(pin, error)) return false;
    impl_->schedule(impl_->now, Phase::Input, 2);
    impl_->solve();
    return true;
}

bool Engine::set_device_param(std::string_view device_id, std::string_view key,
                              const json::Value& value, std::string* error) {
    const auto it = impl_->device_index.find(std::string(device_id));
    if (it == impl_->device_index.end()) {
        if (error) *error = "dispositivo inexistente: " + std::string(device_id);
        return false;
    }
    if (!impl_->devices[it->second].device->set_param(key, value, impl_->now, error)) {
        return false;
    }
    impl_->schedule(impl_->now, Phase::Input, 3);
    impl_->solve();
    return true;
}

json::Value Engine::frame(std::uint64_t seq) const {
    json::Value frame = json::Value::object();
    frame.set("seq", static_cast<long long>(seq));
    frame.set("t_ns", static_cast<long long>(impl_->now));
    frame.set("state", to_string(impl_->state));

    json::Value devices = json::Value::array();
    for (const auto& entry : impl_->devices) {
        json::Value item = entry.device->read_state(impl_->now);
        item.set("id", std::string(entry.device->id()));
        item.set("type", std::string(entry.device->type_id()));
        devices.push(std::move(item));
    }
    frame.set("devices", std::move(devices));

    json::Value nets = json::Value::array();
    for (const auto& net : impl_->nets) {
        json::Value item = json::Value::object();
        item.set("id", net.id);
        item.set("level", to_string(net.level));
        item.set("strength", to_string(net.resolved.strength));
        nets.push(std::move(item));
    }
    frame.set("nets", std::move(nets));
    return frame;
}

json::Value Engine::nets_state() const {
    json::Value nets = json::Value::array();
    for (const auto& net : impl_->nets) {
        json::Value item = json::Value::object();
        item.set("id", net.id);
        item.set("name", net.name);
        item.set("kind", to_string(net.kind));
        item.set("level", to_string(net.level));
        item.set("strength", to_string(net.resolved.strength));
        item.set("pins", static_cast<long long>(net.pins.size()));
        nets.push(std::move(item));
    }
    return nets;
}

}  // namespace simforge
