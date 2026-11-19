#include "simforge/project.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "simforge/board.hpp"
#include "simforge/circuit.hpp"
#include "simforge/device.hpp"
#include "simforge/ids.hpp"
#include "simforge/paths.hpp"

namespace simforge {
namespace {

std::once_flag g_registry_once;

void ensure_registry() {
    std::call_once(g_registry_once, [] {
        register_builtin_devices();
        register_builtin_boards();
    });
}

Diagnostic make_diag(Severity severity, std::string code, std::string message,
                     std::string path) {
    Diagnostic d;
    d.severity = severity;
    d.code = std::move(code);
    d.message = std::move(message);
    d.path = std::move(path);
    return d;
}

bool is_safe_relative_path(const std::string& path) {
    if (path.empty()) return false;
    if (path.find("..") != std::string::npos) return false;
    if (path.size() >= 2 && path[1] == ':') return false;
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) return false;
    return true;
}

const json::Value* array_of(const json::Value& doc, const char* key) {
    const json::Value* value = doc.find(key);
    if (!value || !value->is_array()) return nullptr;
    return value;
}

}  // namespace

const char* to_string(Severity severity) {
    switch (severity) {
        case Severity::Info: return "info";
        case Severity::Warning: return "warning";
        case Severity::Error: return "error";
    }
    return "?";
}

json::Value Diagnostic::to_json() const {
    json::Value out = json::Value::object();
    out.set("severity", to_string(severity));
    out.set("code", code);
    out.set("message", message);
    if (!path.empty()) out.set("path", path);
    if (!net.empty()) out.set("net", net);
    if (!fix.empty()) out.set("fix", fix);
    return out;
}

Project Project::create(std::string name, std::string board_type,
                        std::string* error) {
    ensure_registry();

    json::Value doc = json::Value::object();
    doc.set("format", "simforge.project");
    doc.set("schema", 1);

    json::Value project = json::Value::object();
    project.set("id", new_ulid());
    project.set("name", std::move(name));
    project.set("revision", 1);
    doc.set("project", std::move(project));

    json::Value units = json::Value::object();
    units.set("length", "mm");
    units.set("grid_mm", 1.27);
    units.set("voltage", "V");
    units.set("current", "A");
    units.set("resistance", "ohm");
    units.set("time", "ns");
    doc.set("units", std::move(units));

    json::Value boards = json::Value::array();
    if (!board_type.empty() && board_type != "none") {
        if (!DeviceRegistry::instance().find(board_type)) {
            if (error) *error = "tipo de placa desconhecido: " + board_type;
            return Project{};
        }
        json::Value board = json::Value::object();
        board.set("id", new_ulid());
        board.set("type", board_type);
        board.set("type_version", 1);
        json::Value position = json::Value::object();
        position.set("x", 0);
        position.set("y", 0);
        board.set("position", std::move(position));
        board.set("rotation", 0);
        board.set("params", json::Value::object());
        board.set("pins", json::Value::object());
        boards.push(std::move(board));
    }
    doc.set("boards", std::move(boards));
    doc.set("devices", json::Value::array());
    doc.set("nets", json::Value::array());
    doc.set("wires", json::Value::array());

    json::Value firmware = json::Value::object();
    firmware.set("entry", "src/main.cpp");
    firmware.set("backend", "platformio");
    firmware.set("framework", "arduino");
    firmware.set("libraries", json::Value::array());
    doc.set("firmware", std::move(firmware));

    json::Value simulation = json::Value::object();
    simulation.set("solver", "auto");
    simulation.set("seed", 0);
    simulation.set("slice_us", 1000);
    simulation.set("analog_step_us", 10);
    simulation.set("real_time", false);
    doc.set("simulation", std::move(simulation));

    json::Value layout = json::Value::object();
    layout.set("zoom", 1.0);
    json::Value pan = json::Value::object();
    pan.set("x", 0);
    pan.set("y", 0);
    layout.set("pan", std::move(pan));
    layout.set("grid", true);
    doc.set("layout", std::move(layout));
    doc.set("extensions", json::Value::object());

    Project project_doc;
    project_doc.document_ = std::move(doc);
    return project_doc;
}

