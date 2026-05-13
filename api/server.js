require('dotenv').config();
const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// Estado de transacciones por base de datos
const dbSessions = {};

// Configuración recomendada para despliegue
app.use(cors({
    origin: '*',
    methods: ['GET', 'POST']
}));
app.use(express.json());

// Motor paths
const MOTOR_EXEC = path.resolve(__dirname, '../motor');
const MOTOR_CWD = path.dirname(MOTOR_EXEC);  // project root = Binary-sql/
// Override CWD to project root so motor can find data/
const PROJECT_ROOT = path.resolve(__dirname, '..');

function cleanOutput(stdoutData) {
    const lines = stdoutData.split('\n');
    const filtered = lines.filter(line => {
        const l = line.trim();
        if (l.length === 0) return false;
        // Skip header/footer lines
        if (l.includes('Motor de Base de Datos')) return false;
        if (l.includes('CONSCIENCE EDITION')) return false;
        if (l.includes('[LLM] MiniMax')) return false;
        if (l.includes('================')) return false;
        if (l.includes('Escribe')) return false;
        if (l.includes('Cerrando')) return false;
        if (l.includes('Adios')) return false;
        if (l.includes('---')) return false;
        return true;
    }).map(line => {
        // Strip the mibd prompt prefix from content lines: "mibd> " or "mibd:db> "
        return line.replace(/^mibd[^>]*>\s*/, '');
    });
    return filtered.join('\n').trim();
}

function execMotor(commands) {
    return new Promise((resolve, reject) => {
        const motorProcess = spawn(MOTOR_EXEC, [], { cwd: PROJECT_ROOT });
        let stdoutData = '';
        let stderrData = '';

        motorProcess.stdout.on('data', (data) => { stdoutData += data.toString(); });
        motorProcess.stderr.on('data', (data) => { stderrData += data.toString(); });

        motorProcess.on('close', (code) => {
            resolve({ stdout: stdoutData, stderr: stderrData, code });
        });
        motorProcess.on('error', (err) => {
            reject(err);
        });

        motorProcess.stdin.write(commands);
        motorProcess.stdin.end();
    });
}

async function execQuery(db, query) {
    let commandsToRun = '';
    if (db) {
        commandsToRun += `USAR ${db}\n`;
    }
    commandsToRun += `${query}\nSALIR\n`;

    const result = await execMotor(commandsToRun);
    return cleanOutput(result.stdout);
}

function getSession(dbName) {
    if (!dbSessions[dbName]) {
        dbSessions[dbName] = {
            txActive: false,
            pendingQueries: [] // queries que se ejecutaron para poder hacer undo
        };
    }
    return dbSessions[dbName];
}

function clearSession(dbName) {
    if (dbSessions[dbName]) {
        dbSessions[dbName].txActive = false;
        dbSessions[dbName].pendingQueries = [];
    }
}

