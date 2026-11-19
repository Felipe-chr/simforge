#include <filesystem>
#include <fstream>
#include <string>

#include "framework.hpp"
#include "simforge/ids.hpp"
#include "simforge/project.hpp"

using namespace simforge;
using simforge::json::Value;

namespace {

std::filesystem::path temp_file(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

Project make_project() {
    seed_ids(42);
    std::string error;
    Project project =
        Project::create("Bancada", "simforge.esp32s3.devkitc", &error);
    if (!error.empty()) ST_FAIL(error);
    return project;
}

}  // namespace

TEST(project_create_has_required_fields) {
    Project project = make_project();
    const Value& doc = project.document();
    CHECK(doc.find("format") != nullptr);
    CHECK_EQ(doc.find("format")->as_string(), std::string("simforge.project"));
    CHECK(doc.find("schema") != nullptr);
    CHECK(doc.find("boards") != nullptr);
    CHECK(doc.find("devices") != nullptr);
    CHECK(doc.find("nets") != nullptr);
    CHECK_EQ(project.name(), std::string("Bancada"));
    CHECK(!project.id().empty());
    CHECK(is_ulid(project.id()));
    CHECK_EQ(project.revision(), std::uint64_t(1));
}

TEST(project_create_rejects_unknown_board) {
    std::string error;
    Project project = Project::create("X", "simforge.naoexiste", &error);
    CHECK(!error.empty());
    (void)project;
}

TEST(project_create_without_board) {
    std::string error;
    Project project = Project::create("Vazio", "none", &error);
    CHECK(error.empty());
    const Value* boards = project.document().find("boards");
    CHECK(boards != nullptr);
    if (boards) CHECK_EQ(boards->size(), std::size_t(0));
}

TEST(project_validate_clean_document) {
    Project project = make_project();
    const auto diagnostics = project.validate();
    CHECK(!Project::has_errors(diagnostics));
}

TEST(project_validate_detects_broken_reference) {
    Project project = make_project();
    Value& doc = project.document();
    Value nets = Value::array();
    Value net = Value::object();
    net.set("id", Value("n1"));
    Value pins = Value::array();
    Value ref = Value::object();
    ref.set("device", Value("fantasma"));
    ref.set("pin", Value("A"));
    pins.push(std::move(ref));
    net.set("pins", std::move(pins));
    nets.push(std::move(net));
    doc.set("nets", std::move(nets));

    const auto diagnostics = project.validate();
    CHECK(Project::has_errors(diagnostics));
    bool found = false;
    for (const auto& d : diagnostics) {
        if (d.code == "E_UNKNOWN_OWNER") found = true;
    }
    CHECK(found);
}

TEST(project_validate_detects_short) {
    Project project = make_project();
    Value& doc = project.document();
    // Descobre o ID da placa criada.
    const Value* boards = doc.find("boards");
    CHECK(boards != nullptr && boards->size() == 1);
    const std::string board_id = boards->at(0).find("id")->as_string();

    Value nets = Value::array();
    Value net = Value::object();
    net.set("id", Value("n_short"));
    Value pins = Value::array();
    Value power_pin = Value::object();
    power_pin.set("board", Value(board_id));
    power_pin.set("pin", Value("3V3"));
    pins.push(std::move(power_pin));
    Value ground_pin = Value::object();
    ground_pin.set("board", Value(board_id));
    ground_pin.set("pin", Value("GND"));
    pins.push(std::move(ground_pin));
    net.set("pins", std::move(pins));
    nets.push(std::move(net));
    doc.set("nets", std::move(nets));

    const auto diagnostics = project.validate();
    bool found = false;
    for (const auto& d : diagnostics) {
        if (d.code == "E_SHORT") found = true;
    }
    CHECK(found);
}

TEST(project_roundtrip_preserves_document) {
    Project project = make_project();
    project.document().set("extensions", Value::object());
    project.document()["extensions"].set("acme", Value::object());
    project.document()["extensions"]["acme"].set("nota", Value("teste"));

    const auto path = temp_file("simforge_roundtrip.sforge");
    std::string error;
    CHECK(project.save(path, &error));
    if (!error.empty()) ST_FAIL(error);

    auto loaded = Project::load(path, &error);
    CHECK(error.empty());
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK_EQ(loaded->document().dump(), project.document().dump());
        // Campo desconhecido preservado.
        const Value* ext = loaded->document().find("extensions");
        CHECK(ext != nullptr);
        if (ext) {
            const Value* acme = ext->find("acme");
            CHECK(acme != nullptr);
            if (acme) {
                const Value* nota = acme->find("nota");
                CHECK(nota != nullptr);
                if (nota) CHECK_EQ(nota->as_string(), std::string("teste"));
            }
        }
    }
    std::filesystem::remove(path);
}

