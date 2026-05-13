#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include "../includes/types.h"
#include "../includes/io.h"
#include "../includes/errors.h"

extern CodigoError ultimo_error;

TableLayout* calcular_layout_tabla(DefinicionTabla* tabla) {
    static TableLayout layout;
    strcpy(layout.nombre_tabla, tabla->nombre);
    layout.num_campos = tabla->num_campos;
    layout.tamano_total = 0;

    int offset_actual = 0;
    for (int i = 0; i < tabla->num_campos; i++) {
        strcpy(layout.campos[i].nombre, tabla->campos[i].nombre);
        layout.campos[i].tipo = tabla->campos[i].tipo;
        layout.campos[i].longitud = tabla->campos[i].longitud;
        layout.campos[i].offset_valor = offset_actual;

        switch (tabla->campos[i].tipo) {
            case TIPO_INT:
                offset_actual += sizeof(int);
                break;
            case TIPO_FLOAT:
                offset_actual += sizeof(float);
                break;
            case TIPO_STRING:
                offset_actual += tabla->campos[i].longitud;
                break;
        }
    }
    layout.tamano_total = offset_actual;
    return &layout;
}

void construir_ruta_tabla(const char* db_nombre, const char* tabla_nombre, char* ruta, size_t tam) {
    snprintf(ruta, tam, "data/%s/%s.csv", db_nombre, tabla_nombre);
}

static void construir_ruta_meta(const char* db_nombre, const char* tabla_nombre, char* ruta, size_t tam) {
    snprintf(ruta, tam, "data/%s/%s.meta", db_nombre, tabla_nombre);
}

int guardar_metadatos_tabla(const char* db_nombre, DefinicionTabla* tabla) {
    char ruta[256];
    construir_ruta_meta(db_nombre, tabla->nombre, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "w");
    if (!f) {
        ultimo_error = ERR_ARCHIVO_NO_ENCONTRADO;
        return -1;
    }

    fprintf(f, "# Schema for table: %s\n", tabla->nombre);
    fprintf(f, "nombre:%s\n", tabla->nombre);
    fprintf(f, "modo:%d\n", tabla->modo);
    fprintf(f, "num_campos:%d\n", tabla->num_campos);

    for (int i = 0; i < tabla->num_campos; i++) {
        fprintf(f, "campo:%s|%d|%d\n",
                tabla->campos[i].nombre,
                tabla->campos[i].tipo,
                tabla->campos[i].longitud);
    }

    fclose(f);
    return 0;
}

int cargar_metadatos_tabla(const char* db_nombre, const char* tabla_nombre, DefinicionTabla* resultado) {
    char ruta[256];
    construir_ruta_meta(db_nombre, tabla_nombre, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "r");
    if (!f) {
        return -1;
    }

    char linea[MAX_LINEA];
    resultado->num_campos = 0;
    int campo_idx = 0;

    while (fgets(linea, sizeof(linea), f)) {
        linea[strcspn(linea, "\n")] = 0;

        if (linea[0] == '#' || strlen(linea) == 0) continue;

        char clave[50], valor[MAX_LINEA];
        if (sscanf(linea, "%49[^:]:%[^\n]", clave, valor) == 2) {
            if (strcmp(clave, "nombre") == 0) {
                strcpy(resultado->nombre, valor);
            } else if (strcmp(clave, "modo") == 0) {
                resultado->modo = atoi(valor);
            } else if (strcmp(clave, "num_campos") == 0) {
                resultado->num_campos = atoi(valor);
            } else if (strcmp(clave, "campo") == 0) {
                char* token = strtok(valor, "|");
                if (token) strcpy(resultado->campos[campo_idx].nombre, token);
                token = strtok(NULL, "|");
                if (token) resultado->campos[campo_idx].tipo = atoi(token);
                token = strtok(NULL, "|");
                if (token) resultado->campos[campo_idx].longitud = atoi(token);
                campo_idx++;
            }
        }
    }

    fclose(f);
    return 0;
}

