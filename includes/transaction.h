#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "types.h"

#define MAX_LOG_TRANSACCION 100

typedef enum {
    OP_INSERT,
    OP_DELETE
} OperacionLog;

void iniciar_transaccion(void);
void confirmar_transaccion(void);
void deshacer_transaccion(void);
int esta_en_transaccion(void);
void registrar_en_log(const char* tabla, int id, OperacionLog operacion, const char* datos_previos, const char* datos_nuevos);
void restaurar_estado_transaccion(const char* db);

#endif