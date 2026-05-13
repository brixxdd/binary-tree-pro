#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>

#include "../includes/config.h"
#include "../includes/types.h"
#include "../includes/errors.h"
#include "../includes/transaction.h"
#include "../includes/index.h"
#include "../includes/database.h"
#include "../includes/io.h"
#include "../includes/llm.h"
#include "../includes/query_analytics.h"
#include <sys/time.h>

static int parsear_esquema(const char* esquema, Campo* campos, int* num_campos) {
    *num_campos = 0;

    const char* inicio = strchr(esquema, '(');
    if (!inicio) return -1;

    const char* fin = esquema + strlen(esquema) - 1;
    while (fin > inicio && *fin != ')') fin--;

    if (fin <= inicio) return -1;

    char contenido[500];
    size_t len = fin - inicio - 1;
    if (len >= sizeof(contenido)) return -1;
    strncpy(contenido, inicio + 1, len);
    contenido[len] = '\0';

    char* trabajo = malloc(len + 1);
    if (!trabajo) return -1;
    strcpy(trabajo, contenido);

    char* campo_start = trabajo;

    for (int i = 0; trabajo[i] != '\0'; i++) {
        if (trabajo[i] == ',') {
            trabajo[i] = '\0';
            while (*campo_start == ' ' || *campo_start == '\t') campo_start++;

            char nombre[MAX_NOMBRE];
            char tipo_str[20];

            if (sscanf(campo_start, "%49s %19s", nombre, tipo_str) == 2) {
                if (strcasecmp(tipo_str, "INT") == 0) {
                    campos[*num_campos].tipo = TIPO_INT;
                    campos[*num_campos].longitud = sizeof(int);
                } else if (strcasecmp(tipo_str, "FLOAT") == 0 || strcasecmp(tipo_str, "REAL") == 0) {
                    campos[*num_campos].tipo = TIPO_FLOAT;
                    campos[*num_campos].longitud = sizeof(float);
                } else if (strncmp(tipo_str, "STRING", 6) == 0 || strncmp(tipo_str, "string", 6) == 0) {
                    campos[*num_campos].tipo = TIPO_STRING;
                    campos[*num_campos].longitud = 50;
                    char* p = strchr(tipo_str, '(');
                    if (p && sscanf(p + 1, "%d", &campos[*num_campos].longitud) != 1) {
                        campos[*num_campos].longitud = 50;
                    }
                } else {
                    campo_start = &trabajo[i + 1];
                    continue;
                }
                strcpy(campos[*num_campos].nombre, nombre);
                (*num_campos)++;
            }

            campo_start = &trabajo[i + 1];
        }
    }

    if (*campo_start != '\0') {
        while (*campo_start == ' ' || *campo_start == '\t') campo_start++;

        char nombre[MAX_NOMBRE];
        char tipo_str[20];

        if (sscanf(campo_start, "%49s %19s", nombre, tipo_str) == 2) {
            if (strcasecmp(tipo_str, "INT") == 0) {
                campos[*num_campos].tipo = TIPO_INT;
                campos[*num_campos].longitud = sizeof(int);
            } else if (strcasecmp(tipo_str, "FLOAT") == 0 || strcasecmp(tipo_str, "REAL") == 0) {
                campos[*num_campos].tipo = TIPO_FLOAT;
                campos[*num_campos].longitud = sizeof(float);
            } else if (strncmp(tipo_str, "STRING", 6) == 0 || strncmp(tipo_str, "string", 6) == 0) {
                campos[*num_campos].tipo = TIPO_STRING;
                campos[*num_campos].longitud = 50;
                char* p = strchr(tipo_str, '(');
                if (p && sscanf(p + 1, "%d", &campos[*num_campos].longitud) != 1) {
                    campos[*num_campos].longitud = 50;
                }
            }
            strcpy(campos[*num_campos].nombre, nombre);
            (*num_campos)++;
        }
    }

    free(trabajo);
    return (*num_campos > 0) ? 0 : -1;
}

