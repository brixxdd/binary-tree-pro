# MOTOR CONSCIENCE — Plan de Implementación

## Estado Actual del Motor (Baseline)

```
/motor                    → C binary compilado (REPL)
/src/*.c                 → Parser, DB ops, transaction, IO, index, errors
/includes/*.h            → Headers modulares
/data/<db>/              → CSVs + .meta + _tx_log.csv
/api/server.js          → Express bridge a motor
/frontend/app.js         → SPA con animaciones GSAP
```

## Visión del Proyecto

Motor de base de datos con agente LLM local que:
- Analiza queries y sugiere optimizaciones
- Detecta patrones de uso y auto-crea índices
- Explica planes de ejecución en lenguaje natural
- Alerta sobre anomalías y comportamientos sospechosos
- Traduce queries en lenguaje natural a SQL

---

## Fases de Implementación

---

### FASE 1: Embed LLM en el Motor ✅

**Estado:** COMPLETADO (MiniMax API)

- [x] **1.1 Investigar TinyLlama integration**
  - DESCARTADO: TinyLlama demasiado lento, usar MiniMax API

- [x] **1.2 Crear abstracción de LLM**
  - Creado `src/llm.c` / `includes/llm.h`
  - Interfaz: `llm_think(prompt) → respuesta`
  - Motor no depende de llama.cpp

- [x] **1.3 Memory management del modelo**
  - LLM se inicializa al start del motor
  - Context window: 2048 tokens
  - Temperatura: 0.7

- [x] **1.4 Test de integración**
  - Motor start → MiniMax API → responde a prompt
  - Latencia: ~1-2 segundos por respuesta

**Detalles de implementación:**
- MiniMax API (api.minimax.io/v1/chat/completions)
- Modelo: MiniMax-M2.7
- Auth: Bearer token en header
- Think blocks (`<think>...`) se filtran en post-proceso

---

### FASE 2: Query Analyzer (El cerebro) ✅

**Estado:** COMPLETADO

**Objetivo:** Analizar cada query y generar insights

- [x] **2.1 Hook en parser para queries SELECT**
  - Antes de ejecutar SELECT: pasar query al LLM
  - Prompt: "Analiza esta query SQL y sugiere optimizaciones"
  - Almacenar respuesta del LLM en buffer

- [x] **2.2 Parseo de estructura de query**
  - Extraer: tabla, condiciones WHERE, ORDER BY, LIMIT
  - Detectar: full table scans, missing indexes
  - Identificar:JOINs, subqueries, aggregate functions

- [x] **2.3 Generar explicación**
  - Para cada query SELECT ejecutada:
    ```
    Input:  SELECT * FROM clientes WHERE ciudad = "Buenos Aires"
    Output: "Full table scan en 5000 rows. 
            Filter: ciudad = 'Buenos Aires' → 234 rows.
            Sin índice en 'ciudad', query tardará ~180ms.
            Con índice: ~2ms. Sugerencia: CREAR INDICE clientes ON (ciudad)"
    ```

- [x] **2.4 Sistema de sugerencias**
  - Categorías: missing index, slow query, no limit, full scan
  - Prioridad: HIGH/MEDIUM/LOW
  - Acción sugerida con comando concreto

- [x] **2.5 Test FASE 2**
  - Correr varias queries, verificar que LLM responde con análisis
  - Medir latencia: análisis debe ser < 500ms

---

### FASE 3: Auto-Indexing Engine ✅

**Estado:** COMPLETADO

**Objetivo:** Motor observa patrones y crea índices automáticamente

- [x] **3.1 Query log**
  - Crear `data/<db>/_query_log.csv`
  - Formato: `timestamp|query|execution_time|rows_scanned|table`
  - Cada query ejecutada se registra

- [x] **3.2 Análisis de frecuencia**
  - En `src/query_analytics.c` / `includes/query_analytics.h`
  - Contar: cuántas veces se filtra por cada campo
  - Ejemplo: campo "ciudad" usado en 89 queries → sugerir index

- [x] **3.3 Suggestion engine**
  - Cuando campo usado > 20 veces → sugerir index
  - Cuando query tarda > 500ms → sugerir index
  - Command: `SUGERIR INDICES` → muestra análisis completo

- [x] **3.4 Auto-create index (opt-in)**
  - Nuevo comando: `AUTO INDEXAR [S/N]`
  - Si ON: motor crea índice automáticamente cuando detecta patrón
  - Si OFF: solo sugiere, no crea
  - Guardar preference en `data/<db>/_config.txt`

- [x] **3.5 Validación de mejora**
  - Después de crear índice: re-ejecutar query sample
  - Comparar tiempo antes/después
  - Mostrar al usuario: "Índice aceleró query 50x"

---

### FASE 4: Natural Language Interface ✅

**Estado:** COMPLETADO

**Objetivo:** Traducir español natural → SQL del motor

- [x] **4.1 Parser para queries en español**
  - Nuevos comandos:
    ```
    BUSCAR clientes donde ciudad es Buenos Aires
    MOSTRAR todos los clientes con telefono que empieza con 555
    CONTAR ventas donde fecha es mayor a 2026-01-01
    ```

