// Garante que o header guarda-chuva compila sozinho, com /W4 e sem
// dependências ocultas. Se este arquivo quebrar, a API pública está inconsistente.

#include "framework.hpp"
#include "simforge/simforge.hpp"

TEST(umbrella_header_compiles) {
    using namespace simforge::literals;
    const simforge::SimTime t = 1_ms;
    CHECK_EQ(t, simforge::SimTime(1000000));
    CHECK_EQ(t, simforge::kNsPerMs);
    CHECK(!simforge::format_sim_time(t).empty());

    simforge::Session session;
    const std::string response =
        session.execute(R"({"id":"1","method":"app.ping","params":{}})");
    CHECK(response.find("\"ok\":true") != std::string::npos);
}
