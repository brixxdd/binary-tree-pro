#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "../includes/config.h"
#include "../includes/types.h"
#include "../includes/errors.h"
#include "../includes/io.h"
#include "../includes/transaction.h"
#include "../includes/database.h"

static int en_transaccion = 0;
static char tx_db_nombre[MAX_NOMBRE] = {0};

// Auto-restore transaction state from disk (needed because UI runs C engine per query)
void restaurar_estado_transaccion(const char* db) {
    if (!db) return;
    char path[256];
    snprintf(path, sizeof(path), "data/%s/_tx_log.csv", db);
    if (access(path, F_OK) == 0) {
        en_transaccion = 1;
        strncpy(tx_db_nombre, db, MAX_NOMBRE - 1);
        tx_db_nombre[MAX_NOMBRE - 1] = '\0';
    } else {
        en_transaccion = 0;
        tx_db_nombre[0] = '\0';
    }
}

const char* tx_log_path(void) {
    static char path[MAX_NOMBRE + 20];
    if (tx_db_nombre[0]) {
        snprintf(path, sizeof(path), "data/%s/_tx_log.csv", tx_db_nombre);
    } else {
        path[0] = '\0';
    }
    return path;
}

void iniciar_transaccion(void) {
    char* db = get_database_actual();
    if (!db) {
        printf("Error: No hay base de datos seleccionada.\n");
        return;
    }
    en_transaccion = 1;
    strncpy(tx_db_nombre, db, MAX_NOMBRE - 1);
    tx_db_nombre[MAX_NOMBRE - 1] = '\0';

    const char* path = tx_log_path();
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "# TX_LOG v1\n");
        fclose(f);
    }
    printf("=> Transaccion iniciada (START TRANSACTION)\n");
}

void confirmar_transaccion(void) {
    if (!en_transaccion) {
        printf("Error: No hay transaccion activa.\n");
        return;
    }

    const char* path = tx_log_path();
    
    // As inserts are already written to the main table during the transaction
    // (and the log is only used for UNDO operations), committing simply 
    // means deleting the rollback log so changes become permanent.
    unlink(path);
    en_transaccion = 0;
    tx_db_nombre[0] = '\0';
    printf("=> Transaccion confirmada (COMMIT).\n");
}

void registrar_en_log(const char* tabla, int id, OperacionLog operacion, const char* datos_previos, const char* datos_nuevos) {
    if (!en_transaccion || !tx_db_nombre[0]) return;

    const char* path = tx_log_path();
    FILE* f = fopen(path, "a");
    if (!f) return;

    if (operacion == OP_INSERT && datos_nuevos) {
        fprintf(f, "INSERT|%s|%s\n", tabla, datos_nuevos);
    } else if (operacion == OP_DELETE && datos_previos) {
        fprintf(f, "DELETE|%s|%d|%s\n", tabla, id, datos_previos);
    } else if (operacion == OP_UPDATE && datos_previos && datos_nuevos) {
        fprintf(f, "UPDATE|%s|%d|%s|%s\n", tabla, id, datos_previos, datos_nuevos);
    }
    fclose(f);
}

void deshacer_transaccion(void) {
    if (!en_transaccion) {
        printf("Error: No hay transaccion activa.\n");
        return;
    }

    const char* path = tx_log_path();
    FILE* log = fopen(path, "r");
    if (!log) {
        printf("Error: No se pudo abrir log de transaccion.\n");
        en_transaccion = 0;
        tx_db_nombre[0] = '\0';
        return;
    }

    printf("=> Transaccion cancelada (ROLLBACK).\n");

    char lineas[MAX_LOG_TRANSACCION][MAX_LINEA];
    int num_lineas = 0;

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), log) && num_lineas < MAX_LOG_TRANSACCION) {
        size_t len = strlen(linea);
        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r')) {
            linea[len-1] = '\0';
            len--;
        }
        if (linea[0] == '#' || len == 0) continue;
        strncpy(lineas[num_lineas], linea, MAX_LINEA - 1);
        lineas[num_lineas][MAX_LINEA - 1] = '\0';
        num_lineas++;
    }
    fclose(log);

    // Procesar al revés (undo)
    for (int i = num_lineas - 1; i >= 0; i--) {
        char copia[MAX_LINEA];
        strncpy(copia, lineas[i], MAX_LINEA - 1);
        copia[MAX_LINEA - 1] = '\0';

        if (strncmp(copia, "INSERT|", 7) == 0) {
            char* ptr = copia + 7;
            char tabla[50], datos[MAX_LINEA];
            if (sscanf(ptr, "%49[^|]|%[^\n]", tabla, datos) == 2) {
                char ruta[256];
                construir_ruta_tabla(tx_db_nombre, tabla, ruta, sizeof(ruta));

                FILE* f = fopen(ruta, "r");
                FILE* tmp = fopen("data/.undo_tmp", "w");
                if (f && tmp) {
                    char l[MAX_LINEA];
                    while (fgets(l, sizeof(l), f)) {
                        size_t l_len = strlen(l);
                        while (l_len > 0 && (l[l_len-1] == '\n' || l[l_len-1] == '\r')) {
                            l[l_len-1] = '\0';
                            l_len--;
                        }
                        if (strcmp(l, datos) != 0) {
                            fprintf(tmp, "%s\n", l);
                        }
                    }
                    fclose(f);
                    fclose(tmp);
                    rename("data/.undo_tmp", ruta);
                    printf("  (undo INSERT en '%s')\n", tabla);
                } else {
                    if (f) fclose(f);
                    if (tmp) fclose(tmp);
                }
            }
        } else if (strncmp(copia, "UPDATE|", 7) == 0) {
            char* ptr = copia + 7;
            char tabla[50], id_str[20], datos_previos[MAX_LINEA], datos_nuevos[MAX_LINEA];
            if (sscanf(ptr, "%49[^|]|%19[^|]|%[^*]|%[^\n]", tabla, id_str, datos_previos, datos_nuevos) == 4) {
                int id_undo = atoi(id_str);
                char ruta[256];
                construir_ruta_tabla(tx_db_nombre, tabla, ruta, sizeof(ruta));

                FILE* f = fopen(ruta, "r");
                FILE* tmp = fopen("data/.undo_tmp", "w");
                if (f && tmp) {
                    char l[MAX_LINEA];
                    while (fgets(l, sizeof(l), f)) {
                        size_t l_len = strlen(l);
                        while (l_len > 0 && (l[l_len-1] == '\n' || l[l_len-1] == '\r')) {
                            l[l_len-1] = '\0';
                            l_len--;
                        }
                        if (strncmp(l, id_str, strlen(id_str)) == 0 && l[strlen(id_str)] == '|') {
                            fprintf(tmp, "%s\n", datos_previos);
                        } else {
                            fprintf(tmp, "%s\n", l);
                        }
                    }
                    fclose(f);
                    fclose(tmp);
                    rename("data/.undo_tmp", ruta);
                    printf("  (undo UPDATE en '%s' id %d)\n", tabla, id_undo);
                } else {
                    if (f) fclose(f);
                    if (tmp) fclose(tmp);
                }
            }
        }
    }

    unlink(path);
    en_transaccion = 0;
    tx_db_nombre[0] = '\0';
}

int esta_en_transaccion(void) {
    return en_transaccion;
}