- [x] **4.2 LLM para extracción de intent**
  - Prompt al LLM:
    ```
    "Traduce esta query en español al dialecto SQL del motor:
    Español: mostrar clientes de Buenos Aires con compras mayores a 1000
    Dialecto motor: SELECCIONAR * FROM clientes WHERE ciudad = 'Buenos Aires' AND compras > 1000"
    ```

- [x] **4.3 Validación de traducción**
  - Mostrar al usuario: "Entendí esto: SELECCIONAR * FROM..."
  - Usuario confirma antes de ejecutar
  - Opción: "Ejecutar directo" o "Corregir"

- [x] **4.4 Comandos naturales soportados**
  - `BUSCAR <tabla> [donde <campo> <comparación> <valor>]`
  - `CONTAR <tabla> [donde <condición>]`
  - `MOSTRAR <tabla> donde <condición>`
  - `TOP <n> <tabla> por <campo>`

- [x] **4.5 Test FASE 4**
  - Probar 20 queries en español diferentes
  - Verificar traducción correcta
  - Medir latencia total: NL → SQL → ejecutar

---

### FASE 5: Anomaly Detection

**Objetivo:** Detectar patrones sospechosos o errores

- [ ] **5.1 Baseline de uso normal**
  - Primeros 7 días: motor registra patterns
  - Promedio de queries por hora
  - Queries típicas por tabla
  - Volumen typical de inserts/deletes

- [ ] **5.2 Detección de outliers**
  - Comparar actividad actual vs baseline
  - Triggers:
    - > 2x queries normales en 1 hora
    - > 100 inserts en tabla sin historial de inserts
    - Query a tabla que no existe
    - Acceso a DB a horas inusuales (3am)

- [ ] **5.3 Alertas**
  - Output en terminal:
    ```
    ⚠️ ANOMALÍA DETECTADA:
    Tabla 'clientes' recibió 5000 inserts en los últimos 5 minutos.
    Histórico promedio: 50 inserts/hora.
    ¿Es esto intencional? [S/N/Bloquear IP]
    ```
  - Si N: deshacer cambios (usar transaction log)
  - Si Bloquear IP: agregar a `_blocked_ips.txt`

- [ ] **5.4 Sistema de blocklist**
  - `_blocked_ips.txt`: IPs bloqueadas
  - Motor check IP antes de procesar query
  - Si bloqueada: reject con mensaje claro

- [ ] **5.5 Test FASE 5**
  - Simular spike de inserts, verificar alerta
  - Probar block IP functionality

---

### FASE 6: Dashboard UI

**Objetivo:** Frontend que muestre insights del motor

- [ ] **6.1 Panel de métricas** (En progreso - UI lista, falta data real)
  - Queries ejecutadas hoy
  - Tiempo promedio de query
  - Índices creados
  - Tablas más consultadas

- [ ] **6.2 Query explainer visual**
  - Cada query ejecutada: expandir para ver análisis
  - Código de colores: verde (optimizado), amarillo (sugerencia), rojo (problema)

- [ ] **6.3 Suggestion cards**
  - Lista de sugerencias de índices pending
  - Click para crear índice
  - Click para descartar

- [ ] **6.4 Query history interactivo**
  - Lista de últimas 50 queries
  - Click para ver análisis completo
  - Click para repetir query

- [x] **6.5 Chat con Conscience** (¡Completado! Integrado vía Tauri)
  - Input para preguntar al motor en lenguaje natural
  - "Por qué esta query es lenta?"
  - "Qué índices me recomiendas?"
  - Respuestas del LLM renderizadas en UI

---

### FASE 7: Query Cache Inteligente

**Objetivo:** Motor aprende queries frecuentes y pre-ejecuta

- [ ] **7.1 Frequency analyzer**
  - Contar queries exactas (hash de query string)
  - Contar query patterns (SELECT * WHERE campo =)
  - Si query ejecutada > 5 veces → considerar cachear

- [ ] **7.2 Cache storage**
  - `data/<db>/_query_cache/`
  - Archivo: `<hash_query>.cache` → resultados serializados
  - TTL: 5 minutos default, configurable

- [ ] **7.3 Cache hit logic**
  - Nueva query → calcular hash
  - Si existe cache y TTL válido → retornar cache directamente
  - Si cache frío → ejecutar y guardar resultado

- [ ] **7.4 Cache invalidation**
  - Si INSERT/UPDATE/DELETE en tabla → invalidar cache de esa tabla
  - Si nuevo índice → invalidar cache de queries que usan esa tabla

- [ ] **7.5 Stats de cache**
  - Mostrar en UI: cache hits, cache misses,命中率
  - Target: > 60% hit rate para queries repetitivas

---

### FASE 8: Polish y Optimización

- [ ] **8.1 Performance tuning**
  - Batching de queries al LLM (acumular 10 queries, analizar juntas)
  - Reducir prompts: contexto mínimo necesario
  - Cache de respuestas LLM (mismo query → mismo análisis)

- [ ] **8.2 Compression de query log**
  - Rotar log cuando > 10MB
  - Comprimir logs antiguos con gzip
  - Mantener últimos 30 días de logs