static int parsear_valores_insert(const char* input, char** valores, int* num_valores) {
    *num_valores = 0;

    const char* paren_start = strchr(input, '(');
    const char* paren_end = strchr(input, ')');

    if (!paren_start || !paren_end || paren_end <= paren_start) {
        return -1;
    }

    size_t len = paren_end - paren_start - 1;
    if (len >= 500) return -1;

    char contenido[500];
    strncpy(contenido, paren_start + 1, len);
    contenido[len] = '\0';

    char* trabajo = malloc(len + 1);
    if (!trabajo) return -1;
    strcpy(trabajo, contenido);

    char* token = strtok(trabajo, ",");

    while (token && *num_valores < MAX_CAMPOS) {
        while (*token == ' ' || *token == '\t') token++;

        if (*token == '\'' || *token == '"') {
            char* end = token + strlen(token) - 1;
            if (*end == '\'' || *end == '"') {
                *end = '\0';
                token++;
            }
        }

        valores[*num_valores] = malloc(strlen(token) + 1);
        if (valores[*num_valores]) {
            strcpy(valores[*num_valores], token);
            (*num_valores)++;
        }

        token = strtok(NULL, ",");
    }

    free(trabajo);
    return (*num_valores > 0) ? 0 : -1;
}

static char* saltar_espacios(char* str) {
    while (*str == ' ') str++;
    return str;
}

// FASE 4: Natural Language Interface
void parse_and_execute(char* input); // Forward declaration

static void process_natural_language(const char* input) {
    if (!llm_is_initialized()) {
        printf("Error: Conscience LLM no está activo para traducir lenguaje natural.\n");
        return;
    }

    printf("\n[CONSCIENCE NL] Procesando lenguaje natural...\n");

    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
        "Traduce esta orden natural al dialecto SQL de este motor.\n"
        "Dialecto motor:\n"
        "- SELECCIONAR <tabla> WHERE <condicion>\n"
        "- INSERTAR <tabla> (val1, val2)\n"
        "Si pide 'CONTAR', usa 'SELECCIONAR <tabla> WHERE <condicion>' y el motor lo entenderá.\n"
        "Solo responde con la query traducida, sin comillas extra ni backticks ni formato markdown.\n"
        "Orden: %s", input);

    const char* raw_translation = llm_think(prompt);
    
    // Limpiar espacios y saltos de línea al principio
    const char* translation = raw_translation;
    while (isspace((unsigned char)*translation)) translation++;
    
    printf("\n[CONSCIENCE NL] Entendí esto:\n> %s\n\n¿Ejecutar directo? (S/N): ", translation);
    fflush(stdout);
    
    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin)) {
        if (confirm[0] == 'S' || confirm[0] == 's') {
            char query_to_run[1024];
            strncpy(query_to_run, translation, sizeof(query_to_run)-1);
            query_to_run[sizeof(query_to_run)-1] = '\0';
            printf("\n");
            parse_and_execute(query_to_run);
        } else {
            printf("Operación cancelada.\n");
        }
    }
}

