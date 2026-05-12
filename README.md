# Motor de Base de Datos v0.3

Base de datos en C con arquitectura modular, soporte multi-tabla y comandos en español.

## Arquitectura Modular

```
includes/
├── config.h      → Constantes globales (MAX_BUFFER, MAX_LOG, MAX_CAMPOS)
├── types.h        → Tipos, enums, estructuras (Empleado, Cliente, Producto, DefinicionTabla, etc)
├── index.h       → Funciones de índice binario
├── transaction.h  → Manejo de transacciones ACID
├── parser.h      → Prototipo del parser
├── database.h    → Funciones de base de datos y tablas
├── io.h           → I/O CSV para tablas dinámicas
└── errors.h       → Códigos de error

src/
├── errors.c      → Mensajes de error
├── transaction.c  → INICIAR/CONFIRMAR/DESHACER
├── index.c        → Índice {id, offset} para búsqueda rápida
├── database.c     → CRUD de DBs y tablas
├── io.c           → Lectura/escritura CSV para tablas dinámicas
└── parser.c       → Parser de comandos y dispatch

motor.c            → main y loop principal
Makefile           → Build system
```

## Comandos de Ejecución

```bash
make          # compilar
make run      # ejecutar
make test     # tests automatizados
make clean    # limpiar archivos .dat, .idx, data/
make help     # mostrar ayuda
```

## Guía Rápida - Comandos en Español

### Bases de Datos
```sql
CREAR BASE DE DATOS miapp
USAR miapp
MOSTRAR BASES DE DATOS
RENOMBRAR BASE DE DATOS miapp miapp_new
ELIMINAR BASE DE DATOS miapp
```

### Tablas Dinámicas
```sql
CREAR TABLA clientes (id INT, nombre STRING(50), telefono STRING(14))
MOSTRAR TABLAS
ELIMINAR TABLA clientes
```

### Registros - Tablas Dinámicas
```sql
INSERTAR clientes (1, Juan, 5551234)
INSERTAR clientes (2, Maria, 5555678)
SELECCIONAR clientes
ELIMINAR clientes 1
SELECCIONAR clientes
```

### Transacciones
```sql
INICIAR TRANSACCION
INSERTAR clientes (3, Pedro, 5559999)
DESHACER
SELECCIONAR clientes
```

### Utilidades
```sql
REINDEXAR empleados
SALIR
```

## Ejemplo Completo - Copiar y Pegar

Ejecuta todo esto para probar el motor completo:

```bash
# 1. Crear base de datos
CREAR BASE DE DATOS escuela

# 2. Usar la base de datos
USAR escuela

# 3. Crear tablas dinámicas
CREAR TABLA estudiantes (id INT, nombre STRING(50), promedio FLOAT)
CREAR TABLA cursos (id INT, nombre STRING(50), creditos INT)

# 4. Ver las tablas creadas
MOSTRAR TABLAS

# 5. Insertar registros en estudiantes
INSERTAR estudiantes (1, Juan Perez, 8.5)
INSERTAR estudiantes (2, Maria Lopez, 9.0)
INSERTAR estudiantes (3, Carlos Ruiz, 7.5)

# 6. Insertar registros en cursos
INSERTAR cursos (101, Matematicas, 5)
INSERTAR cursos (102, Historia, 4)
INSERTAR cursos (103, Fisica, 5)

# 7. Ver todos los registros
SELECCIONAR estudiantes
SELECCIONAR cursos

# 8. Eliminar un registro
ELIMINAR estudiantes 2

# 9. Ver estudiantes después de eliminar
SELECCIONAR estudiantes

# 10. Probar transacciones
INICIAR TRANSACCION
INSERTAR estudiantes (4, Ana Gomez, 8.0)
SELECCIONAR estudiantes
DESHACER
SELECCIONAR estudiantes

# 11. Confirmar transacción
INICIAR TRANSACCION
INSERTAR estudiantes (5, Pedro Santos, 8.5)
SELECCIONAR estudiantes
CONFIRMAR
SELECCIONAR estudiantes

# 12. Eliminar tabla y base de datos
ELIMINAR TABLA estudiantes
MOSTRAR TABLAS
ELIMINAR BASE DE DATOS escuela
MOSTRAR BASES DE DATOS

# 13. Salir
SALIR
```

## Tipos de Datos para CREATE TABLE

| Tipo | Descripción | Ejemplo |
|------|-------------|---------|
| `INT` | Entero (4 bytes) | `id INT` |
| `FLOAT` | Número decimal | `precio FLOAT` |
| `STRING(n)` | Texto hasta n caracteres | `nombre STRING(50)` |