- [ ] **8.3 Crash recovery para Conscience**
  - Si LLM crash, motor sigue funcionando sin análisis
  - Log de errores del LLM para debug
  - Auto-restart del LLM cada 24 horas

- [ ] **8.4 Documentación**
  - README actualizado con nuevos comandos
  - Ejemplos de queries en español
  - Guía de troubleshooting

---

## Archivos a Crear/Modificar

### Nuevos archivos:
```
includes/
  llm.h              → Abstracción del LLM
  query_analytics.h  → Análisis de patrones
  anomaly.h          → Detección de anomalías

src/
  llm.c              → Integration con MiniMax API
  query_analytics.c  → Query log y análisis
  anomaly.c          → Detección de outliers
  explain.c          → Query explainer
  nl_parser.c        → Parser de español natural

models/
  tinyllama.bin      → Modelo descargado

frontend/
  conscience-ui.js   → Dashboard de insights
  chat-panel.js      → Chat con Conscience
```

### Archivos a modificar:
```
motor.c              → Init LLM, hooks de análisis
src/parser.c         → Nuevos comandos NL
src/database.c       → Auto-indexing trigger
Makefile             → Flags estándar (-lm -lpthread)
api/server.js        → Nuevos endpoints
frontend/app.js      → Integrar Conscience UI
```

---

## Dependencias

```
MiniMax API      → https://api.minimax.io/v1/chat/completions
  - No requiere modelo local
  - Costo: gratuito (tier actual)
  - Latencia: ~1-2s por respuesta
  - Auth: Bearer token

llama.cpp        → DESCARTADO (demasiado lento para local)
TinyLlama model  → DESCARTADO (usar MiniMax API)
```

---

## Orden de Implementación Sugerido

```
Semana 1-2: FASE 1 ✅ (MiniMax API)
  → MiniMax API replace TinyLlama (lento)
  → Test inference cloud
  → Verificar que no rompe el motor existente

Semana 3-4: FASE 2
  → Query analyzer
  → Hook en parser
  → Primeros insights del LLM

Semana 5-6: FASE 3
  → Query log
  → Auto-indexing
  → Suggestion engine

Semana 7-8: FASE 4
  → Natural language parser
  → Translation engine
  → Validación de usuario

Semana 9-10: FASE 5
  → Anomaly detection
  → Alerts
  → Blocklist

Semana 11-12: FASE 6
  → Dashboard UI
  → Query explainer visual
  → Chat panel

Semana 13-14: FASE 7
  → Query cache
  → Performance optimization

Semana 15-16: FASE 8
  → Polish
  → Docs
  → Stress testing
```

---

## Testing Checklist

- [x] Motor compile con LLM linkage
- [x] `make test` existing pasa (backward compat)
- [x] LLM responde en < 5 segundos
- [ ] Query analysis muestra insights correctos
- [ ] Auto-indexing sugiere índices útiles
- [ ] Español natural traduce correctamente
- [ ] Anomaly detection detecta spikes
- [ ] Cache mejora performance de queries repetitivas
- [ ] Dashboard muestra métricas en tiempo real
- [ ] Chat con Conscience responde coherentemente

---

## Success Metrics

| Métrica | Target |
|---------|--------|
| LLM response time | < 3 segundos |
| Query analysis overhead | < 100ms por query |
| Memory usage total | < 500MB RAM |
| Auto-index accuracy | > 80% suggestions useful |
| NL translation accuracy | > 90% queries traducidas correctamente |
| Cache hit rate | > 60% para queries repetitivas |
| Anomaly detection | Detecta 95% de spikes |

---

## Riscos y Mitigaciones

| Riesgo | Mitigación |
|--------|------------|
| LLM muy lento | Async analysis, no block UI |
| Memoria excesiva | TinyLlama en vez de modelo grande |
| False positives anomaly | Threshold ajustables por usuario |
| Cache corruption | Invalidar en cada write, no信任 stale |
| Model download fail | Fallback a modo sin LLM |

---

## Comandos Nuevos del Motor

```
--- Conscience Commands ---

EXPLICAR CONSULTA <query>
  → Muestra plan de ejecución y sugerencias

SUGERIR INDICES
  → Lista índices sugeridos con理由

AUTO INDEXAR [ON/OFF]
  → Toggle auto-creation de índices

VER ANALYTICS
  → Dashboard de usage patterns

QUIÉN SOS
  → Conscience se presenta

AYUDA CONSCIENCE
  → Lista comandos disponibles

--- Natural Language ---

BUSCAR <tabla> donde <campo> <op> <valor>
CONTAR <tabla> donde <condición>
TOP <n> <tabla> por <campo>
```

---

## Configuraciones

```
# data/<db>/_conscience_config.txt
AUTO_INDEX=on
ANOMALY_THRESHOLD=2.0
CACHE_ENABLED=true
CACHE_TTL=300
MODEL_PATH=models/tinyllama.bin
LOG_QUERIES=true
```

---

*Plan creado: 2026-05-12*
*Basado en: motor C existente + TinyLlama integration*
*Meta: AI-native database con query intelligence*