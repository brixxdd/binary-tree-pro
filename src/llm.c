#include "llm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static struct {
    bool initialized;
    bool enabled;
    bool use_api;
    char api_key[512];
    char api_url[512];
    char model[64];
} state = {0};

static char response_buffer[4096];
static char json_buffer[16384];

int llm_init(const char * api_key) {
    if (state.initialized) {
        return 0;
    }

    // Use provided key or default from header
    if (api_key && strlen(api_key) > 5) {
        strncpy(state.api_key, api_key, sizeof(state.api_key) - 1);
    } else {
        strncpy(state.api_key, MINIMAX_API_KEY, sizeof(state.api_key) - 1);
    }

    strncpy(state.api_url, "https://api.minimax.io/v1/chat/completions", sizeof(state.api_url) - 1);
    strncpy(state.model, "MiniMax-M2.7", sizeof(state.model) - 1);

    state.initialized = true;
    state.enabled = true;
    state.use_api = true;

    printf("[LLM] MiniMax API mode initialized. Model: %s\n", state.model);
    return 0;
}

int llm_init_with_url(const char * api_key, const char * api_url, const char * model_name) {
    if (state.initialized) {
        return 0;
    }

    if (!api_key || strlen(api_key) < 5) {
        fprintf(stderr, "[LLM] Error: Invalid API key\n");
        return -1;
    }

    strncpy(state.api_key, api_key, sizeof(state.api_key) - 1);
    strncpy(state.api_url, api_url, sizeof(state.api_url) - 1);
    strncpy(state.model, model_name, sizeof(state.model) - 1);

    state.initialized = true;
    state.enabled = true;
    state.use_api = true;

    printf("[LLM] MiniMax API mode initialized. URL: %s, Model: %s\n", state.api_url, state.model);
    return 0;
}

void llm_cleanup(void) {
    state.initialized = false;
    state.enabled = false;
    state.use_api = false;
}

const char * llm_think(const char * prompt) {
    if (!state.initialized || !state.enabled) {
        return "[LLM] Error: LLM not initialized or disabled\n";
    }

    memset(response_buffer, 0, sizeof(response_buffer));
    memset(json_buffer, 0, sizeof(json_buffer));

    // Escape special characters in prompt for JSON
    char escaped_prompt[8192];
    int ep = 0;
    for (int i = 0; prompt[i] && ep < (int)(sizeof(escaped_prompt) - 2); i++) {
        if (prompt[i] == '\\') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = '\\';
        } else if (prompt[i] == '"') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = '"';
        } else if (prompt[i] == '\'') {
            escaped_prompt[ep++] = '\'';
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = '\'';
            escaped_prompt[ep++] = '\'';
        } else if (prompt[i] == '\n') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = 'n';
        } else if (prompt[i] == '\r') {
            // skip CR
        } else {
            escaped_prompt[ep++] = prompt[i];
        }
    }
    escaped_prompt[ep] = '\0';

    // Build JSON body with properly escaped prompt
    snprintf(json_buffer, sizeof(json_buffer),
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"Eres MOTOR CONSCIENCE, optimizador de BD. Responde conciso, máximo 5 líneas. Usa formato: 1) 2) 3) 4). Cuando sugieras índices usa la sintaxis: CREAR INDICE <tabla> <campo>.\"},{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":512,\"temperature\":0.7}",
        state.model, escaped_prompt);

    // Build curl command
    char cmd[32768];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST 'https://api.minimax.io/v1/chat/completions' "
        "-H 'Authorization: Bearer %s' "
        "-H 'Content-Type: application/json' "
        "-d '%s' "
        "> /tmp/llm_resp.json 2>&1",
        state.api_key, json_buffer);

    system(cmd);

    // Read raw response
    FILE * f = fopen("/tmp/llm_resp.json", "r");
    if (!f) {
        return "[LLM] Error: cannot read response file\n";
    }

    int i = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF && i < (int)sizeof(response_buffer) - 1) {
        response_buffer[i++] = (char)ch;
    }
    response_buffer[i] = '\0';
    fclose(f);

    // Check for error
    if (strstr(response_buffer, "\"error\"")) {
        snprintf(response_buffer, sizeof(response_buffer), "[LLM] API Error");
        return response_buffer;
    }

    // Parse JSON properly - extract "content" field value
    // Format: {"content":"text here..."} - we need to find content field
    char *content_start = strstr(response_buffer, "\"content\":\"");
    if (content_start) {
        content_start += 11; // skip {"content":"

        // Find the closing " after content (before ,"name or ,"role)
        char *p = content_start;
        while (*p) {
            if (*p == '"' && (p[1] == ',' || p[1] == '}')) {
                int len = (int)(p - content_start);
                if (len > 2047) len = 2047;
                memcpy(response_buffer, content_start, len);
                response_buffer[len] = '\0';
                break;
            }
            p++;
        }
    } else {
        snprintf(response_buffer, sizeof(response_buffer), "[LLM] Parse error");
        return response_buffer;
    }

    // Unescape newlines
    int dst = 0;
    for (int j = 0; response_buffer[j] && j < 4095; j++) {
        if (response_buffer[j] == '\\' && response_buffer[j+1] == 'n') {
            response_buffer[dst++] = '\n';
            j++;
        } else {
            response_buffer[dst++] = response_buffer[j];
        }
    }
    response_buffer[dst] = '\0';

    // Remove <think>...  分析 blocks - state machine approach
    {
        char *src = response_buffer;
        char *dst = response_buffer;
        int in_think = 0;

        while (*src) {
            if (!in_think) {
                // Check for either opening tag
                if (strncmp(src, "[ 分析", 8) == 0) {
                    in_think = 1;
                    src += 8;
                } else if (strncmp(src, "<think>", 7) == 0) {
                    in_think = 1;
                    src += 7;
                } else {
                    *dst++ = *src++;
                }
            } else {
                // In think block - look for either closing tag
                if (strncmp(src, " 分析", 7) == 0) {
                    in_think = 0;
                    src += 7;
                    if (*src == '>') src++;
                } else if (strncmp(src, "</think>", 8) == 0) {
                    in_think = 0;
                    src += 8;
                } else {
                    src++;
                }
            }
        }
        *dst = '\0';
    }

    return response_buffer[0] ? response_buffer : "[LLM] No response\n";
}

bool llm_is_enabled(void) {
    return state.enabled;
}

bool llm_is_initialized(void) {
    return state.initialized;
}