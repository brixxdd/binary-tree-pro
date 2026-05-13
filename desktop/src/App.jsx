import { useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import "./App.css";

function App() {
  const [activeTab, setActiveTab] = useState("dashboard");
  const [queryInput, setQueryInput] = useState("");
  const [chatHistory, setChatHistory] = useState([]);
  const [isProcessing, setIsProcessing] = useState(false);
  const [currentDb, setCurrentDb] = useState(null);
  const [txSession, setTxSession] = useState({ active: false, pending: [] });

  const handleExecute = async () => {
    if (!queryInput.trim()) return;

    const query = queryInput.trim();
    const upperQuery = query.toUpperCase();
    const userMsg = { role: 'user', content: query };
    setChatHistory(prev => [...prev, userMsg]);
    setIsProcessing(true);
    
    try {
      // 1. Interceptar comandos de transacción (igual que server.js)
      if (upperQuery === "INICIAR TRANSACCION" || upperQuery === "INICIAR") {
        if (!currentDb) throw new Error("Selecciona una DB primero (USAR <db>)");
        setTxSession({ active: true, pending: [] });
        setChatHistory(prev => [...prev, { role: 'conscience', content: "=> Transaccion iniciada (START TRANSACTION)" }]);
        setIsProcessing(false);
        setQueryInput("");
        return;
      }

      if (upperQuery === "CONFIRMAR") {
        if (!currentDb) throw new Error("Selecciona una DB primero");
        setTxSession({ active: false, pending: [] });
        setChatHistory(prev => [...prev, { role: 'conscience', content: "=> Transaccion confirmada (COMMIT)." }]);
        setIsProcessing(false);
        setQueryInput("");
        return;
      }

      if (upperQuery === "DESHACER") {
        if (!currentDb) throw new Error("Selecciona una DB primero");
        if (!txSession.active) throw new Error("Error: No hay transaccion activa.");
        
        // Ejecutar los deletes para simular rollback
        for (let i = txSession.pending.length - 1; i >= 0; i--) {
          const q = txSession.pending[i];
          if (q.type === 'INSERT') {
            const idMatch = q.original.match(/INSERTAR\s+\S+\s*\(\s*(\d+)/i);
            if (idMatch) {
              const deleteCmd = `ELIMINAR ${q.tabla} ${idMatch[1]}`;
              await invoke("execute_motor", { query: deleteCmd, db: currentDb });
            }
          }
        }
        setTxSession({ active: false, pending: [] });
        setChatHistory(prev => [...prev, { role: 'conscience', content: "=> Transaccion cancelada (ROLLBACK)." }]);
        setIsProcessing(false);
        setQueryInput("");
        return;
      }

      // 2. Ejecutar la query real
      const result = await invoke("execute_motor", { 
        query: query,
        db: currentDb
      });
      
      // 3. Trackear INSERTs si hay transaccion activa
      if (upperQuery.startsWith("INSERTAR") && txSession.active) {
        const tablaMatch = query.match(/INSERTAR\s+(\S+)/i);
        if (tablaMatch) {
          setTxSession(prev => ({
            ...prev,
            pending: [...prev.pending, { type: 'INSERT', tabla: tablaMatch[1], original: query }]
          }));
        }
      }

      // Si el comando fue "USAR db" y fue exitoso, guardamos el estado
      if (upperQuery.startsWith("USAR ") && result.includes("Usando base de datos")) {
        const dbName = query.split(" ")[1];
        if (dbName) setCurrentDb(dbName.trim());
      }

      setChatHistory(prev => [...prev, { role: 'conscience', content: result }]);
    } catch (error) {
      setChatHistory(prev => [...prev, { role: 'error', content: String(error) }]);
    } finally {
      setIsProcessing(false);
      setQueryInput("");
    }
  };

  return (
    <div className="app-container">
      {/* SIDEBAR */}
      <aside className="sidebar">
        <div className="sidebar-header">
          <div className="logo-glow">C</div>
          <div className="sidebar-title">Conscience Studio</div>
        </div>

        <nav className="nav-menu">
          <div 
            className={`nav-item ${activeTab === 'dashboard' ? 'active' : ''}`}
            onClick={() => setActiveTab('dashboard')}
          >
            📊 Dashboard
          </div>
          <div 
            className={`nav-item ${activeTab === 'chat' ? 'active' : ''}`}
            onClick={() => setActiveTab('chat')}
          >
            🧠 AI Assistant
          </div>
        </nav>
      </aside>

      {/* MAIN CONTENT */}
      <main className="main-content">
        <div className="animate-fade-in" style={{ animationDelay: '0.1s', display: 'flex', flexDirection: 'column', height: '100%' }}>
          <header className="page-header">
            <h1 className="page-title">
              {activeTab === 'dashboard' && 'Engine Overview'}
              {activeTab === 'chat' && 'Ask Conscience'}
            </h1>
            <p className="page-subtitle">Real-time database performance and AI insights.</p>
          </header>

          {activeTab === 'dashboard' && (
            <div className="dashboard-grid animate-fade-in" style={{ animationDelay: '0.2s' }}>
              <div className="glass-card">
                <div className="metric-title">Queries Executed</div>
                <div className="metric-value">1,204</div>
              </div>
              <div className="glass-card">
                <div className="metric-title">Avg. Latency</div>
                <div className="metric-value">12 ms</div>
              </div>
            </div>
          )}

          {activeTab === 'chat' && (
            <div className="glass-card animate-fade-in chat-container" style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '20px' }}>
              
              <div className="chat-messages" style={{ flex: 1, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '16px' }}>
                {chatHistory.length === 0 && (
                  <div style={{ color: 'var(--text-tertiary)', textAlign: 'center', marginTop: '40px' }}>
                    Type a Natural Language command or SQL to execute in the C Engine.
                  </div>
                )}
                {chatHistory.map((msg, idx) => (
                  <div key={idx} style={{
                    alignSelf: msg.role === 'user' ? 'flex-end' : 'flex-start',
                    background: msg.role === 'user' ? 'rgba(10, 132, 255, 0.2)' : (msg.role === 'error' ? 'rgba(255, 50, 50, 0.2)' : 'rgba(255, 255, 255, 0.05)'),
                    padding: '12px 16px',
                    borderRadius: '12px',
                    maxWidth: '80%',
                    border: '1px solid ' + (msg.role === 'user' ? 'rgba(10, 132, 255, 0.4)' : 'rgba(255, 255, 255, 0.1)'),
                    whiteSpace: 'pre-wrap',
                    fontFamily: msg.role !== 'user' ? 'var(--font-mono)' : 'var(--font-sans)',
                    fontSize: msg.role !== 'user' ? '13px' : '15px'
                  }}>
                    {msg.content}
                  </div>
                ))}
                {isProcessing && <div style={{ color: 'var(--text-secondary)' }}>Conscience is thinking...</div>}
              </div>

              <div style={{ display: 'flex', gap: '12px' }}>
                <input 
                  type="text" 
                  value={queryInput}
                  onChange={(e) => setQueryInput(e.target.value)}
                  onKeyDown={(e) => e.key === 'Enter' && handleExecute()}
                  placeholder="Ej: BUSCAR clientes donde pais es argentina"
                  style={{
                    flex: 1,
                    background: 'rgba(0, 0, 0, 0.3)',
                    border: '1px solid var(--glass-border)',
                    borderRadius: '8px',
                    padding: '12px 16px',
                    color: 'white',
                    outline: 'none',
                    fontFamily: 'var(--font-sans)'
                  }}
                />
                <button 
                  onClick={handleExecute}
                  disabled={isProcessing}
                  style={{
                    background: 'var(--accent-gradient)',
                    border: 'none',
                    borderRadius: '8px',
                    padding: '0 24px',
                    color: 'white',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  Run
                </button>
              </div>
            </div>
          )}
        </div>
      </main>
    </div>
  );
}

export default App;