// Query endpoint con soporte de transacciones via estado en memoria
app.post('/api/query', async (req, res) => {
    const { db, query } = req.body;

    if (!query) {
        return res.status(400).json({ error: 'Falta la propiedad "query"' });
    }

    const upperQuery = query.toUpperCase().trim();

    // Manejo de transacciones desde el API
    if (upperQuery === 'INICIAR TRANSACCION' || upperQuery === 'INICIAR') {
        if (!db) {
            return res.status(400).json({ success: false, error: 'Selecciona una DB primero' });
        }
        const session = getSession(db);
        session.txActive = true;
        session.pendingQueries = [];
        return res.json({ success: true, output: '=> Transaccion iniciada (START TRANSACTION)' });
    }

    if (upperQuery === 'CONFIRMAR') {
        if (!db) {
            return res.status(400).json({ success: false, error: 'No hay DB seleccionada' });
        }
        const session = getSession(db);
        session.txActive = false;
        session.pendingQueries = [];
        return res.json({ success: true, output: '=> Transaccion confirmada (COMMIT).' });
    }

    if (upperQuery === 'DESHACER') {
        if (!db) {
            return res.status(400).json({ success: false, error: 'No hay DB seleccionada' });
        }
        const session = getSession(db);
        if (!session.txActive) {
            return res.status(400).json({ success: false, error: 'Error: No hay transaccion activa.' });
        }

        // Procesar pending queries en orden inverso para hacer undo
        for (let i = session.pendingQueries.length - 1; i >= 0; i--) {
            const q = session.pendingQueries[i];

            if (q.type === 'INSERT') {
                // Extraer tabla y el ID del INSERT para poder eliminarlo
                // Formato esperado: INSERTAR tabla (id, dato1, dato2, ...)
                const idMatch = q.original.match(/INSERTAR\s+\S+\s*\(\s*(\d+)/i);
                if (idMatch) {
                    const id = idMatch[1];
                    const deleteCmd = `USAR ${db}\nELIMINAR ${q.tabla} ${id}\nSALIR\n`;
                    await execMotor(deleteCmd);
                }
            }
            // TODO: agregar mas tipos si se necesitan (UPDATE, DELETE)
        }

        session.txActive = false;
        session.pendingQueries = [];
        return res.json({ success: true, output: '=> Transaccion cancelada (ROLLBACK).' });
    }

    // INSERTAR dentro de transaccion: trackear y ejecutar
    if (upperQuery.startsWith('INSERTAR')) {
        if (!db) {
            return res.status(400).json({ success: false, error: 'No hay DB seleccionada' });
        }
        const session = getSession(db);

        // Extraer nombre de tabla
        const tablaMatch = query.match(/INSERTAR\s+(\S+)/i);
        const tabla = tablaMatch ? tablaMatch[1] : 'unknown';

        // Ejecutar el INSERT
        const result = await execQuery(db, query);

        if (session.txActive) {
            session.pendingQueries.push({
                type: 'INSERT',
                tabla: tabla,
                original: query
            });
        }

        return res.json({ success: true, output: result });
    }

    // Query normal sin transaccion
    const result = await execQuery(db, query);
    const hasError = result.toLowerCase().includes('error');
    return res.json({ success: !hasError, output: result });
});

app.post('/api/create-db', (req, res) => {
    const { name } = req.body;

    if (!name) {
        return res.status(400).json({ error: 'Falta el nombre de la DB' });
    }

    const createProcess = spawn(MOTOR_EXEC, [], { cwd: MOTOR_CWD });
    let stdoutData = '';
    let stderrData = '';

    createProcess.stdout.on('data', (data) => { stdoutData += data.toString(); });
    createProcess.stderr.on('data', (data) => { stderrData += data.toString(); });

    createProcess.on('close', (code) => {
        const output = stdoutData.replace(/mibd(:[a-zA-Z0-9_]+)?>/g, '').trim();
        if (code === 0 || output.includes('creada') || output.includes('éxito')) {
            return res.json({ success: true, output });
        }
        return res.status(500).json({ success: false, error: stderrData || output });
    });

    createProcess.on('error', (err) => {
        res.status(500).json({ success: false, error: err.message });
    });

    createProcess.stdin.write(`CREAR ${name}\nSALIR\n`);
    createProcess.stdin.end();
});

app.post('/api/delete-db', (req, res) => {
    const { name } = req.body;

    if (!name) {
        return res.status(400).json({ error: 'Falta el nombre de la DB' });
    }

    const deleteProcess = spawn(MOTOR_EXEC, [], { cwd: MOTOR_CWD });
    let stdoutData = '';
    let stderrData = '';

    deleteProcess.stdout.on('data', (data) => { stdoutData += data.toString(); });
    deleteProcess.stderr.on('data', (data) => { stderrData += data.toString(); });

    deleteProcess.on('close', (code) => {
        const output = stdoutData.replace(/mibd(:[a-zA-Z0-9_]+)?>/g, '').trim();
        if (code === 0 || output.includes('eliminada') || output.includes('éxito') || output.includes('eliminada')) {
            return res.json({ success: true, output });
        }
        return res.status(500).json({ success: false, error: stderrData || output });
    });

    deleteProcess.on('error', (err) => {
        res.status(500).json({ success: false, error: err.message });
    });

    deleteProcess.stdin.write(`ELIMINAR BASE DE DATOS ${name}\nSALIR\n`);
    deleteProcess.stdin.end();
});

app.post('/api/rename-db', (req, res) => {
    const { oldName, newName } = req.body;

    if (!oldName || !newName) {
        return res.status(400).json({ error: 'Faltan parametros: oldName y newName requeridos' });
    }

    const renameProcess = spawn(MOTOR_EXEC, [], { cwd: MOTOR_CWD });
    let stdoutData = '';
    let stderrData = '';

    renameProcess.stdout.on('data', (data) => { stdoutData += data.toString(); });
    renameProcess.stderr.on('data', (data) => { stderrData += data.toString(); });

    renameProcess.on('close', (code) => {
        const output = stdoutData.replace(/mibd(:[a-zA-Z0-9_]+)?>/g, '').trim();
        if (code === 0 || output.includes('renombrada') || output.includes('éxito') || output.includes('renombrada')) {
            return res.json({ success: true, output });
        }
        return res.status(500).json({ success: false, error: stderrData || output });
    });

    renameProcess.on('error', (err) => {
        res.status(500).json({ success: false, error: err.message });
    });

    renameProcess.stdin.write(`RENOMBRAR BASE DE DATOS ${oldName} ${newName}\nSALIR\n`);
    renameProcess.stdin.end();
});

app.listen(PORT, () => {
    console.log(`🚀 Bridge API corriendo en el puerto ${PORT}`);
    console.log(`🔗 Usando ejecutable de DB en: ${MOTOR_EXEC}`);
});

// ============================================
// REST API: Acceso directo a datos
// ============================================

// Listar tablas de una DB
app.get('/api/db/:db/tables', async (req, res) => {
    const { db } = req.params;
    const output = await execQuery(db, 'MOSTRAR TABLAS');
    const tables = output.split('\n')
        .map(l => l.trim().replace(/^[*-]\s*/, ''))  // trim, remove bullet points
        .filter(l => l.length > 0 && !l.includes('no existe') && !l.includes('Tablas'));
    res.json({ success: true, tables });
});

// Obtener datos de una tabla
app.get('/api/db/:db/table/:table', async (req, res) => {
    const { db, table } = req.params;
    const output = await execQuery(db, `SELECCIONAR * FROM ${table}`);

    // Parsear output del motor:
    // Formato: "id: 1 | nombre: Juan Perez | promedio: 8.5"
    const lines = output.split('\n').filter(l => l.trim() && !l.includes('CONTENIDO') && !l.includes('---'));
    const rows = [];
    const columns = [];

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line || line.includes('CONTENIDO') || line.includes('---')) continue;

        // Split por "|"
        const fields = line.split('|').map(f => f.trim());
        if (fields.length === 0) continue;

        const values = [];
        const colNames = [];

        for (const field of fields) {
            const colonIdx = field.indexOf(':');
            if (colonIdx > 0) {
                const colName = field.substring(0, colonIdx).trim();
                const colValue = field.substring(colonIdx + 1).trim();

                if (i === 0) colNames.push(colName);
                values.push(colValue);
            } else {
                values.push(field);
            }
        }

        if (i === 0) {
            columns.push(...colNames);
        }
        if (values.length > 0) {
            rows.push(values);
        }
    }

    res.json({ success: true, columns, rows });
});