int escribir_registro_dinamico(const char* db, const char* tabla, char** valores, int num_valores) {
    DefinicionTabla def;
    if (cargar_metadatos_tabla(db, tabla, &def) != 0) {
        ultimo_error = ERR_TABLA_NO_EXISTE;
        return -1;
    }

    if (num_valores != def.num_campos) {
        ultimo_error = ERR_SINTAXIS;
        return -1;
    }

    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "a");
    if (!f) {
        ultimo_error = ERR_ARCHIVO_NO_ENCONTRADO;
        return -1;
    }

    for (int i = 0; i < num_valores; i++) {
        fprintf(f, "%s", valores[i]);
        if (i < num_valores - 1) fprintf(f, "|");
    }
    fprintf(f, "\n");

    fclose(f);
    return 0;
}

int id_existe_en_tabla(const char* db, const char* tabla, const char* id) {
    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "r");
    if (!f) return 0; // Archivo no existe = no existe ID

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), f)) {
        size_t len = strlen(linea);
        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r')) {
            linea[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        // Primera columna hasta el primer |
        char* pipe = strchr(linea, '|');
        if (pipe) {
            size_t id_len = pipe - linea;
            if (strlen(id) == id_len && strncmp(linea, id, id_len) == 0) {
                fclose(f);
                return 1; // ID existe
            }
        }
    }
    fclose(f);
    return 0; // ID no existe
}

void obtener_ultimo_id(const char* db, const char* tabla, char* resultado, int max_len) {
    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "r");
    if (!f) {
        if (resultado && max_len > 0) resultado[0] = '\0';
        return;
    }

    char linea[MAX_LINEA];
    char* ultima_linea = NULL;

    while (fgets(linea, sizeof(linea), f)) {
        if (linea[0] != '\n' && linea[0] != '\0') {
            ultima_linea = strdup(linea);
        }
    }
    fclose(f);

    if (ultima_linea) {
        size_t len = strlen(ultima_linea);
        while (len > 0 && (ultima_linea[len-1] == '\n' || ultima_linea[len-1] == '\r')) {
            ultima_linea[len-1] = '\0';
            len--;
        }
        if (resultado && max_len > 0) {
            strncpy(resultado, ultima_linea, max_len - 1);
            resultado[max_len - 1] = '\0';
        }
        free(ultima_linea);
    } else {
        if (resultado && max_len > 0) resultado[0] = '\0';
    }
}

