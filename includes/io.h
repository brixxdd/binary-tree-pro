#ifndef IO_H
#define IO_H

#include "types.h"

int guardar_metadatos_tabla(const char* db_nombre, DefinicionTabla* tabla);
int cargar_metadatos_tabla(const char* db_nombre, const char* tabla_nombre, DefinicionTabla* resultado);
TableLayout* calcular_layout_tabla(DefinicionTabla* tabla);
int escribir_registro_dinamico(const char* db, const char* tabla, char** valores, int num_valores);
char** leer_registro_dinamico(const char* db, const char* tabla, int id_registro);
int listar_registros_dinamicos(const char* db, const char* tabla);
int eliminar_registro_dinamico(const char* db, const char* tabla, int id);
int actualizar_registro_dinamico(const char* db, const char* tabla, int id, char** valores, int num_valores);
void liberar_valores(char** valores, int num_valores);
void obtener_ultimo_id(const char* db, const char* tabla, char* resultado, int max_len);
void construir_ruta_tabla(const char* db_nombre, const char* tabla_nombre, char* ruta, size_t tam);

#endif