std::optional<Project> Project::from_json(json::Value document,
                                          std::string* error) {
    ensure_registry();
    if (!document.is_object()) {
        if (error) *error = "documento não é um objeto JSON";
        return std::nullopt;
    }
    const json::Value* format = document.find("format");
    if (!format || !format->is_string() ||
        format->as_string() != "simforge.project") {
        if (error) *error = "formato inválido: esperado 'simforge.project'";
        return std::nullopt;
    }
    const json::Value* schema = document.find("schema");
    if (schema && schema->is_number()) {
        const std::int64_t version = schema->as_int64();
        if (version > 1) {
            if (error) *error = "schema futuro não suportado: " +
                                std::to_string(version);
            return std::nullopt;
        }
    }
    const json::Value* project = document.find("project");
    if (!project || !project->is_object()) {
        if (error) *error = "documento sem 'project'";
        return std::nullopt;
    }
    if (!array_of(document, "boards") || !array_of(document, "devices") ||
        !array_of(document, "nets")) {
        if (error) *error = "documento sem 'boards', 'devices' ou 'nets'";
        return std::nullopt;
    }
    Project out;
    out.document_ = std::move(document);
    return out;
}

std::optional<Project> Project::load(const std::filesystem::path& path,
                                     std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "não foi possível abrir: " + utf8_from_path(path);
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    json::ParseError parse_error;
    json::Value doc = json::parse(buffer.str(), &parse_error);
    if (!parse_error.ok()) {
        if (error) {
            *error = "JSON inválido em " + utf8_from_path(path) + " (" +
                     std::to_string(parse_error.line) + ":" +
                     std::to_string(parse_error.column) + "): " +
                     parse_error.message;
        }
        return std::nullopt;
    }
    return from_json(std::move(doc), error);
}

bool Project::save(const std::filesystem::path& path, std::string* error) const {
    const std::string text = document_.dump(2) + "\n";
    const auto temp = std::filesystem::path(path).concat(".tmp");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "não foi possível escrever: " + utf8_from_path(temp);
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            if (error) *error = "falha ao escrever: " + utf8_from_path(temp);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        if (error) *error = "falha ao renomear: " + ec.message();
        return false;
    }
    return true;
}

std::string Project::to_string() const { return document_.dump(2) + "\n"; }

std::uint64_t Project::revision() const {
    const json::Value* project = document_.find("project");
    if (!project) return 0;
    const json::Value* revision = project->find("revision");
    if (!revision || !revision->is_number()) return 0;
    return static_cast<std::uint64_t>(revision->as_int64());
}

void Project::set_revision(std::uint64_t revision) {
    json::Value* project = document_.find("project");
    if (!project) project = &document_.set("project", json::Value::object());
    project->set("revision", static_cast<long long>(revision));
}

void Project::bump_revision() { set_revision(revision() + 1); }

std::string Project::id() const {
    const json::Value* project = document_.find("project");
    if (!project) return {};
    const json::Value* id = project->find("id");
    return id && id->is_string() ? id->as_string() : std::string{};
}

std::string Project::name() const {
    const json::Value* project = document_.find("project");
    if (!project) return {};
    const json::Value* name = project->find("name");
    return name && name->is_string() ? name->as_string() : std::string{};
}

const json::Value* Project::find_object(std::string_view array,
                                        std::string_view id) const {
    const json::Value* list = document_.find(array);
    if (!list || !list->is_array()) return nullptr;
    for (const auto& item : list->as_array()) {
        const json::Value* item_id = item.find("id");
        if (item_id && item_id->is_string() && item_id->as_string() == id) {
            return &item;
        }
    }
    return nullptr;
}

json::Value* Project::find_object(std::string_view array, std::string_view id) {
    json::Value* list = document_.find(array);
    if (!list || !list->is_array()) return nullptr;
    for (auto& item : list->as_array()) {
        json::Value* item_id = item.find("id");
        if (item_id && item_id->is_string() && item_id->as_string() == id) {
            return &item;
        }
    }
    return nullptr;
}

bool Project::has_errors(const std::vector<Diagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const Diagnostic& d) {
                           return d.severity == Severity::Error;
                       });
}

