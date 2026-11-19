#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "simforge/json.hpp"
#include "simforge/session.hpp"

namespace {

using simforge::json::ParseError;
using simforge::json::Value;

int print_usage() {
    std::puts(
        "simforge-cli 0.1.0\n"
        "Uso:\n"
        "  simforge-cli version\n"
        "  simforge-cli api                 # NDJSON em stdin/stdout\n"
        "  simforge-cli validate <arquivo>  # valida um projeto .sforge\n"
        "  simforge-cli run <arquivo> [--until-ns N]\n");
    return 1;
}

/// Monta a requisição do envelope api-v1.
std::string request(const char* id, const char* method, Value params) {
    Value req = Value::object();
    req.set("id", id);
    req.set("method", method);
    req.set("params", std::move(params));
    return req.dump(0);
}

int cmd_version() {
    std::printf("simforge %s (api v%d)\n", simforge::Session::product_version(),
                simforge::Session::api_version());
    return 0;
}

int cmd_api() {
    simforge::Session session;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::cout << session.execute(line) << '\n';
        std::cout.flush();
    }
    return 0;
}

/// Abre o projeto na sessão; devolve o documento de resposta ou nullptr.
bool open_project(simforge::Session& session, const std::string& path,
                  Value& response) {
    Value params = Value::object();
    params.set("path", path);
    ParseError err;
    response = simforge::json::parse(
        session.execute(request("cli-open", "project.open", std::move(params))),
        &err);
    if (!err.ok()) return false;
    const Value* ok = response.find("ok");
    return ok && ok->is_bool() && ok->as_bool();
}

int cmd_validate(const std::string& path) {
    simforge::Session session;
    Value response;
    if (!open_project(session, path, response)) {
        const Value* error = response.find("error");
        std::fprintf(stderr, "falha ao abrir: %s\n",
                     error ? error->dump(0).c_str() : "erro desconhecido");
        return 2;
    }

    ParseError err;
    Value validation = simforge::json::parse(session.execute(request(
        "cli-validation", "project.validate", Value::object())), &err);
    if (!err.ok()) {
        std::fprintf(stderr, "resposta inválida\n");
        return 2;
    }
    const Value* result = validation.find("result");
    const Value* diagnostics = result ? result->find("diagnostics") : nullptr;
    if (!diagnostics || !diagnostics->is_array()) {
        std::fprintf(stderr, "sem diagnósticos\n");
        return 2;
    }

    int errors = 0;
    int warnings = 0;
    for (const auto& d : diagnostics->as_array()) {
        const Value* severity = d.find("severity");
        const Value* code = d.find("code");
        const Value* message = d.find("message");
        const Value* dpath = d.find("path");
        if (severity && severity->is_string()) {
            if (severity->as_string() == "error") ++errors;
            if (severity->as_string() == "warning") ++warnings;
        }
        std::printf("%s %s %s %s\n",
                    severity && severity->is_string() ? severity->as_string().c_str() : "?",
                    code && code->is_string() ? code->as_string().c_str() : "?",
                    dpath && dpath->is_string() ? dpath->as_string().c_str() : "",
                    message && message->is_string() ? message->as_string().c_str() : "");
    }
    std::printf("\n%d erro(s), %d aviso(s)\n", errors, warnings);
    return errors > 0 ? 1 : 0;
}

int cmd_run(const std::string& path, std::int64_t until_ns) {
    simforge::Session session;
    Value response;
    if (!open_project(session, path, response)) {
        const Value* error = response.find("error");
        std::fprintf(stderr, "falha ao abrir: %s\n",
                     error ? error->dump(0).c_str() : "erro desconhecido");
        return 2;
    }

    ParseError err;
    Value loaded = simforge::json::parse(
        session.execute(request("cli-load", "sim.load", Value::object())), &err);
    const Value* ok = loaded.find("ok");
    if (!err.ok() || !ok || !ok->as_bool()) {
        const Value* error = loaded.find("error");
        std::fprintf(stderr, "falha ao carregar: %s\n",
                     error ? error->dump(0).c_str() : "erro desconhecido");
        return 2;
    }

    Value params = Value::object();
    if (until_ns > 0) params.set("until_ns", static_cast<long long>(until_ns));
    Value ran = simforge::json::parse(
        session.execute(request("cli-run", "sim.run", std::move(params))), &err);
    ok = ran.find("ok");
    if (!err.ok() || !ok || !ok->as_bool()) {
        const Value* error = ran.find("error");
        std::fprintf(stderr, "falha ao executar: %s\n",
                     error ? error->dump(0).c_str() : "erro desconhecido");
        return 2;
    }

    const Value* result = ran.find("result");
    std::printf("%s\n", result ? result->dump(2).c_str() : "{}");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return print_usage();
    const char* cmd = argv[1];

    if (std::strcmp(cmd, "version") == 0) return cmd_version();
    if (std::strcmp(cmd, "api") == 0) return cmd_api();
    if (std::strcmp(cmd, "validate") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "uso: simforge-cli validate <arquivo>\n");
            return 1;
        }
        return cmd_validate(argv[2]);
    }
    if (std::strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "uso: simforge-cli run <arquivo> [--until-ns N]\n");
            return 1;
        }
        std::int64_t until = 0;
        for (int i = 3; i + 1 < argc; ++i) {
            if (std::strcmp(argv[i], "--until-ns") == 0) {
                until = std::strtoll(argv[i + 1], nullptr, 10);
                ++i;
            }
        }
        return cmd_run(argv[2], until);
    }
    return print_usage();
}
