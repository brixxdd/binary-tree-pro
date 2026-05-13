# MOTOR TEMPORAL — Plan de Implementación

## Estado Actual del Motor (Baseline)

```
/motor                    → C binary compilado (REPL)
/src/*.c                 → Parser, DB ops, transaction, IO, index, errors
/includes/*.h            → Headers modulares
/data/<db>/              → CSVs + .meta + _tx_log.csv
/api/server.js          → Express bridge a motor
/frontend/app.js         → SPA con animaciones GSAP
```

## Fases de Implementación

---

### FASE 1: Foundation (Core Event Log)

**Objetivo:** Transformar CSV flat a append-only event log

- [ ] **1.1 Nuevo formato de storage: `.log` files**
  - Crear `src/event_log.c` / `includes/event_log.h`
  - Formato: `timestamp|uuid_operacion|tipo_op|tabla|campos_json`
  - No más sobrescribir CSVs — solo append
  - Cada tabla tiene su `.log` file

- [ ] **1.2 Event types básicos**
  - `INSERT` → `{type:"INSERT", table:"clientes", data:{...}, ts:..., id:uuid}`
  - `UPDATE` → `{type:"UPDATE", table:"clientes", id_orig:1, data_nuevo:{...}, ts:...}`
  - `DELETE` → `{type:"DELETE", table:"clientes", id:1, ts:...}`
  - `CREATE_TABLE` → `{type:"CREATE_TABLE", table:"nueva", schema:{...}}`

- [ ] **1.3 Proyecciones (tablas materializadas)**
  - Motor mantiene `.proj/<tabla>.csv` como vista materializada
  - Se reconstruyen aplicando eventos del log
  - Init: leer todos los eventos y calcular estado final

- [ ] **1.4 Modificar parser para usar event log**
  - `INSERTAR` → escribe al log, no al CSV directo
  - `ACTUALIZAR` → escribe evento UPDATE, no muta CSV
  - `ELIMINAR` → escribe evento DELETE, no borra del CSV

- [ ] **1.5 Compilar y testear FASE 1**
  - `make test` sigue funcionando
  - Nuevo test: verificar que INSERT genera evento en log

---

### FASE 2: Time-Travel Queries

**Objetivo:** Leer estado de la base en cualquier momento del pasado

- [ ] **2.1 Query syntax nueva**
  ```
  SELECCIONAR <tabla> COMO ESTABA EN <fecha>
  SELECCIONAR <tabla> COMO ESTABA HACE <n> DIAS
  SELECCIONAR <tabla> EN OPERACION <uuid>
  ```

- [ ] **2.2 Reconstrucción de estado histórico**
  - En `src/query_time.c` / `includes/query_time.h`
  - Función: `reconstruir_estado(tabla, timestamp)` → itera eventos hasta ese punto
  - Optimización: snapshot points cada 1000 operaciones

- [ ] **2.3 Diff entre versiones**
  ```
  DIFERENCIA <tabla> ENTRE <fecha1> Y <fecha2>
  ```
  - Output: qué registros cambiaron, qué valores eran antes/después

- [ ] **2.4 Parser para nuevos comandos**
  - Agregar a `parser.c` el dispatch para `COMO ESTABA`, `DIFERENCIA`, `HACE`

- [ ] **2.5 Test FASE 2**
  - Insertar datos, borrar algunos, hacer query a fecha anterior
  - Verificar que devuelve estado correcto

---

### FASE 3: Visual Timeline (Frontend)

**Objetivo:** Mostrar línea temporal de cambios en la UI

- [ ] **3.1 Timeline component en frontend**
  - `frontend/timeline.js` — componente React o vanilla
  - Eje horizontal: tiempo
  - Cada punto: una operación
  - Click en punto → ver estado en ese momento

- [ ] **3.2 Data para timeline**
  - Nuevo endpoint API: `GET /api/timeline/<db>`
  - Devuelve array de eventos con timestamps
  - Node.js lee los `.log` files y parsea

- [ ] **3.3 Scrubber visual**
  - Slider / slider interactivo para navegar en el tiempo
  - Actualiza la vista de tabla al position del scrubber

- [ ] **3.4 Diff view**
  - Panel lado a lado: "antes" vs "después"
  - Highlight en verde lo que cambió, rojo lo que se borró

- [ ] **3.5 Animaciones GSAP**
  - Línea temporal con puntos que "brillan" en hover
  - Transiciones suaves al cambiar entre versiones

---

### FASE 4: Branching (Git-like para datos)

**Objetivo:** Crear ramas independientes de la base de datos

- [ ] **4.1 Estructura de branches**
  - `data/<db>/branches/<branch_name>/` — copia del event log
  - Branch default: `main`
  - Cada branch tiene su propio `.log` completo

- [ ] **4.2 Comandos nuevos**
  ```
  RAMIFICAR <nombre>          → Crear branch desde ahora
  LISTAR RAMAS                → Mostrar todas las ramas
  CAMBIAR RAMA <nombre>        → Switch a branch (apuntar motor a otro log)
  FUNDIR <rama_destino>       → Merge branch A → B (replay eventos)
  DESCARTAR RAMA <nombre>     → Eliminar branch
  ```

