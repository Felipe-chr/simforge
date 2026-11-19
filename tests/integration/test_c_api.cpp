#include <string>
#include <vector>

#include "framework.hpp"
#include "simforge/c_api/simforge.h"

namespace {

int g_event_count = 0;
std::string g_last_event;

void on_event(void* user, const char* event_json) {
    (void)user;
    ++g_event_count;
    if (event_json) g_last_event = event_json;
}

/// Executa uma requisição com buffer suficiente e devolve a resposta.
std::string run(sf_session* session, const char* request) {
    size_t needed = 0;
    char probe[1];
    int rc = sf_session_execute(session, request, probe, sizeof(probe), &needed);
    if (rc != SF_ERR_BUFFER && rc != SF_OK) return {};
    std::vector<char> buffer(needed > 0 ? needed : 1);
    size_t written = 0;
    rc = sf_session_execute(session, request, buffer.data(), buffer.size(), &written);
    if (rc != SF_OK) return {};
    return std::string(buffer.data());
}

}  // namespace

TEST(c_api_version_symbols) {
    CHECK_EQ(std::string(sf_version()), std::string("0.1.0"));
    CHECK_EQ(sf_api_version(), 1);
}

TEST(c_api_session_lifecycle) {
    sf_session* session = nullptr;
    CHECK_EQ(sf_session_create(nullptr, &session), SF_OK);
    CHECK(session != nullptr);
    sf_session_destroy(session);
}

TEST(c_api_rejects_null_arguments) {
    CHECK_EQ(sf_session_create(nullptr, nullptr), SF_ERR_INVALID_ARGUMENT);
    CHECK_EQ(sf_session_execute(nullptr, "{}", nullptr, 0, nullptr),
             SF_ERR_INVALID_ARGUMENT);
    CHECK_EQ(sf_session_subscribe(nullptr, nullptr, nullptr, nullptr),
             SF_ERR_INVALID_ARGUMENT);
    CHECK_EQ(sf_session_last_error(nullptr, 0), SF_ERR_INVALID_ARGUMENT);
}

TEST(c_api_buffer_reports_required_size) {
    sf_session* session = nullptr;
    CHECK_EQ(sf_session_create(nullptr, &session), SF_OK);
    const char* request = R"({"id":"1","method":"app.ping","params":{"nonce":"abc"}})";

    char small[4] = {};
    size_t needed = 0;
    const int rc =
        sf_session_execute(session, request, small, sizeof(small), &needed);
    CHECK_EQ(rc, SF_ERR_BUFFER);
    CHECK(needed > sizeof(small));

    std::vector<char> buffer(needed);
    size_t written = 0;
    CHECK_EQ(sf_session_execute(session, request, buffer.data(), buffer.size(),
                                &written),
             SF_OK);
    CHECK_EQ(written, needed);
    const std::string text(buffer.data());
    CHECK(text.find("\"ok\":true") != std::string::npos);
    CHECK(text.find("\"nonce\":\"abc\"") != std::string::npos);

    sf_session_destroy(session);
}

TEST(c_api_unknown_method_is_a_stable_error) {
    sf_session* session = nullptr;
    CHECK_EQ(sf_session_create(nullptr, &session), SF_OK);
    const std::string text =
        run(session, R"({"id":"9","method":"nao.existe","params":{}})");
    CHECK(text.find("\"ok\":false") != std::string::npos);
    CHECK(text.find("\"code\":\"unknown_method\"") != std::string::npos);
    sf_session_destroy(session);
}

TEST(c_api_invalid_json) {
    sf_session* session = nullptr;
    CHECK_EQ(sf_session_create(nullptr, &session), SF_OK);
    const std::string text = run(session, "{ nao e json }");
    CHECK(text.find("\"code\":\"invalid_request\"") != std::string::npos);
    sf_session_destroy(session);
}

TEST(c_api_subscribe_receives_events) {
    sf_session* session = nullptr;
    CHECK_EQ(sf_session_create(nullptr, &session), SF_OK);
    g_event_count = 0;
    g_last_event.clear();
    CHECK_EQ(sf_session_subscribe(session, nullptr, &on_event, nullptr), SF_OK);

    const std::string text =
        run(session, R"({"id":"1","method":"project.new","params":{}})");
    CHECK(text.find("\"ok\":true") != std::string::npos);
    CHECK(g_event_count >= 1);
    CHECK(g_last_event.find("\"type\":") != std::string::npos);
    CHECK(g_last_event.find("project.changed") != std::string::npos);

    sf_session_destroy(session);
}