int listar_registros_dinamicos(const char* db, const char* tabla) {
    DefinicionTabla def;
    if (cargar_metadatos_tabla(db, tabla, &def) != 0) {
        printf("Error: Tabla '%s' no existe.\n", tabla);
        return -1;
    }

    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    FILE* f = fopen(ruta, "r");
    if (!f) {
        printf("Info: Tabla '%s' esta vacia.\n", tabla);
        return 0;
    }

    printf("\n--- CONTENIDO DE LA TABLA: %s ---\n", tabla);

    char linea[MAX_LINEA];
    int rows_scanned = 0;
    while (fgets(linea, sizeof(linea), f)) {
        size_t len = strlen(linea);
        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r' || linea[len-1] == ' ')) {
            linea[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;
        
        rows_scanned++;

        int pos = 0;
        int campo_idx = 0;
        int primero = 1;

        while (pos < (int)len && campo_idx < def.num_campos) {
            int start = pos;
            int pipe_pos = -1;

            for (int i = pos; i < (int)len; i++) {
                if (linea[i] == '|') {
                    pipe_pos = i;
                    break;
                }
            }

            if (pipe_pos >= 0) {
                linea[pipe_pos] = '\0';
            }

            while (start < (int)len && linea[start] == ' ') start++;

            if (!primero) printf(" | ");
            printf("%s: %s", def.campos[campo_idx].nombre, linea + start);
            primero = 0;
            campo_idx++;

            if (pipe_pos >= 0) {
                pos = pipe_pos + 1;
            } else {
                pos = len;
            }
        }
        printf("\n");
    }

    fclose(f);
    return rows_scanned;
}

int eliminar_registro_dinamico(const char* db, const char* tabla, int id) {
    DefinicionTabla def;
    if (cargar_metadatos_tabla(db, tabla, &def) != 0) {
        ultimo_error = ERR_TABLA_NO_EXISTE;
        return -1;
    }

    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    char ruta_tmp[300];
    snprintf(ruta_tmp, sizeof(ruta_tmp), "%s.tmp", ruta);

    FILE* f_orig = fopen(ruta, "r");
    if (!f_orig) {
        ultimo_error = ERR_ARCHIVO_NO_ENCONTRADO;
        return -1;
    }

    FILE* f_tmp = fopen(ruta_tmp, "w");
    if (!f_tmp) {
        fclose(f_orig);
        return -1;
    }

    char linea[MAX_LINEA];
    int linea_eliminada = 0;

    while (fgets(linea, sizeof(linea), f_orig)) {
        size_t len = strlen(linea);
        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r')) {
            linea[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        int first_pipe = -1;
        for (int i = 0; i < (int)len; i++) {
            if (linea[i] == '|') {
                first_pipe = i;
                break;
            }
        }

        if (first_pipe > 0) {
            linea[first_pipe] = '\0';
            int id_linea = atoi(linea);
            linea[first_pipe] = '|';

            if (id_linea == id) {
                linea_eliminada = 1;
                continue;
            }
        }

        fprintf(f_tmp, "%s\n", linea);
    }

    fclose(f_orig);
    fclose(f_tmp);

    if (linea_eliminada) {
        rename(ruta_tmp, ruta);
        return 0;
    } else {
        remove(ruta_tmp);
        ultimo_error = ERR_ID_NO_ENCONTRADO;
        return -1;
    }
}

int actualizar_registro_dinamico(const char* db, const char* tabla, int id, const char* campo, const char* nuevo_valor) {
    DefinicionTabla def;
    if (cargar_metadatos_tabla(db, tabla, &def) != 0) {
        ultimo_error = ERR_TABLA_NO_EXISTE;
        return -1;
    }

    int idx_campo = -1;
    for (int i = 0; i < def.num_campos; i++) {
        if (strcasecmp(def.campos[i].nombre, campo) == 0) {
            idx_campo = i;
            break;
        }
    }
    if (idx_campo < 0) {
        ultimo_error = ERR_CAMPO_NO_ENCONTRADO;
        return -1;
    }

    char ruta[256];
    construir_ruta_tabla(db, tabla, ruta, sizeof(ruta));

    char ruta_tmp[300];
    snprintf(ruta_tmp, sizeof(ruta_tmp), "%s.tmp", ruta);

    FILE* f_orig = fopen(ruta, "r");
    if (!f_orig) {
        ultimo_error = ERR_ARCHIVO_NO_ENCONTRADO;
        return -1;
    }

    FILE* f_tmp = fopen(ruta_tmp, "w");
    if (!f_tmp) {
        fclose(f_orig);
        return -1;
    }

    char linea[MAX_LINEA];
    int linea_actualizada = 0;

    while (fgets(linea, sizeof(linea), f_orig)) {
        size_t len = strlen(linea);
        while (len > 0 && (linea[len-1] == '\n' || linea[len-1] == '\r')) {
            linea[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        int first_pipe = -1;
        for (int i = 0; i < (int)len; i++) {
            if (linea[i] == '|') {
                first_pipe = i;
                break;
            }
        }

        if (first_pipe > 0) {
            linea[first_pipe] = '\0';
            int id_linea = atoi(linea);
            linea[first_pipe] = '|';

            if (id_linea == id) {
                char* campos[MAX_CAMPOS];
                int num_campos = 0;
                char temp[1024];
                strncpy(temp, linea, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';

                char* saveptr;
                char* token = strtok_r(temp, "|", &saveptr);
                while (token && num_campos < MAX_CAMPOS) {
                    campos[num_campos++] = token;
                    token = strtok_r(NULL, "|", &saveptr);
                }

                if (idx_campo < num_campos) {
                    campos[idx_campo] = (char*)nuevo_valor;
                }

                for (int i = 0; i < num_campos; i++) {
                    fprintf(f_tmp, "%s", campos[i]);
                    if (i < num_campos - 1) fprintf(f_tmp, "|");
                }
                fprintf(f_tmp, "\n");
                linea_actualizada = 1;
                continue;
            }
        }

        fprintf(f_tmp, "%s\n", linea);
    }

    fclose(f_orig);
    fclose(f_tmp);

    if (linea_actualizada) {
        rename(ruta_tmp, ruta);
        return 0;
    } else {
        remove(ruta_tmp);
        ultimo_error = ERR_ID_NO_ENCONTRADO;
        return -1;
    }
}