TEST(project_load_rejects_wrong_format) {
    const auto path = temp_file("simforge_bad_format.sforge");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << R"({"format":"outra.coisa","schema":1,"project":{},"boards":[],"devices":[],"nets":[]})";
    }
    std::string error;
    auto loaded = Project::load(path, &error);
    CHECK(!loaded.has_value());
    CHECK(!error.empty());
    std::filesystem::remove(path);
}

TEST(project_load_rejects_invalid_json) {
    const auto path = temp_file("simforge_bad_json.sforge");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "{ isto nao e json }";
    }
    std::string error;
    auto loaded = Project::load(path, &error);
    CHECK(!loaded.has_value());
    CHECK(error.find("JSON inválido") != std::string::npos);
    std::filesystem::remove(path);
}

TEST(project_load_missing_file) {
    std::string error;
    auto loaded = Project::load(temp_file("simforge_inexistente.sforge"), &error);
    CHECK(!loaded.has_value());
    CHECK(!error.empty());
}

TEST(project_example_blink_led_is_valid) {
    // O exemplo distribuído precisa continuar abrindo e validando limpo.
    // O caminho do fonte vem como UTF-8 do CMake; construir path a partir de
    // u8string evita depender do code page ANSI (o workspace tem "Área").
    const auto* source_bytes = reinterpret_cast<const char8_t*>(SIMFORGE_SOURCE_DIR);
    std::filesystem::path path = std::filesystem::path(std::u8string(source_bytes)) /
                                 "examples" / "blink-led.sforge";
    CHECK(std::filesystem::exists(path));

    std::string error;
    auto loaded = Project::load(path, &error);
    CHECK(error.empty());
    CHECK(loaded.has_value());
    if (!loaded) return;

    const auto diagnostics = loaded->validate();
    for (const auto& d : diagnostics) {
        ST_FAIL(d.code + ": " + d.message);
    }
    CHECK_EQ(diagnostics.size(), std::size_t(0));

    // O exemplo demonstra GPIO2 -> LED e GPIO0 -> botão.
    const Value* boards = loaded->document().find("boards");
    CHECK(boards != nullptr && boards->size() == 1);
    const Value* devices = loaded->document().find("devices");
    CHECK(devices != nullptr && devices->size() == 2);
    const Value* nets = loaded->document().find("nets");
    CHECK(nets != nullptr && nets->size() == 3);
}

TEST(project_revision_bumps) {
    Project project = make_project();
    CHECK_EQ(project.revision(), std::uint64_t(1));
    project.bump_revision();
    CHECK_EQ(project.revision(), std::uint64_t(2));
    project.set_revision(10);
    CHECK_EQ(project.revision(), std::uint64_t(10));
}

TEST(ffi_ids_are_unique_and_valid) {
    seed_ids(7);
    std::string previous = new_ulid();
    CHECK(is_ulid(previous));
    for (int i = 0; i < 1000; ++i) {
        const std::string id = new_ulid();
        CHECK(is_ulid(id));
        CHECK(id != previous);
        previous = id;
    }
    CHECK(!is_ulid("curto"));
    CHECK(!is_ulid("0000000000000000000000000O"));
}
