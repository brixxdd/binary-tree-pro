CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_GNU_SOURCE -I. -I./includes
TARGET = motor
SOURCES = motor.c \
          src/errors.c \
          src/transaction.c \
          src/index.c \
          src/database.c \
          src/io.c \
          src/parser.c \
          src/llm.c \
          src/query_analytics.c
HEADERS = includes/config.h \
          includes/types.h \
          includes/index.h \
          includes/transaction.h \
          includes/parser.h \
          includes/database.h \
          includes/errors.h \
          includes/io.h \
          includes/llm.h \
          includes/query_analytics.h

.PHONY: all clean run test help

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lm -lpthread

clean:
	rm -f $(TARGET) *.o
	rm -rf data/

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	@echo "=== Test 1: Base de datos ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nMOSTRAR BASES DE DATOS\nSALIR\n" | ./$(TARGET)
	@echo ""
	@echo "=== Test 2: Crear tablas dinamicas ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nCREAR TABLA empleados (id INT, nombre STRING(50), salario FLOAT)\nCREAR TABLA productos (id INT, nombre STRING(50), precio FLOAT, stock INT)\nMOSTRAR TABLAS\nSALIR\n" | ./$(TARGET)
	@echo ""
	@echo "=== Test 3: INSERT y SELECT ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nCREAR TABLA empleados (id INT, nombre STRING(50), salario FLOAT)\nINSERTAR empleados (1, Juan, 25000)\nINSERTAR empleados (2, Maria, 30000)\nINSERTAR empleados (3, Pedro, 28000)\nSELECCIONAR empleados\nSALIR\n" | ./$(TARGET)
	@echo ""
	@echo "=== Test 4: ELIMINAR registro ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nCREAR TABLA empleados (id INT, nombre STRING(50), salario FLOAT)\nINSERTAR empleados (1, Juan, 25000)\nINSERTAR empleados (2, Maria, 30000)\nELIMINAR empleados 1\nSELECCIONAR empleados\nSALIR\n" | ./$(TARGET)
	@echo ""
	@echo "=== Test 5: Transacciones ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nCREAR TABLA empleados (id INT, nombre STRING(50), salario FLOAT)\nINICIAR TRANSACCION\nINSERTAR empleados (1, Juan, 25000)\nCONFIRMAR\nSELECCIONAR empleados\nSALIR\n" | ./$(TARGET)
	@echo ""
	@echo "=== Test 6: ELIMINAR TABLA y BD ==="
	@rm -rf data/
	@printf "CREAR BASE DE DATOS testdb\nUSAR testdb\nCREAR TABLA empleados (id INT, nombre STRING(50), salario FLOAT)\nELIMINAR TABLA empleados\nMOSTRAR TABLAS\nELIMINAR BASE DE DATOS testdb\nMOSTRAR BASES DE DATOS\nSALIR\n" | ./$(TARGET)

help:
	@echo "Motor de BD v0.4 - CONSCIENCE EDITION"
	@echo ""
	@echo "  Bases de datos:"
	@echo "    CREAR BASE DE DATOS <nombre>  - Crear base de datos"
	@echo "    USAR <base_de_datos>           - Seleccionar base de datos"
	@echo "    MOSTRAR BASES DE DATOS         - Listar bases de datos"
	@echo "    ELIMINAR BASE DE DATOS <nombre> - Eliminar base de datos"
	@echo "    RENOMBRAR BASE DE DATOS <viejo> <nuevo> - Renombrar base de datos"
	@echo ""
	@echo "  Tablas:"
	@echo "    CREAR TABLA <nombre> (campo1 TIPO, ...) - Crear tabla"
	@echo "    ELIMINAR TABLA <nombre>      - Eliminar tabla"
	@echo "    MOSTRAR TABLAS                - Listar tablas"
	@echo ""
	@echo "  Registros (tablas dinamicas):"
	@echo "    INSERTAR <tabla> (val1, val2, ...) - Insertar"
	@echo "    SELECCIONAR <tabla>          - Mostrar registros"
	@echo "    ELIMINAR <tabla> <id>        - Eliminar registro"
	@echo ""
	@echo "  Transacciones:"
	@echo "    INICIAR TRANSACCION           - Iniciar transaccion"
	@echo "    CONFIRMAR                     - Confirmar cambios"
	@echo "    DESHACER                      - Deshacer cambios"
	@echo ""
	@echo "  Conscience (AI):"
	@echo "    EXPLICAR CONSULTA <query>     - Analizar query y sugerir optimizaciones"
	@echo "    SUGERIR INDICES               - Ver indices sugeridos por patron de uso"
	@echo "    AUTO INDEXAR [ON/OFF]         - Toggle auto-creacion de indices"
	@echo "    VER ANALYTICS                 - Ver dashboard de uso"
	@echo "    QUIEN SOS                     - Conocer a Conscience"
	@echo ""
	@echo "  Utilidades:"
	@echo "    REINDEXAR <tabla>             - Reconstruir indice"
	@echo "    SALIR                         - Salir"
	@echo ""
	@echo "  Tipos: INT, FLOAT, STRING(n)"