- [ ] **4.3 Merge strategy**
  - Replay todos los eventos de branch B sobre branch A
  - Si hay conflicto (mismo ID, diferentes datos): generar conflicto para usuario
  - Usuario decide: preservar A, preservar B, o manualmente

- [ ] **4.4 Implementar en C**
  - `src/branch.c` / `includes/branch.h`
  - Funciones: create_branch, list_branches, switch_branch, merge_branches

- [ ] **4.5 Test FASE 4**
  - Crear branch "test", hacer cambios en test, switch a main, verify main intacto
  - Merge, verificar conflictos

---

### FASE 5: Optimizaciones

**Objetivo:** Que no sea lento pese a event log creciente

- [ ] **5.1 Compactación periódica**
  - Cada N operaciones o N días: consolidar log → snapshot
  - Snapshot: estado actual en CSV + truncando log viejo
  - Config: `COMPACT_THRESHOLD=5000`

- [ ] **5.2 Indexación por timestamp**
  - En `.log` file: index al inicio de cada día/bloque
  - Búsqueda rápida por fecha: binary search en index, luego escanear bloque

- [ ] **5.3 Cache de proyecciones**
  - Mantener en memoria últimas 10 proyecciones solicitadas
  - Invalidar cuando nuevo evento modifica esa tabla

- [ ] **5.4 Compression de eventos**
  - Eventos UPDATE solo guardan campos que cambiaron, no registro completo
  - Implementar delta encoding

---

### FASE 6: Polish y Features Extras

- [ ] **6.1 Audit log exportable**
  ```
  EXPORTAR AUDITORIA <tabla> COMO CSV
  ```
  - Genera CSV con timeline completo de cambios

- [ ] **6.2 Query history con replay**
  - Guardar queries ejecutadas con timestamp
  - `REPETIR CONSULTA <id>` → re-ejecuta query del pasado

- [ ] **6.3 WebUI mejorado**
  - Split view: SQL input + Timeline + Results
  - Dark mode con colores de syntax highlighting
  - Guardar queries favoritas en localStorage

- [ ] **6.4 Documentación**
  - README actualizado con nuevos comandos
  - Diagrama de arquitectura
  - Ejemplos de time-travel queries

---

## Orden de Implementación Sugerido

```
Mes 1 (Semanas 1-4): FASE 1 - Foundation
  → Entender y adaptar código existente
  → Implementar event log básico
  → Mantener backward compatibility

Mes 2 (Semanas 5-8): FASE 2 - Time-Travel  
  → Queries históricas
  → Proyecciones reconstruidas

Mes 3 (Semanas 9-12): FASE 3 - Frontend
  → Timeline visual
  → Scrubber interactivo

Mes 4 (Semanas 13-16): FASE 4 - Branching
  → Ramas y merge
  → Resolución de conflictos

Mes 5 (Semanas 17-20): FASE 5 - Optimización
  → Performance
  → Compression

Mes 6 (Semanas 21-24): FASE 6 - Polish
  → UI final
  → Docs
  → Tests exhaustivos
```

---

## Archivos a Crear/Modificar

### Nuevos archivos:
```
includes/
  event_log.h
  query_time.h
  branch.h

src/
  event_log.c
  query_time.c
  branch.c

frontend/
  timeline.js
  branches.js
  diff-view.js

api/
  timeline.js    → endpoint para timeline
  branches.js    → endpoint para branches
```

### Archivos a modificar:
```
motor.c              → main loop, agregar parsing de nuevos comandos
src/parser.c         → dispatch para COMO ESTABA, RAMIFICAR, etc.
src/database.c       → dejar de escribir directo a CSV
src/io.c             → adicionar write para event log
Makefile             → agregar nuevos archivos al build
api/server.js        → nuevos endpoints
frontend/app.js      → componentes de timeline y branches
```

---

## Dependencias externas

- **Ninguna nueva** — todo en C standard library
- Frontend: same Tailwind + GSAP que ya usan
- API: same Express que ya usan

---

## Flags de compilación

```makefile
CFLAGS += -Wall -Wextra -std=c99
LDFLAGS += -lm
```

---

## Testing checklist

- [ ] Motor compile sin errores
- [ ] `make test` existing pasa (backward compat)
- [ ] INSERT genera evento en `.log`
- [ ] `SELECCIONAR COMO ESTABA EN <fecha>` devuelve estado correcto
- [ ] Timeline muestra todos los eventos
- [ ] Branch creation/switch/merge funcionan
- [ ] Merge con conflictos pregunta al usuario
- [ ] Compactación no corrompe datos
- [ ] Performance aceptable con 10k eventos

---

## Success Metrics

| Métrica | Target |
|---------|--------|
| Build time | < 5 segundos |
| INSERT latency | < 50ms |
| Query time-travel | < 200ms para 10k eventos |
| Timeline render | < 1s para 1k eventos |
| Branch switch | < 100ms |
| Merge | < 1s para 1k eventos |

---

## Riscos y Mitigaciones

| Riesgo | Mitigación |
|--------|------------|
| Event log crece rápido | Compactación agresiva + compresión |
| Reconstrucción lenta | Snapshots periódicos + cache |
| Merge conflicts complejos | UI clara para resolución manual |
| Corrupción de log | Checksums en cada evento + backup automático |

---

*Plan creado: 2026-05-12*
*Basado en: motor C existente con CSV storage*
*Meta: time-travel database con branching como git*