void parse_and_execute(char* input) {
    while (isspace(*input)) input++;

    if (strlen(input) == 0) return;

    if (strncasecmp(input, "BUSCAR", 6) == 0 ||
        strncasecmp(input, "CONTAR", 6) == 0 ||
        strncasecmp(input, "TOP", 3) == 0 ||
        (strncasecmp(input, "MOSTRAR", 7) == 0 && 
         strncasecmp(input, "MOSTRAR BASES DE DATOS", 22) != 0 && 
         strncasecmp(input, "MOSTRAR TABLAS", 14) != 0)) {
        process_natural_language(input);
        return;
    }

    char tabla[50];

    if (strncasecmp(input, "SALIR", 5) == 0) {
        printf("Cerrando el motor de base de datos... Adios.\n");
        exit(EXIT_SUCCESS);
    }
    else if (strncasecmp(input, "CREAR BASE DE DATOS", 19) == 0) {
        char* ptr = input + 19;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            sscanf(ptr, "%49s", tabla);
            crear_database(tabla);
        } else {
            printf("Uso: CREAR BASE DE DATOS <nombre>\n");
        }
        return;
    }
    else if (strncasecmp(input, "ELIMINAR BASE DE DATOS", 22) == 0) {
        char* ptr = input + 22;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            sscanf(ptr, "%49s", tabla);
            eliminar_database(tabla);
        } else {
            printf("Uso: ELIMINAR BASE DE DATOS <nombre>\n");
        }
        return;
    }
    else if (strncasecmp(input, "RENOMBRAR BASE DE DATOS", 23) == 0) {
        char* ptr = input + 23;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            char nombre_viejo[50], nombre_nuevo[50];
            int matched = sscanf(ptr, "%49s %49s", nombre_viejo, nombre_nuevo);
            if (matched == 2) {
                renombrar_database(nombre_viejo, nombre_nuevo);
            } else {
                printf("Uso: RENOMBRAR BASE DE DATOS <nombre_viejo> <nombre_nuevo>\n");
            }
        } else {
            printf("Uso: RENOMBRAR BASE DE DATOS <nombre_viejo> <nombre_nuevo>\n");
        }
        return;
    }
    else if (strncasecmp(input, "CREAR TABLA", 11) == 0) {
        char* ptr = input + 11;
        ptr = saltar_espacios(ptr);

        char* paren_start = strchr(ptr, '(');
        if (paren_start) {
            size_t len_nombre = paren_start - ptr;
            if (len_nombre < 50) {
                strncpy(tabla, ptr, len_nombre);
                tabla[len_nombre] = '\0';

                char* first_space = strchr(tabla, ' ');
                if (first_space) *first_space = '\0';

                char esquema[500];
                strcpy(esquema, paren_start);

                Campo campos[MAX_CAMPOS];
                int num_campos = 0;

                if (parsear_esquema(esquema, campos, &num_campos) == 0) {
                    crear_tabla_desde_parser(tabla, campos, num_campos);
                } else {
                    printf("Error: No se pudo parsear el esquema.\n");
                }
            }
        } else {
            printf("Uso: CREAR TABLA <nombre> (campo1 TIPO, campo2 TIPO, ...)\n");
        }
        return;
    }
    else if (strncasecmp(input, "ELIMINAR TABLA", 14) == 0) {
        char* db = get_database_actual();
        if (!db) {
            printf("Error: Seleccione una base de datos con USAR <nombre>\n");
            return;
        }
        char* ptr = input + 14;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            sscanf(ptr, "%49s", tabla);
            eliminar_tabla(db, tabla);
        } else {
            printf("Uso: ELIMINAR TABLA <nombre>\n");
        }
        return;
    }
    else if (strncasecmp(input, "USAR", 4) == 0) {
        char* ptr = input + 4;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            sscanf(ptr, "%49s", tabla);
            usar_database(tabla);
        } else {
            printf("Uso: USAR <base_de_datos>\n");
        }
        return;
    }
    else if (strncasecmp(input, "MOSTRAR BASES DE DATOS", 22) == 0) {
        DefinicionDB* dbs;
        int count;
        listar_databases(&dbs, &count);
        printf("\n--- Bases de datos (%d) ---\n", count);
        for (int i = 0; i < count; i++) {
            printf("  %s\n", dbs[i].nombre);
        }
        return;
    }
    else if (strncasecmp(input, "MOSTRAR TABLAS", 14) == 0) {
        char* db = get_database_actual();
        if (!db) {
            printf("Error: Seleccione una base de datos con USAR <nombre>\n");
            return;
        }
        DefinicionTabla tablas[MAX_TABLAS];
        int count;
        listar_tablas(db, tablas, &count);
        printf("\n--- Tablas en '%s' (%d) ---\n", db, count);
        for (int i = 0; i < count; i++) {
            printf("  %s\n", tablas[i].nombre);
        }
        return;
    }
    else if (strncasecmp(input, "INICIAR TRANSACCION", 18) == 0 ||
             strncasecmp(input, "INICIAR", 8) == 0) {
        iniciar_transaccion();
        return;
    }
    else if (strncasecmp(input, "CONFIRMAR", 9) == 0) {
        confirmar_transaccion();
        return;
    }
    else if (strncasecmp(input, "DESHACER", 8) == 0) {
        deshacer_transaccion();
        return;
    }

    char* db_actual = get_database_actual();

    if (strncasecmp(input, "SELECCIONAR", 11) == 0) {
        char* ptr = input + 11;
        ptr = saltar_espacios(ptr);
        sscanf(ptr, "%49s", tabla);

        if (db_actual && table_exists(db_actual, tabla)) {
            // FASE 2.1: Hook automático - analizar query antes de ejecutar
            if (llm_is_initialized()) {
                char prompt[1024];
                snprintf(prompt, sizeof(prompt),
                    "Eres un optimizador de queries SQL. Este motor usa dialecto español: "
                    "'SELECCIONAR <tabla>' = 'SELECT * FROM <tabla>'. "
                    "'SELECCIONAR <tabla> WHERE <condicion>' = 'SELECT * FROM <tabla> WHERE <condicion>'. "
                    "Analiza la query y dame: 1) Si hace full table scan 2) Si le falta LIMIT 3) Índice sugerido si aplica 4) Tiempo estimado. "
                    "Query: %s\nResponde en español, máximo 3 oraciones.", input);

                const char * analysis = llm_think(prompt);
                printf("\n[CONSCIENCE] %s\n\n", analysis);
            }
            struct timeval start, end;
            gettimeofday(&start, NULL);
            
            int rows = listar_registros_dinamicos(db_actual, tabla);
            
            gettimeofday(&end, NULL);
            double exec_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
            
            if (rows >= 0) {
                log_query(db_actual, tabla, input, exec_ms, rows);
                process_auto_index(db_actual, input, exec_ms, tabla);
            }
        } else {
            printf("Error: Tabla '%s' no existe.\n", tabla);
        }
        return;
    }
    else if (strncasecmp(input, "INSERTAR", 8) == 0) {
        char* ptr = input + 8;
        ptr = saltar_espacios(ptr);
        sscanf(ptr, "%49s", tabla);

        char** valores = malloc(MAX_CAMPOS * sizeof(char*));
        int num_valores = 0;

        if (parsear_valores_insert(input + 8, valores, &num_valores) != 0) {
            printf("Uso: INSERTAR <tabla> (valor1, valor2, ...)\n");
            free(valores);
            return;
        }

        if (db_actual && table_exists(db_actual, tabla)) {
            // Validar ID unico (primera columna)
            if (num_valores > 0 && id_existe_en_tabla(db_actual, tabla, valores[0])) {
                printf("Error: El ID '%s' ya existe en '%s'.\n", valores[0], tabla);
            } else if (escribir_registro_dinamico(db_actual, tabla, valores, num_valores) == 0) {
                printf("=> Registro insertado en '%s'.\n", tabla);
                if (esta_en_transaccion()) {
                    char datos_nuevos[MAX_LINEA] = {0};
                    for (int i = 0; i < num_valores; i++) {
                        if (i > 0) strncat(datos_nuevos, "|", MAX_LINEA - strlen(datos_nuevos) - 1);
                        strncat(datos_nuevos, valores[i], MAX_LINEA - strlen(datos_nuevos) - 1);
                    }
                    registrar_en_log(tabla, -1, OP_INSERT, NULL, datos_nuevos);
                }
            } else {
                printf("Error: No se pudo insertar el registro.\n");
            }
        } else {
            printf("Error: Tabla '%s' no existe.\n", tabla);
        }

        for (int i = 0; i < num_valores; i++) free(valores[i]);
        free(valores);
        return;
    }
    else if (strncasecmp(input, "ELIMINAR", 8) == 0) {
        char* ptr = input + 8;
        ptr = saltar_espacios(ptr);
        int id;

        char tabla_copy[100];
        strncpy(tabla_copy, ptr, 99);
        tabla_copy[99] = '\0';

        if (sscanf(tabla_copy, "%49s %d", tabla, &id) == 2) {
            if (db_actual && table_exists(db_actual, tabla)) {
                if (esta_en_transaccion()) {
                    // Leer datos antes de eliminar para poder restaurarlos
                    // Simplemente eliminamos sin guardar para el undo
                    if (eliminar_registro_dinamico(db_actual, tabla, id) == 0) {
                        printf("=> ID %d eliminado de '%s'.\n", id, tabla);
                        registrar_en_log(tabla, id, OP_DELETE, "datos_backup", NULL);
                    } else {
                        printf("Error: ID %d no encontrado en '%s'.\n", id, tabla);
                    }
                } else {
                    if (eliminar_registro_dinamico(db_actual, tabla, id) == 0) {
                        printf("=> ID %d eliminado de '%s'.\n", id, tabla);
                    } else {
                        printf("Error: ID %d no encontrado en '%s'.\n", id, tabla);
                    }
                }
            } else {
                printf("Error: Tabla '%s' no existe.\n", tabla);
            }
        } else {
            printf("Uso: ELIMINAR <tabla> <id>\n");
        }
        return;
    }

    // Conscience Commands
    if (strncasecmp(input, "QUIEN SOS", 9) == 0) {
        if (llm_is_initialized()) {
            const char * response = llm_think("Eres MOTOR CONSCIENCE, un asistente de IA para un motor de base de datos SQL en español. Preséntate en 2-3 oraciones, menciona que puedes analizar queries, sugerir índices y detectar anomalías.");
            printf("\n[CONSCIENCE]\n%s\n\n", response);
        } else {
            printf("\n[CONSCIENCE] Soy el motor de inteligencia artificial de MOTOR.\n");
            printf("[CONSCIENCE] Actualmente estoy dormido (sin LLM cargado).\n");
            printf("[CONSCIENCE] Para activarme necesitas un modelo GGUF en models/\n\n");
        }
        return;
    }
    else if (strncasecmp(input, "AYUDA CONSCIENCE", 16) == 0) {
        printf("\n=== COMANDOS CONSCIENCE ===\n");
        printf("  QUIEN SOS             - Conocer a Conscience\n");
        printf("  EXPLICAR CONSULTA     - Analizar query y sugerir optimizaciones\n");
        printf("  SUGERIR INDICES       - Ver indices sugeridos por patron de uso\n");
        printf("  AUTO INDEXAR [ON/OFF] - Toggle auto-creacion de indices\n");
        printf("  VER ANALYTICS         - Ver dashboard de uso\n");
        printf("  AYUDA CONSCIENCE      - Este mensaje\n");
        printf("\n");
        return;
    }
    else if (strncasecmp(input, "EXPLICAR CONSULTA", 16) == 0) {
        char * ptr = input + 16;
        ptr = saltar_espacios(ptr);
        if (*ptr) {
            if (llm_is_initialized()) {
                char prompt[1024];
                snprintf(prompt, sizeof(prompt),
                    "Analiza esta query SQL y sugiere optimizaciones. Si es lenta, explica por qué y sugiere un índice.\nQuery: %s\nResponde en español, de forma concisa.", ptr);

                const char * response = llm_think(prompt);
                printf("\n[CONSCIENCE ANALISIS]\n%s\n\n", response);
            } else {
                printf("[CONSCIENCE] LLM no disponible. Ejecuta con modelo GGUF.\n");
            }
        } else {
            printf("Uso: EXPLICAR CONSULTA <tu query>\n");
        }
        return;
    }
    else if (strncasecmp(input, "SUGERIR INDICES", 15) == 0) {
        printf("\n[CONSCIENCE] Analizando patrones de uso...\n");
        analyze_and_suggest_indices(db_actual);
        return;
    }
    else if (strncasecmp(input, "AUTO INDEXAR", 12) == 0) {
        char * ptr = input + 12;
        ptr = saltar_espacios(ptr);
        if (strncasecmp(ptr, "ON", 2) == 0 || strncasecmp(ptr, "S", 1) == 0) {
            toggle_auto_index(db_actual, 1);
        } else if (strncasecmp(ptr, "OFF", 3) == 0 || strncasecmp(ptr, "N", 1) == 0) {
            toggle_auto_index(db_actual, 0);
        } else {
            printf("Uso: AUTO INDEXAR [ON/OFF]\n");
        }
        return;
    }

    printf("Error: Comando '%s' no reconocido.\n", input);
}