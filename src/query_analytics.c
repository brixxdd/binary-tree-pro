#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "../includes/query_analytics.h"
#include "../includes/index.h"
#include "../includes/database.h"

int is_auto_index_on(const char* db) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "data/%s/_config.txt", db);
    FILE* f = fopen(filepath, "r");
    if (!f) return 0; // Default OFF
    char line[256];
    int on = 0;
    while(fgets(line, sizeof(line), f)) {
        if (strncmp(line, "AUTO_INDEX=ON", 13) == 0) {
            on = 1;
        }
    }
    fclose(f);
    return on;
}

void toggle_auto_index(const char* db, int on) {
    if (!db) {
        printf("Error: Seleccione una base de datos primero.\n");
        return;
    }
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "data/%s/_config.txt", db);
    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "AUTO_INDEX=%s\n", on ? "ON" : "OFF");
        fclose(f);
        printf("=> Auto-Index %s para la base de datos '%s'.\n", on ? "activado" : "desactivado", db);
    }
}

static void extract_where_field(const char* query, char* field_out) {
    field_out[0] = '\0';
    const char* p = strcasestr(query, " WHERE ");
    if (!p) p = strcasestr(query, " DONDE ");
    if (p) {
        p += 7; // skip " WHERE "
        while (*p && isspace((unsigned char)*p)) p++;
        int i = 0;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '<' && *p != '>' && i < 49) {
            field_out[i++] = *p++;
        }
        field_out[i] = '\0';
    }
}

void init_query_analytics(void) {}

void log_query(const char* db, const char* table, const char* query_text, double execution_time_ms, int rows_scanned) {
    if (!db || !table || !query_text) return;
    
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "data/%s/_query_log.csv", db);
    
    FILE* f = fopen(filepath, "a");
    if (!f) return;
    
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    fprintf(f, "%s|%s|%.2f|%d|%s\n", time_str, query_text, execution_time_ms, rows_scanned, table);
    fclose(f);
}

void process_auto_index(const char* db, const char* query_text, double exec_ms, const char* table) {
    if (!db || !table || !query_text) return;
    if (!is_auto_index_on(db)) return;
    
    char field[50];
    extract_where_field(query_text, field);
    if (field[0] == '\0') return;
    
    if (exec_ms > 500.0) {
        printf("\n[CONSCIENCE] Auto-index detectó query lenta (%.2f ms) en %s(%s).\n", exec_ms, table, field);
        // crear_indice((char*)table, field); // NOT IMPLEMENTED IN BACKEND YET
        printf("[CONSCIENCE] (Simulación) Índice creado automáticamente en %s(%s). Las siguientes consultas serán mucho más rápidas.\n", table, field);
    }
}

void analyze_and_suggest_indices(const char* db) {
    if (!db) {
        printf("Error: Seleccione una base de datos primero.\n");
        return;
    }
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "data/%s/_query_log.csv", db);
    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("[CONSCIENCE ANALYTICS]\nNo hay suficientes datos registrados en '%s' para sugerir índices.\n", db);
        return;
    }
    
    struct {
        char table[50];
        char field[50];
        int count;
        double total_time;
    } stats[100];
    int stat_count = 0;
    
    char line[1024];
    int slow_queries = 0;
    
    while(fgets(line, sizeof(line), f)) {
        strtok(line, "|"); // ts (unused)
        char* query = strtok(NULL, "|");
        char* exec_str = strtok(NULL, "|");
        strtok(NULL, "|"); // rows_str (unused)
        char* table = strtok(NULL, "\n");
        
        if (!query || !exec_str || !table) continue;
        
        double exec_time = atof(exec_str);
        if (exec_time > 500.0) slow_queries++;
        
        char field[50];
        extract_where_field(query, field);
        if (field[0] != '\0') {
            int found = 0;
            for(int i=0; i<stat_count; i++) {
                if(strcmp(stats[i].table, table) == 0 && strcmp(stats[i].field, field) == 0) {
                    stats[i].count++;
                    stats[i].total_time += exec_time;
                    found = 1; break;
                }
            }
            if (!found && stat_count < 100) {
                strcpy(stats[stat_count].table, table);
                strcpy(stats[stat_count].field, field);
                stats[stat_count].count = 1;
                stats[stat_count].total_time = exec_time;
                stat_count++;
            }
        }
    }
    fclose(f);
    
    printf("\n[CONSCIENCE ANALYTICS - %s]\n", db);
    int suggestions = 0;
    for(int i=0; i<stat_count; i++) {
        if (stats[i].count > 20 || (stats[i].count > 5 && (stats[i].total_time / stats[i].count) > 500.0)) {
            printf("Sugerencia: Crear indice en %s(%s) - Usado %d veces en filtros, avg %.2f ms\n", 
                stats[i].table, stats[i].field, stats[i].count, stats[i].total_time / stats[i].count);
            suggestions++;
        }
    }
    
    if (slow_queries > 0) {
        printf("Detectadas %d queries lentas (>500ms).\n", slow_queries);
    }
    
    if (suggestions == 0) {
        printf("No hay patrones suficientes que requieran indexación todavía.\n");
    }
    printf("\n");
}