std::vector<Diagnostic> Project::validate() const {
    std::vector<Diagnostic> out;

    const json::Value* format = document_.find("format");
    if (!format || !format->is_string() ||
        format->as_string() != "simforge.project") {
        out.push_back(make_diag(Severity::Error, "E_FORMAT",
                                "format deve ser 'simforge.project'", "/format"));
    }
    const json::Value* schema = document_.find("schema");
    if (!schema || !schema->is_number() || schema->as_int64() != 1) {
        out.push_back(make_diag(Severity::Error, "E_SCHEMA",
                                "schema deve ser 1", "/schema"));
    }

    const json::Value* project = document_.find("project");
    if (!project || !project->is_object()) {
        out.push_back(
            make_diag(Severity::Error, "E_PROJECT", "campo 'project' ausente", "/"));
        return out;
    }
    if (const json::Value* id = project->find("id");
        !id || !id->is_string() || !is_ulid(id->as_string())) {
        out.push_back(make_diag(Severity::Warning, "W_PROJECT_ID",
                                "project.id não é um ULID", "/project/id"));
    }

    // Índice de donos (placas + dispositivos) -> pinos do descritor.
    struct Owner {
        std::string id;
        std::string type;
        std::vector<PinDesc> pins;
    };
    std::unordered_map<std::string, Owner> owners;
    std::unordered_map<std::string, std::string> owner_array;

    const auto collect = [&](const char* array_name) {
        const json::Value* list = document_.find(array_name);
        if (!list || !list->is_array()) return;
        std::size_t index = 0;
        for (const auto& item : list->as_array()) {
            const std::string path = "/" + std::string(array_name) + "/" +
                                     std::to_string(index++);
            const json::Value* id = item.find("id");
            const json::Value* type = item.find("type");
            if (!id || !id->is_string()) {
                out.push_back(make_diag(Severity::Error, "E_MISSING_ID",
                                        "objeto sem 'id'", path));
                continue;
            }
            if (!type || !type->is_string()) {
                out.push_back(make_diag(Severity::Error, "E_MISSING_TYPE",
                                        "objeto sem 'type'", path));
                continue;
            }
            if (owners.count(id->as_string())) {
                out.push_back(make_diag(Severity::Error, "E_DUPLICATE_ID",
                                        "id duplicado: " + id->as_string(), path));
                continue;
            }
            const DeviceType* info =
                DeviceRegistry::instance().find(type->as_string());
            Owner owner;
            owner.id = id->as_string();
            owner.type = type->as_string();
            if (info) {
                auto instance = DeviceRegistry::instance().create(
                    type->as_string(), id->as_string(), json::Value::object());
                if (instance) {
                    owner.pins.assign(instance->pins().begin(),
                                      instance->pins().end());
                }
            } else {
                out.push_back(make_diag(Severity::Warning, "W_UNKNOWN_TYPE",
                                        "tipo não registrado: " + type->as_string(),
                                        path + "/type"));
            }
            owners.emplace(owner.id, std::move(owner));
            owner_array.emplace(id->as_string(), array_name);
        }
    };
    collect("boards");
    collect("devices");

    // Nets: referências e conectividade.
    std::unordered_map<std::string, std::size_t> pin_usage;
    std::unordered_set<std::string> net_ids;
    if (const json::Value* nets = document_.find("nets"); nets && nets->is_array()) {
        std::size_t net_index = 0;
        for (const auto& net : nets->as_array()) {
            const std::string base = "/nets/" + std::to_string(net_index++);
            const json::Value* id = net.find("id");
            if (!id || !id->is_string()) {
                out.push_back(
                    make_diag(Severity::Error, "E_NET_ID", "net sem 'id'", base));
                continue;
            }
            const std::string net_id = id->as_string();
            if (!net_ids.insert(net_id).second) {
                out.push_back(make_diag(Severity::Error, "E_DUPLICATE_NET",
                                        "net duplicada: " + net_id, base));
            }
            const json::Value* pins = net.find("pins");
            std::size_t pin_count = 0;
            if (pins && pins->is_array()) {
                for (const auto& pin_ref : pins->as_array()) {
                    const json::Value* owner = pin_ref.find("board")
                                                   ? pin_ref.find("board")
                                                   : pin_ref.find("device");
                    const json::Value* pin = pin_ref.find("pin");
                    if (!owner || !pin || !pin->is_string()) {
                        out.push_back(make_diag(Severity::Error, "E_PIN_REF",
                                                "referência de pino inválida",
                                                base + "/pins"));
                        continue;
                    }
                    ++pin_count;
                    const auto it = owners.find(owner->as_string());
                    if (it == owners.end()) {
                        out.push_back(make_diag(
                            Severity::Error, "E_UNKNOWN_OWNER",
                            "dono inexistente: " + owner->as_string(),
                            base + "/pins"));
                        continue;
                    }
                    const auto& owner_pins = it->second.pins;
                    const bool known = std::any_of(
                        owner_pins.begin(), owner_pins.end(),
                        [&](const PinDesc& p) { return p.name == pin->as_string(); });
                    if (!known && !owner_pins.empty()) {
                        out.push_back(make_diag(
                            Severity::Error, "E_UNKNOWN_PIN",
                            "pino inexistente: " + owner->as_string() + "." +
                                pin->as_string(),
                            base + "/pins"));
                    }
                    const std::string key = owner->as_string() + "." + pin->as_string();
                    if (++pin_usage[key] > 1) {
                        Diagnostic d = make_diag(
                            Severity::Error, "E_PIN_MULTI_NET",
                            "pino em mais de uma net: " + key, base);
                        d.net = net_id;
                        out.push_back(std::move(d));
                    }
                }
            }
            if (pin_count < 2) {
                Diagnostic d = make_diag(Severity::Warning, "W_DANGLING_NET",
                                         "net com menos de dois pinos", base);
                d.net = net_id;
                out.push_back(std::move(d));
            }
        }
    }

    // `pins` de cada dono deve concordar com as nets.
    for (const auto& [owner_id, owner] : owners) {
        const json::Value* obj = find_object(owner_array[owner_id], owner_id);
        if (!obj) continue;
        const json::Value* pin_map = obj->find("pins");
        if (!pin_map || !pin_map->is_object()) continue;
        for (const auto& [pin_name, assignment] : pin_map->as_object()) {
            const json::Value* net = assignment.find("net");
            if (!net || !net->is_string()) continue;
            if (!net_ids.count(net->as_string())) {
                out.push_back(make_diag(
                    Severity::Error, "E_UNKNOWN_NET",
                    "pino " + owner_id + "." + pin_name + " aponta para net inexistente: " +
                        net->as_string(),
                    "/" + owner_array[owner_id] + "/" + owner_id + "/pins"));
            }
        }
    }

    // Wires precisam de net existente.
    if (const json::Value* wires = document_.find("wires"); wires && wires->is_array()) {
        std::size_t index = 0;
        for (const auto& wire : wires->as_array()) {
            const std::string base = "/wires/" + std::to_string(index++);
            const json::Value* net = wire.find("net");
            if (!net || !net->is_string() || !net_ids.count(net->as_string())) {
                out.push_back(make_diag(Severity::Error, "E_WIRE_WITHOUT_NET",
                                        "fio sem net válida", base));
            }
            const json::Value* id = wire.find("id");
            if (!id || !id->is_string()) {
                out.push_back(make_diag(Severity::Error, "E_WIRE_ID",
                                        "fio sem 'id'", base));
            }
        }
    }

    // Firmware: caminho relativo seguro.
    if (const json::Value* firmware = document_.find("firmware");
        firmware && firmware->is_object()) {
        if (const json::Value* entry = firmware->find("entry");
            entry && entry->is_string() &&
            !is_safe_relative_path(entry->as_string())) {
            out.push_back(make_diag(Severity::Error, "E_UNSAFE_PATH",
                                    "firmware.entry deve ser relativo e sem '..'",
                                    "/firmware/entry"));
        }
    }

    // ERC mínima: curto entre trilhos.
    if (const json::Value* nets = document_.find("nets"); nets && nets->is_array()) {
        std::size_t index = 0;
        for (const auto& net : nets->as_array()) {
            const std::string base = "/nets/" + std::to_string(index++);
            bool has_power = false;
            bool has_ground = false;
            if (const json::Value* pins = net.find("pins"); pins && pins->is_array()) {
                for (const auto& ref : pins->as_array()) {
                    const json::Value* owner = ref.find("board");
                    const json::Value* pin = ref.find("pin");
                    if (!owner || !pin || !pin->is_string()) continue;
                    const auto it = owners.find(owner->as_string());
                    if (it == owners.end()) continue;
                    for (const auto& desc : it->second.pins) {
                        if (desc.name != pin->as_string()) continue;
                        if (desc.direction == PinDirection::Power) has_power = true;
                        if (desc.direction == PinDirection::Ground) has_ground = true;
                    }
                }
            }
            if (has_power && has_ground) {
                Diagnostic d = make_diag(Severity::Error, "E_SHORT",
                                         "curto entre alimentação e terra", base);
                d.fix = "separate_net";
                out.push_back(std::move(d));
            }
        }
    }

    return out;
}

}  // namespace simforge
