import { useState, useEffect, useRef } from "react";
import { invoke } from "@tauri-apps/api/core";
import "./App.css";

function App() {
  const [queryInput, setQueryInput] = useState("");
  const [chatHistory, setChatHistory] = useState([]);
  const [isProcessing, setIsProcessing] = useState(false);
  const [currentDb, setCurrentDb] = useState(null);
  const [txSession, setTxSession] = useState({ active: false, pending: [] });
  
  // Novedades para Panel y Sidebar
  const [databases, setDatabases] = useState([]);
  const [metrics, setMetrics] = useState({ total_queries: 0, avg_latency_ms: 0 });
  const [latencyHistory, setLatencyHistory] = useState(Array(15).fill(0));
  const [showSplash, setShowSplash] = useState(true);
  const [activeTab, setActiveTab] = useState('chat'); // 'chat' | 'explorer'
  const [explorerData, setExplorerData] = useState(null);
  const chatEndRef = useRef(null);

  // Splash screen timer
  useEffect(() => {
    const timer = setTimeout(() => {
      setShowSplash(false);
    }, 2800); // 2.8s
    return () => clearTimeout(timer);
  }, []);

  // Auto-scroll del chat
  useEffect(() => {
    if (activeTab === 'chat') {
      chatEndRef.current?.scrollIntoView({ behavior: "smooth" });
    }
  }, [chatHistory, activeTab]);

  const fetchEngineData = async () => {
    try {
      const dbs = await invoke("get_databases");
      setDatabases(dbs);
      const met = await invoke("get_dashboard_metrics");
      setMetrics(met);
    } catch (err) {
      console.error("Error fetching engine data:", err);
    }
  };

  useEffect(() => {
    fetchEngineData();
    // Refrescar cada 5 segundos
    const interval = setInterval(fetchEngineData, 5000);
    return () => clearInterval(interval);
  }, []);

  const handleExecute = async () => {
    if (!queryInput.trim()) return;

    const query = queryInput.trim();
    const upperQuery = query.toUpperCase();
    const userMsg = { role: 'user', content: query };
    setChatHistory(prev => [...prev, userMsg]);
    setIsProcessing(true);
    setActiveTab('chat');
    
    try {
      // Detect transaction commands to update UI state, but STILL send to backend
      if (upperQuery === "INICIAR TRANSACCION" || upperQuery === "INICIAR") {
        setTxSession({ active: true });
      } else if (upperQuery === "CONFIRMAR" || upperQuery === "DESHACER") {
        setTxSession({ active: false });
      }

      // 2. Ejecutar la query real y medir tiempo para el gráfico
      const startTime = performance.now();
      const result = await invoke("execute_motor", { 
        query: query,
        db: currentDb
      });
      const endTime = performance.now();
      const latency = endTime - startTime;
      
      // Update latency history array
      setLatencyHistory(prev => {
        const newHist = [...prev.slice(1), latency];
        return newHist;
      });

      if (upperQuery.startsWith("USAR ") && result.includes("Usando base de datos")) {
        const dbName = query.split(" ")[1];
        if (dbName) setCurrentDb(dbName.trim());
      }

      setChatHistory(prev => [...prev, { role: 'conscience', content: result }]);
      fetchEngineData();

      // 4. Async AI analysis for SELECCIONAR queries (non-blocking)
      // Only analyze when the query actually returned data
      if (upperQuery.startsWith("SELECCIONAR") && currentDb && result.includes("CONTENIDO DE LA TABLA")) {
        invoke("execute_motor", { 
          query: `EXPLICAR CONSULTA ${query}`,
          db: currentDb 
        }).then(analysis => {
          if (analysis && !analysis.includes("Comando procesado") && !analysis.includes("no reconocido")) {
            setChatHistory(prev => [...prev, { 
              role: 'conscience', 
              content: `🧠 [CONSCIENCE ANALYSIS]\n${analysis}` 
            }]);
          }
        }).catch(() => {});
      }
    } catch (error) {
      setChatHistory(prev => [...prev, { role: 'error', content: String(error) }]);
    } finally {
      setIsProcessing(false);
      setQueryInput("");
    }
  };

  const handleTableClick = async (dbName, tableName) => {
    setCurrentDb(dbName);
    try {
      const data = await invoke("get_table_data", { db: dbName, table: tableName });
      console.log("TABLE DATA RECEIVED:", JSON.stringify(data));
      setExplorerData({ db: dbName, table: tableName, ...data });
      setActiveTab('explorer');
    } catch (e) {
      console.error("TABLE DATA ERROR:", e);
      // Show error visually in explorer
      setExplorerData({ db: dbName, table: tableName, columns: ["Error"], rows: [[String(e)]] });
      setActiveTab('explorer');
    }
  };

  if (showSplash) {
    return (
      <div className="splash-screen">
        <img src="/conscience_studio_logo.svg" alt="Conscience Studio" className="splash-logo animate-pulse" />
        <div className="splash-loader"></div>
      </div>
    );
  }

  return (
    <div className="app-container" style={{ 
      display: 'flex', 
      height: '100vh', 
      width: '100vw',
      overflow: 'hidden',
      background: 'var(--bg-gradient)',
      transition: 'box-shadow 0.3s ease',
      boxShadow: txSession.active ? 'inset 0 0 50px rgba(255, 149, 0, 0.4), inset 0 0 10px rgba(255, 149, 0, 0.8)' : 'none',
      border: txSession.active ? '1px solid rgba(255, 149, 0, 0.8)' : 'none'
    }}>
      {/* FLOATING TRANSACTION BAR */}
      {txSession.active && (
        <div style={{
          position: 'absolute',
          top: '20px',
          left: '50%',
          transform: 'translateX(-50%)',
          background: 'rgba(20, 20, 20, 0.9)',
          backdropFilter: 'blur(10px)',
          border: '1px solid rgba(255, 149, 0, 0.5)',
          borderRadius: '20px',
          padding: '10px 24px',
          display: 'flex',
          alignItems: 'center',
          gap: '20px',
          zIndex: 1000,
          boxShadow: '0 10px 30px rgba(255, 149, 0, 0.2)'
        }}>
          <div style={{ color: '#ff9500', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px' }}>
            <span style={{ height: '8px', width: '8px', background: '#ff9500', borderRadius: '50%', boxShadow: '0 0 10px #ff9500', animation: 'pulse 1.5s infinite' }}></span>
            TRANSACTION ACTIVE
          </div>
          <div style={{ display: 'flex', gap: '10px' }}>
            <button onClick={() => { setQueryInput("CONFIRMAR"); setTimeout(() => handleExecute(), 50); }} style={{
              background: 'rgba(48, 209, 88, 0.2)', border: '1px solid #30d158', color: '#30d158', padding: '6px 16px', borderRadius: '6px', cursor: 'pointer', fontWeight: 'bold', fontSize: '11px'
            }}>✅ CONFIRMAR</button>
            <button onClick={() => { setQueryInput("DESHACER"); setTimeout(() => handleExecute(), 50); }} style={{
              background: 'rgba(255, 69, 58, 0.2)', border: '1px solid #ff453a', color: '#ff453a', padding: '6px 16px', borderRadius: '6px', cursor: 'pointer', fontWeight: 'bold', fontSize: '11px'
            }}>❌ DESHACER</button>
          </div>
        </div>
      )}

      {/* SIDEBAR */}
      <aside className="sidebar glass">
        <div className="sidebar-header">
          <div className="logo-icon"></div>
          <h2>Conscience</h2>
          <span className="version">v0.4 Native</span>
        </div>
        
        <nav className="nav-menu">
          <div className={`nav-item ${activeTab === 'chat' ? 'active' : ''}`} onClick={() => setActiveTab('chat')}>
            🧠 Ask Conscience
          </div>
          <div className={`nav-item ${activeTab === 'explorer' ? 'active' : ''}`} onClick={() => { if(explorerData) setActiveTab('explorer') }}>
            📊 Data Explorer
          </div>
        </nav>

        <div className="databases-section" style={{ marginTop: '30px', padding: '0 20px' }}>
          <h3 style={{ fontSize: '11px', color: 'var(--text-tertiary)', textTransform: 'uppercase', letterSpacing: '1px', marginBottom: '10px' }}>Databases</h3>
          {databases.length === 0 ? (
            <div style={{ color: 'var(--text-tertiary)', fontSize: '12px' }}>No databases found</div>
          ) : (
            databases.map(db => (
              <div key={db.name} style={{ marginBottom: '15px' }}>
                <div 
                  onClick={() => setCurrentDb(db.name)}
                  style={{ 
                    display: 'flex', 
                    alignItems: 'center', 
                    gap: '8px', 
                    color: currentDb === db.name ? 'var(--accent)' : 'var(--text-secondary)',
                    fontWeight: currentDb === db.name ? '600' : '400',
                    cursor: 'pointer',
                    fontSize: '14px',
                    marginBottom: '5px'
                  }}
                >
                  <span>🗄️</span> {db.name}
                </div>
                {db.tables.length > 0 && (
                  <div style={{ paddingLeft: '22px', display: 'flex', flexDirection: 'column', gap: '5px' }}>
                    {db.tables.map(t => (
                      <div 
                        key={t.name} 
                        onClick={() => handleTableClick(db.name, t.name)}
                        style={{ 
                          color: 'var(--text-tertiary)', 
                          fontSize: '13px', 
                          cursor: 'pointer' 
                        }}
                      >
                        📄 {t.name}
                      </div>
                    ))}
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      </aside>

      {/* MAIN CONTENT */}
      <main className="main-content">
        <div className="animate-fade-in" style={{ animationDelay: '0.1s', display: 'flex', flexDirection: 'column', height: '100%' }}>
          <header className="page-header">
            <h1 className="page-title">
              {activeTab === 'chat' && 'Conscience Studio'}
              {activeTab === 'explorer' && explorerData && `Table: ${explorerData.table}`}
            </h1>
            <p className="page-subtitle">
              {activeTab === 'chat' ? 'Real-time database performance and AI insights.' : `Database: ${explorerData?.db} · ${explorerData?.rows?.length} rows · ${explorerData?.columns?.length} columns`}
            </p>
          </header>

          {activeTab === 'chat' && (
            <div className="dashboard-grid animate-fade-in" style={{ animationDelay: '0.2s', marginBottom: '20px' }}>
              <div className="glass-card">
                <div className="metric-title">Queries Executed</div>
                <div className="metric-value">{metrics.total_queries}</div>
              </div>
              <div className="glass-card" style={{ display: 'flex', flexDirection: 'column', position: 'relative' }}>
                <div className="metric-title">Live Latency (ms)</div>
                <div className="metric-value">{metrics.avg_latency_ms} ms</div>
                
                {/* SVG SPARKLINE GRAPH */}
                <div style={{ position: 'absolute', bottom: '0', left: '0', right: '0', height: '40px', opacity: 0.5 }}>
                  <svg width="100%" height="100%" preserveAspectRatio="none">
                    <polyline 
                      points={latencyHistory.map((val, i) => {
                        const x = (i / (latencyHistory.length - 1)) * 100;
                        const maxLat = Math.max(10, ...latencyHistory);
                        const y = 40 - (Math.min(val, maxLat) / maxLat * 40);
                        return `${x}%,${y}`;
                      }).join(' ')}
                      fill="none"
                      stroke="var(--accent-primary)"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </svg>
                </div>
              </div>
              <div className="glass-card">
                <div className="metric-title">Active Database</div>
                <div className="metric-value" style={{ fontSize: '20px' }}>{currentDb || "None"}</div>
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
                {chatHistory.map((msg, idx) => {
                  // Extract CREAR INDICE commands - only match real field names (not <placeholders>)
                  const indexMatch = msg.content.match(/CREAR INDICE\s+([a-zA-Z_]\w*)\s+([a-zA-Z_]\w*)/i);
                  const isAnalysis = msg.content.includes('[CONSCIENCE ANALYSIS]') || msg.content.includes('[CONSCIENCE ANALISIS]');
                  
                  return (
                    <div key={idx} style={{
                      alignSelf: msg.role === 'user' ? 'flex-end' : 'flex-start',
                      background: msg.role === 'user' 
                        ? 'rgba(10, 132, 255, 0.2)' 
                        : msg.role === 'error' 
                          ? 'rgba(255, 50, 50, 0.2)' 
                          : isAnalysis 
                            ? 'rgba(94, 92, 230, 0.15)' 
                            : 'rgba(255, 255, 255, 0.05)',
                      padding: '12px 16px',
                      borderRadius: '12px',
                      maxWidth: '80%',
                      border: '1px solid ' + (msg.role === 'user' 
                        ? 'rgba(10, 132, 255, 0.4)' 
                        : isAnalysis 
                          ? 'rgba(94, 92, 230, 0.4)' 
                          : 'rgba(255, 255, 255, 0.1)'),
                      whiteSpace: 'pre-wrap',
                      fontFamily: msg.role !== 'user' ? 'var(--font-mono)' : 'var(--font-sans)',
                      fontSize: msg.role !== 'user' ? '13px' : '15px'
                    }}>
                      {msg.content}
                      {isAnalysis && indexMatch && (
                        <button 
                          onClick={async () => {
                            const cmd = `CREAR INDICE ${indexMatch[1]} ${indexMatch[2]}`;
                            setChatHistory(prev => [...prev, 
                              { role: 'user', content: cmd },
                            ]);
                            try {
                              const result = await invoke("execute_motor", { query: cmd, db: currentDb });
                              setChatHistory(prev => [...prev, { role: 'conscience', content: result }]);
                            } catch (e) {
                              setChatHistory(prev => [...prev, { role: 'error', content: String(e) }]);
                            }
                          }}
                          style={{
                            display: 'block',
                            marginTop: '12px',
                            padding: '8px 16px',
                            background: 'linear-gradient(135deg, #30d158, #34c759)',
                            border: 'none',
                            borderRadius: '8px',
                            color: 'white',
                            fontWeight: '600',
                            fontSize: '12px',
                            cursor: 'pointer',
                            letterSpacing: '0.5px'
                          }}
                        >
                          ⚡ Apply: CREAR INDICE {indexMatch[1]} {indexMatch[2]}
                        </button>
                      )}
                    </div>
                  );
                })}
                {isProcessing && <div style={{ color: 'var(--text-secondary)' }}>Conscience is thinking...</div>}
                <div ref={chatEndRef} />
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

          {activeTab === 'explorer' && explorerData && (
            <div className="glass-card animate-fade-in" style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
              <div style={{ overflow: 'auto', flex: 1 }}>
                <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left' }}>
                  <thead>
                    <tr>
                      {explorerData.columns.map((col, i) => (
                        <th key={i} style={{ 
                          padding: '16px', 
                          borderBottom: '1px solid var(--glass-border)',
                          color: 'var(--text-secondary)',
                          fontSize: '12px',
                          textTransform: 'uppercase',
                          letterSpacing: '1px'
                        }}>
                          {col}
                          {explorerData.indexes?.includes(col) && (
                            <span style={{
                              marginLeft: '8px',
                              background: 'linear-gradient(135deg, rgba(48, 209, 88, 0.2), rgba(52, 199, 89, 0.1))',
                              color: '#34c759',
                              padding: '2px 6px',
                              borderRadius: '4px',
                              fontSize: '10px',
                              fontWeight: 'bold',
                              border: '1px solid rgba(52, 199, 89, 0.3)'
                            }}>
                              ⚡ INDEXED
                            </span>
                          )}
                        </th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {explorerData.rows.length === 0 ? (
                      <tr>
                        <td colSpan={explorerData.columns.length} style={{ padding: '30px', textAlign: 'center', color: 'var(--text-tertiary)' }}>
                          Table is empty
                        </td>
                      </tr>
                    ) : (
                      explorerData.rows.map((row, i) => (
                        <tr key={i} style={{ borderBottom: '1px solid rgba(255,255,255,0.05)' }}>
                          {row.map((cell, j) => (
                            <td key={j} style={{ padding: '16px', fontSize: '14px', fontFamily: 'var(--font-mono)' }}>
                              {cell}
                            </td>
                          ))}
                        </tr>
                      ))
                    )}
                  </tbody>
                </table>
              </div>
            </div>
          )}

        </div>
      </main>
    </div>
  );
}

export default App;