// Obtener schema de una tabla
app.get('/api/db/:db/table/:schema/schema', async (req, res) => {
    const { db, schema } = req.params;
    const output = await execQuery(db, `VER ESQUEMA ${schema}`);
    res.json({ success: true, schema: output });
});

// ============================================
// Generate test data
// ============================================

const firstNames = ['Juan', 'Ana', 'Carlos', 'Maria', 'Pedro', 'Laura', 'Miguel', 'Sofia', 'Diego', 'Emma', 'Luis', 'Camila', 'Andres', 'Valentina', 'Javier', 'Isabella', 'Fernando', 'Luna', 'Ricardo', 'Zoe'];
const lastNames = ['Gomez', 'Perez', 'Rodriguez', 'Lopez', 'Martinez', 'Gonzalez', 'Garcia', 'Sanchez', 'Torres', 'Ramirez', 'Flores', 'Morales', 'Cruz', 'Reyes', 'Mendez', 'Vega', 'Castro', 'Diaz', 'Ortiz', 'Rios'];
const cities = ['Buenos Aires', 'Madrid', 'Mexico DF', 'Bogota', 'Lima', 'Santiago', 'Montevideo', 'Caracas', 'Quito', 'Panama'];
const domains = ['gmail.com', 'hotmail.com', 'outlook.com', 'yahoo.com', 'proton.me'];

