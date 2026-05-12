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
    origin: process.env.FRONTEND_URL || '*',
    methods: ['GET', 'POST']
}));
app.use(express.json());

// Motor paths
const motorRelativePath = process.env.DB_ENGINE_PATH || '../motor';
const MOTOR_EXEC = path.resolve(__dirname, motorRelativePath);
const MOTOR_CWD = path.dirname(MOTOR_EXEC);

function cleanOutput(stdoutData) {
    return stdoutData
        .split('\n')
        .filter(line =>
            !line.includes('Motor de Base de Datos v0.3 Iniciado') &&
            !line.includes('Escribe \'exit\' para salir') &&
            !line.includes('=========================================') &&
            !line.includes('Cerrando el motor de base de datos... Adios.')
        )
        .join('\n')
        .replace(/mibd(:[a-zA-Z0-9_]+)?>/g, '')
        .trim();
}

function execMotor(commands) {
    return new Promise((resolve, reject) => {
        const motorProcess = spawn(MOTOR_EXEC, [], { cwd: MOTOR_CWD });
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
