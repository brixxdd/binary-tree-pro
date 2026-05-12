#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "types.h"

#define MAX_LOG_TRANSACCION 100

typedef enum {
    OP_INSERT,
    OP_DELETE
} OperacionLog;

typedef struct {
    OperacionLog operacion;
    char tabla[50];
    int id;
    char datos_previos[MAX_LINEA];
    char datos_nuevos[MAX_LINEA];
} EntradaLog;

void iniciar_transaccion(void);
void confirmar_transaccion(void);
void deshacer_transaccion(void);
void registrar_en_log(const char* tabla, int id, OperacionLog operacion, const char* datos_previos, const char* datos_nuevos);
int esta_en_transaccion(void);
void obtener_ultimo_id(const char* db, const char* tabla, char* resultado, int max_len);

#endif