function randomInt(min, max) { return Math.floor(Math.random() * (max - min + 1)) + min; }
function randomFloat(min, max, dec = 1) { return (Math.random() * (max - min) + min).toFixed(dec); }
function randomPick(arr) { return arr[Math.floor(Math.random() * arr.length)]; }

function generateFakeData(table, count) {
    const results = [];
    for (let i = 0; i < count; i++) {
        if (table === 'estudiantes') {
            const id = 1000 + i;
            const name = `${randomPick(firstNames)} ${randomPick(lastNames)}`;
            const promedio = randomFloat(5, 10);
            results.push(`${id},${name},${promedio}`);  // comma-separated for INSERT parser
        } else if (table === 'cursos') {
            const id = 100 + i;
            const name = `${randomPick(['Matematicas', 'Historia', 'Fisica', 'Quimica', 'Literatura', 'Arte', 'Musica', 'Programacion', 'Economia', 'Filosofia'])} ${randomInt(1, 99)}`;
            const creditos = randomInt(1, 10);
            results.push(`${id},${name},${creditos}`);
        } else if (table === 'profesores') {
            const id = 200 + i;
            const name = `${randomPick(firstNames)} ${randomPick(lastNames)}`;
            const especialidad = randomPick(['Matematicas', 'Historia', 'Fisica', 'Quimica']);
            const telefono = `+54${randomInt(9, 15)}${randomInt(1000000000, 9999999999)}`;
            results.push(`${id},${name},${especialidad},${telefono}`);
        } else if (table === 'examenes') {
            const id = 300 + i;
            const materia = randomPick(['Matematicas', 'Historia', 'Fisica', 'Quimica', 'Literatura']);
            const fecha = `2026-${String(randomInt(1, 12)).padStart(2,'0')}-${String(randomInt(1, 28)).padStart(2,'0')}`;
            const nota = randomFloat(1, 10);
            results.push(`${id},${materia},${fecha},${nota}`);
        } else {
            // Generic fallback: ID + 2 fields
            const id = 400 + i;
            const name = `${randomPick(firstNames)} ${randomPick(lastNames)}`;
            const extra = randomFloat(0, 100);
            results.push(`${id},${name},${extra}`);
        }
    }
    return results;
}

app.post('/api/generate-data', async (req, res) => {
    const { db, table, count = 10 } = req.body;

    if (!db || !table) {
        return res.status(400).json({ error: 'Faltan db y table' });
    }

    if (count < 1 || count > 1000) {
        return res.status(400).json({ error: 'Count debe estar entre 1 y 1000' });
    }

    const fakeRows = generateFakeData(table, count);
    let inserted = 0;

    for (const row of fakeRows) {
        const result = await execQuery(db, `INSERTAR ${table} (${row})`);
        if (result.includes('insertado')) inserted++;
    }

    res.json({
        success: true,
        message: `Generados ${inserted} registros en ${table}`,
        count: inserted
    });
});
