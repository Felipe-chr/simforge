#include "simforge/c_api/simforge.h"

#include <cstring>
#include <exception>
#include <new>
#include <string>

#include "simforge/session.hpp"

namespace {

thread_local std::string g_last_error;

void set_error(const std::string& message) { g_last_error = message; }

/// Copia `text` para o buffer do chamador seguindo o contrato da ABI.
int copy_out(const std::string& text, char* response, size_t capacity,
             size_t* written) {
    const size_t needed = text.size() + 1;  // inclui terminador
    if (written) *written = needed;
    if (!response) return SF_ERR_INVALID_ARGUMENT;
    if (capacity < needed) return SF_ERR_BUFFER;
    std::memcpy(response, text.c_str(), needed);
    return SF_OK;
}

}  // namespace

struct sf_session {
    simforge::Session session;
    sf_event_cb cb = nullptr;
    void* user = nullptr;
    std::uint64_t push_seq = 0;
};

extern "C" {

SF_API const char* sf_version(void) {
    return simforge::Session::product_version();
}

SF_API int sf_api_version(void) {
    return simforge::Session::api_version();
}

SF_API int sf_session_create(const char* config_json, sf_session** out) {
    (void)config_json;  // configuração entra na v1.1
    if (!out) {
        set_error("sf_session_create: 'out' nulo");
        return SF_ERR_INVALID_ARGUMENT;
    }
    try {
        auto* handle = new sf_session();
        *out = handle;
        set_error("");
        return SF_OK;
    } catch (const std::bad_alloc&) {
        set_error("sf_session_create: sem memória");
        return SF_ERR_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        set_error(std::string("sf_session_create: ") + e.what());
        return SF_ERR_INTERNAL;
    }
}

SF_API int sf_session_execute(sf_session* session, const char* request_json,
                              char* response, size_t capacity, size_t* written) {
    if (!session) {
        set_error("sf_session_execute: sessão nula");
        return SF_ERR_INVALID_ARGUMENT;
    }
    if (!request_json) {
        set_error("sf_session_execute: requisição nula");
        return SF_ERR_INVALID_ARGUMENT;
    }

    int status = SF_OK;
    try {
        const std::string text = session->session.execute(request_json);
        status = copy_out(text, response, capacity, written);
        if (status == SF_OK) {
            set_error("");
        } else if (status == SF_ERR_BUFFER) {
            set_error("sf_session_execute: buffer insuficiente");
        }

        // Push de eventos pendentes (se houver assinatura).
        if (session->cb) {
            const simforge::EventBatch batch =
                session->session.poll_events(session->push_seq, 1024);
            for (const auto& event : batch.events) {
                session->cb(session->user, event.dump(0).c_str());
            }
            session->push_seq = batch.next_seq;
        }
    } catch (const std::bad_alloc&) {
        set_error("sf_session_execute: sem memória");
        return SF_ERR_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        set_error(std::string("sf_session_execute: ") + e.what());
        return SF_ERR_INTERNAL;
    }
    return status;
}

SF_API int sf_session_subscribe(sf_session* session, const char* topics,
                                sf_event_cb cb, void* user) {
    if (!session) {
        set_error("sf_session_subscribe: sessão nula");
        return SF_ERR_INVALID_ARGUMENT;
    }
    (void)topics;  // filtragem por tópico entra na v1.1
    session->cb = cb;
    session->user = user;
    set_error("");
    return SF_OK;
}

SF_API int sf_session_last_error(char* buffer, size_t capacity) {
    if (!buffer) {
        set_error("sf_session_last_error: buffer nulo");
        return SF_ERR_INVALID_ARGUMENT;
    }
    const size_t needed = g_last_error.size() + 1;
    if (capacity < needed) {
        return SF_ERR_BUFFER;
    }
    std::memcpy(buffer, g_last_error.c_str(), needed);
    return SF_OK;
}

SF_API void sf_session_destroy(sf_session* session) { delete session; }

}  // extern "C"
