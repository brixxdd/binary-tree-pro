#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "includes/config.h"
#include "includes/types.h"
#include "includes/errors.h"
#include "includes/parser.h"
#include "includes/transaction.h"
#include "includes/index.h"
#include "includes/database.h"
#include "includes/llm.h"

void print_prompt(void) {
    char* db = get_database_actual();
    if (db && db[0]) {
        printf("mibd:%s> ", db);
    } else {
        printf("mibd> ");
    }
}

void read_input(char* buffer) {
    if (fgets(buffer, MAX_BUFFER, stdin) == NULL) {
        printf("Error fatal al leer la entrada.\n");
        exit(EXIT_FAILURE);
    }
    buffer[strcspn(buffer, "\n")] = 0;
}

int main(int argc, char * argv[]) {
    char input_buffer[MAX_BUFFER];

    printf("=========================================\n");
    printf("   Motor de Base de Datos v0.4 Iniciado  \n");
    printf("   CONSCIENCE EDITION                   \n");
    printf("=========================================\n\n");

    init_databases();

    // Si pasa argumento que parece API key (contiene "sk-" o es muy largo), usar API mode
    // Si pasa un path a archivo .gguf, usar modo local (deprecated)
    // Por defecto: usar MiniMax API con la key configurada
    if (argc > 1 && strncmp(argv[1], "sk-", 3) == 0) {
        // Es una API key
        if (llm_init(argv[1]) == 0) {
            printf("\n[CONSCIENCE] MiniMax API activa.\n\n");
        } else {
            printf("\n[CONSCIENCE] Error inicializando API.\n\n");
        }
    } else if (argc > 1 && strstr(argv[1], ".gguf") != NULL) {
        // Es un archivo GGUF - no soportado en API mode, usar fallback
        printf("\n[CONSCIENCE] Modo local no disponible en esta version.\n");
        printf("[CONSCIENCE] Usando MiniMax API por defecto.\n\n");
        llm_init(NULL); // Usar key default de includes/llm.h
    } else {
        // Default: usar MiniMax API con key de configuracion
        llm_init(NULL);
    }

    while (1) {
        print_prompt();
        read_input(input_buffer);

        if (strlen(input_buffer) == 0) continue;

        parse_and_execute(input_buffer);
    }

    llm_cleanup();
    return 0;
}