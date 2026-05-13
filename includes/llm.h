#ifndef LLM_H
#define LLM_H

#include <stdbool.h>
#include <sys/types.h>

#define LLM_MAX_RESPONSE 2048
#define LLM_MAX_CONTEXT 2048

// MiniMax API configuration - set via environment or change here
#define MINIMAX_API_KEY "sk-cp-_-rPYpeRLX02a2Jzq-dQ1oN9gJ0jYDHQh6R1TvCrwF9qyaVsJ5zAZEUQfMnV7zifB7J3CtqD4sAVajbNr-avF9OPB5K5ZmdfbXXBLsd_7IM3gw9ZAGAUomQ"
#define MINIMAX_API_URL "https://api.minimax.io/v1/chat/completions"
#define MINIMAX_MODEL "MiniMax-M2.7"

typedef struct {
    void * ctx;           // unused for API mode
    void * model;         // unused for API mode
    int n_ctx;            // context size
    int n_threads;        // unused for API mode
    bool initialized;     // init flag
    bool enabled;         // conscience enabled
    bool use_api;         // true if using cloud API
} LLMState;

int llm_init(const char * api_key);
void llm_cleanup(void);
const char * llm_think(const char * prompt);
bool llm_is_enabled(void);
bool llm_is_initialized(void);

// Alternative: init with custom API key
int llm_init_with_url(const char * api_key, const char * api_url, const char * model_name);

#endif