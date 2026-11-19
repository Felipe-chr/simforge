#ifndef SIMFORGE_C_API_H
#define SIMFORGE_C_API_H

/*
 * API C do SimForge — fronteira binária estável do produto (ADR 0001).
 *
 * Regras:
 *  - Toda função devolve `int`: 0 = sucesso, negativo = erro (ver sf_status).
 *  - Strings são UTF-8 terminadas em '\0'.
 *  - `response` pertence ao chamador; se `capacity` for insuficiente,
 *    devolve SF_ERR_BUFFER e escreve em `written` o tamanho necessário
 *    (incluindo o terminador).
 *  - Sessões são independentes; `sf_session_execute` é thread-safe.
 */

#include <stddef.h>

#if defined(_WIN32)
#  if defined(SF_BUILD_SHARED)
#    define SF_API __declspec(dllexport)
#  else
#    define SF_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define SF_API __attribute__((visibility("default")))
#else
#  define SF_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SF_API_VERSION 1

typedef struct sf_session sf_session;

typedef enum sf_status {
    SF_OK = 0,
    SF_ERR_INVALID_ARGUMENT = -1,
    SF_ERR_OUT_OF_MEMORY = -2,
    SF_ERR_BUFFER = -3,
    SF_ERR_STATE = -4,
    SF_ERR_UNSUPPORTED = -5,
    SF_ERR_INTERNAL = -6
} sf_status;

typedef void (*sf_event_cb)(void* user, const char* event_json);

/* Versão do produto, ex.: "0.1.0". */
SF_API const char* sf_version(void);

/* Versão major do contrato de envelope (api-v1). */
SF_API int sf_api_version(void);

/* Cria uma sessão. `config_json` pode ser NULL (padrões). */
SF_API int sf_session_create(const char* config_json, sf_session** out);

/* Executa um comando e devolve a resposta JSON. */
SF_API int sf_session_execute(sf_session* session, const char* request_json,
                              char* response, size_t capacity, size_t* written);

/* Assina tópicos de evento (prefixos separados por vírgula; NULL = todos). */
SF_API int sf_session_subscribe(sf_session* session, const char* topics,
                                sf_event_cb cb, void* user);

/* Mensagem do último erro da thread, para diagnóstico. */
SF_API int sf_session_last_error(char* buffer, size_t capacity);

SF_API void sf_session_destroy(sf_session* session);

#ifdef __cplusplus
}  /* extern C */
#endif

#endif /* SIMFORGE_C_API_H */
