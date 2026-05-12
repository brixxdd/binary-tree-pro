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
static EntradaLog log_transaccion[MAX_LOG_TRANSACCION];
static int num_entradas = 0;

void iniciar_transaccion(void) {
    en_transaccion = 1;
    num_entradas = 0;
    printf("=> Transaccion iniciada (START TRANSACTION)\n");
}

void confirmar_transaccion(void) {
    if (!en_transaccion) {
        printf("Error: No hay transaccion activa.\n");
        return;
    }
    en_transaccion = 0;
    num_entradas = 0;
    printf("=> Transaccion confirmada (COMMIT).\n");
}

void registrar_en_log(const char* tabla, int id, OperacionLog operacion, const char* datos_previos, const char* datos_nuevos) {
    if (!en_transaccion) return;
    if (num_entradas >= MAX_LOG_TRANSACCION) return;

    EntradaLog* entrada = &log_transaccion[num_entradas];
    entrada->operacion = operacion;
    strncpy(entrada->tabla, tabla, 49);
    entrada->tabla[49] = '\0';
    entrada->id = id;
    if (datos_previos) {
        strncpy(entrada->datos_previos, datos_previos, MAX_LINEA - 1);
        entrada->datos_previos[MAX_LINEA - 1] = '\0';
    } else {
        entrada->datos_previos[0] = '\0';
    }
    if (datos_nuevos) {
        strncpy(entrada->datos_nuevos, datos_nuevos, MAX_LINEA - 1);
        entrada->datos_nuevos[MAX_LINEA - 1] = '\0';
    } else {
        entrada->datos_nuevos[0] = '\0';
    }
    num_entradas++;
}

void deshacer_transaccion(void) {
    if (!en_transaccion) {
        printf("Error: No hay transaccion activa.\n");
        return;
    }

    printf("=> Transaccion cancelada (ROLLBACK).\n");

    for (int i = num_entradas - 1; i >= 0; i--) {
        EntradaLog* entrada = &log_transaccion[i];

        char* db_actual = get_database_actual();
        if (!db_actual) {
            printf("  (undo skip: no hay DB activa)\n");
            continue;
        }
        if (entrada->operacion == OP_INSERT) {
            // Undo INSERT = eliminar la línea que coincide con datos_nuevos
            if (entrada->datos_nuevos[0]) {
                char ruta[256];
                construir_ruta_tabla(db_actual, entrada->tabla, ruta, sizeof(ruta));

                FILE* f = fopen(ruta, "r");
                FILE* tmp = fopen("data/.undo_tmp", "w");
                if (f && tmp) {
                    char linea[MAX_LINEA];
                    while (fgets(linea, sizeof(linea), f)) {
                        size_t len = strlen(linea);
                        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r')) {
                            linea[len-1] = '\0';
                            len--;
                        }
                        if (strcmp(linea, entrada->datos_nuevos) != 0) {
                            fprintf(tmp, "%s\n", linea);
                        }
                    }
                    fclose(f);
                    fclose(tmp);
                    rename("data/.undo_tmp", ruta);
                    printf("  (undo INSERT en '%s')\n", entrada->tabla);
                } else {
                    if (f) fclose(f);
                    if (tmp) fclose(tmp);
                }
            }
        }
        else if (entrada->operacion == OP_DELETE) {
            // Undo DELETE = re-insertar los datos previos
            if (entrada->datos_previos[0]) {
                char* valores[MAX_CAMPOS];
                char copia[MAX_LINEA];
                strncpy(copia, entrada->datos_previos, MAX_LINEA - 1);
                copia[MAX_LINEA - 1] = '\0';

                int num_vals = 0;
                char* token = strtok(copia, "|");
                while (token && num_vals < MAX_CAMPOS) {
                    valores[num_vals] = strdup(token);
                    num_vals++;
                    token = strtok(NULL, "|");
                }

                escribir_registro_dinamico(db_actual, entrada->tabla, valores, num_vals);
                printf("  (undo DELETE id=%d en '%s', restaurado: %s)\n", entrada->id, entrada->tabla, entrada->datos_previos);

                for (int j = 0; j < num_vals; j++) free(valores[j]);
            }
        }
    }

    en_transaccion = 0;
    num_entradas = 0;
}

int esta_en_transaccion(void) {
    return en_transaccion;
}