## Archivos Generados

| Archivo/Directorio | Descripción |
|---------------------|-------------|
| `motor` | Ejecutable compilado |
| `data/` | Directorio de datos |
| `data/<db>/` | Archivos de una base de datos |
| `data/<db>/<tabla>.meta` | Metadatos de tabla dinámica (esquema) |
| `data/<db>/<tabla>.csv` | Datos de tabla dinámica (formato CSV) |
| `*.dat` | Datos de tablas predefinidas (binario) |
| `*.idx` | Índices de tablas predefinidas |

## Estructura de Metadatos (.meta)

```
# Schema for table: clientes
nombre:clientes
modo:1
num_campos:3
campo:id|0|4
campo:nombre|2|50
campo:telefono|2|14
```

- `modo:1` = TABLA_DINAMICA
- `campo:nombre|tipo|longitud`
- tipo: 0=INT, 1=FLOAT, 2=STRING

## Comandos SQL Soportados (Resumen)

### Bases de Datos
| Comando | Descripción |
|---------|-------------|
| `CREAR BASE DE DATOS <nombre>` | Crear base de datos |
| `USAR <base_de_datos>` | Seleccionar base de datos |
| `MOSTRAR BASES DE DATOS` | Listar bases de datos |
| `RENOMBRAR BASE DE DATOS <viejo> <nuevo>` | Renombrar base de datos |
| `ELIMINAR BASE DE DATOS <nombre>` | Eliminar base de datos |

### Tablas
| Comando | Descripción |
|---------|-------------|
| `CREAR TABLA <nombre> (campo TIPO, ...)` | Crear tabla dinámica |
| `ELIMINAR TABLA <nombre>` | Eliminar tabla |
| `MOSTRAR TABLAS` | Listar tablas |

### Registros - Tablas Dinámicas
| Comando | Descripción |
|---------|-------------|
| `INSERTAR <tabla> (val1, val2, ...)` | Insertar registro |
| `SELECCIONAR <tabla>` | Mostrar todos los registros |
| `ELIMINAR <tabla> <id>` | Eliminar registro por ID |

### Transacciones
| Comando | Descripción |
|---------|-------------|
| `INICIAR TRANSACCION` | Iniciar transacción |
| `CONFIRMAR` | Confirmar cambios |
| `DESHACER` | Deshacer cambios |

### Utilidades
| Comando | Descripción |
|---------|-------------|
| `REINDEXAR <tabla>` | Reconstruir índice |
| `SALIR` | Salir del motor |

## Notas Importantes

1. **Sintaxis de CREATE TABLE**: Usa `campo TIPO` no `campo(tam)`
   - ✅ `CREATE TABLA clientes (id INT, nombre STRING(50))`
   - ❌ `CREATE TABLA clientes (id INT, nombre(50))`

2. **Sintaxis de INSERTAR**: Usa paréntesis con valores separados por comas
   - ✅ `INSERTAR clientes (1, Juan, 5551234)`
   - ❌ `INSERTAR clientes VALUES 1, Juan, 5551234`

3. **Transacciones**: Los cambios solo se persisten al ejecutar `CONFIRMAR` o al salir del programa sin errores

4. **Tipos de datos**: Son case-insensitive (INT = int = Int)

## Limitación de Transacciones en API

**Problema**: Cada request al API spawns un proceso motor nuevo. El estado de transacción (`en_transaccion`) se pierde entre requests.

```
Request 1: INICIAR TRANSACCION  → motor #1 arranca, tx=ON
Request 2: INSERTAR           → motor #2 arranca, tx=OFF (nuevo!)
Request 3: DESHACER            → motor #3 arranca, tx=OFF
```

**Solución implementada**: Transacciones file-based. El log se guarda en `data/<db>/_tx_log.csv`.

```
INICIAR TRANSACCION → crea data/<db>/_tx_log.csv
INSERTAR            → append a _tx_log.csv (NO al CSV principal)
CONFIRMAR           → mueve registros de _tx_log.csv → CSV principal
DESHACER            → borra _tx_log.csv (registros nunca fueron al CSV)
```

**Importante**: Una transacción a la vez por base de datos. Cerrar la transacción antes de cambiar de DB.

**Estado actual**: Transacciones funcionan en terminal. En frontend/API requieren una sesión persistente (futuro). Por ahora, usar transacciones solo en terminal.