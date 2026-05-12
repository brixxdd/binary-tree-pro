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

void parse_and_execute(char* input) {
    while (isspace(*input)) input++;

    if (strlen(input) == 0) return;

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
            listar_registros_dinamicos(db_actual, tabla);
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
            if (escribir_registro_dinamico(db_actual, tabla, valores, num_valores) == 0) {
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
                    char datos_previos[MAX_LINEA];
                    datos_previos[0] = '\0';
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

    printf("Error: Comando '%s' no reconocido.\n", input);
}