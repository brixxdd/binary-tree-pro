use std::process::{Command, Stdio};
use std::io::Write;

#[tauri::command]
async fn execute_motor(query: String, db: Option<String>) -> Result<String, String> {
    // Motor binary and data dir are in Binary-sql/ (parent of desktop/)
    let exe_path = std::env::current_exe().unwrap_or_default();
    // In dev: exe is at desktop/src-tauri/target/debug/desktop
    // We need to go up to Binary-sql/ — the motor always runs from there
    let project_root = exe_path
        .ancestors()
        .find(|p| p.join("motor").exists())
        .map(|p| p.to_path_buf())
        .unwrap_or_else(|| std::path::PathBuf::from(".."));

    let motor_bin = project_root.join("motor");
    let mut motor = Command::new(&motor_bin)
        .current_dir(&project_root)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Error iniciando el motor C: {} (path: {:?})", e, motor_bin))?;

    if let Some(mut stdin) = motor.stdin.take() {
        if let Some(db_name) = db {
            let _ = stdin.write_all(format!("USAR {}\n", db_name).as_bytes());
        }
        let _ = stdin.write_all(format!("{}\nSALIR\n", query).as_bytes());
    }

    let output = motor.wait_with_output().map_err(|e| format!("Error leyendo output: {}", e))?;
    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    
    let mut cleaned = String::new();
    for line in stdout.lines() {
        let mut text = line;
        
        if let Some(idx) = text.find("> ") {
            if text[..idx].contains("mibd") {
                text = &text[idx + 2..];
            }
        } else if let Some(idx) = text.find(">") {
            if text[..idx].contains("mibd") {
                text = &text[idx + 1..];
            }
        }
        
        let text = text.trim();
        
        if text.is_empty() 
            || text.contains("Motor de Base de Datos") 
            || text.contains("Escribe 'exit' para salir")
            || text.contains("=========================================")
            || text.contains("Cerrando el motor de base de datos") 
            || text.contains("CONSCIENCE EDITION")
            || text.contains("[LLM]") {
            continue;
        }
        
        cleaned.push_str(text);
        cleaned.push('\n');
    }
    
    let final_output = cleaned.trim().to_string();
    if final_output.is_empty() {
        Ok("✓ Comando procesado (Sin salida o ya existía)".to_string())
    } else {
        Ok(final_output)
    }
}

fn get_project_root() -> std::path::PathBuf {
    let exe_path = std::env::current_exe().unwrap_or_default();
    let root = exe_path
        .ancestors()
        .find(|p| p.join("motor").exists())
        .map(|p| p.to_path_buf())
        .unwrap_or_else(|| std::path::PathBuf::from(".."));
    eprintln!("[DEBUG] exe={:?} root={:?}", exe_path, root);
    root
}

#[tauri::command]
fn debug_paths() -> Result<String, String> {
    let root = get_project_root();
    let data_dir = root.join("data");
    let mut info = format!("root: {:?}\ndata_dir: {:?}\ndata_exists: {}\n", root, data_dir, data_dir.exists());
    
    if data_dir.exists() {
        if let Ok(entries) = std::fs::read_dir(&data_dir) {
            for e in entries.flatten() {
                info.push_str(&format!("  {:?}\n", e.path()));
            }
        }
    }
    
    // Try escuela specifically
    let meta = root.join("data/escuela/estudiantes.meta");
    info.push_str(&format!("\nmeta_path: {:?}\nmeta_exists: {}\n", meta, meta.exists()));
    
    if meta.exists() {
        if let Ok(content) = std::fs::read_to_string(&meta) {
            info.push_str(&format!("meta_content:\n{}\n", content));
        }
    }
    
    Ok(info)
}

use std::fs;
use std::path::Path;

#[derive(serde::Serialize)]
struct TableInfo {
    name: String,
}

#[derive(serde::Serialize)]
struct DatabaseInfo {
    name: String,
    tables: Vec<TableInfo>,
}

#[tauri::command]
fn get_databases() -> Result<Vec<DatabaseInfo>, String> {
    let mut dbs = Vec::new();
    let root = get_project_root();
    let data_path = root.join("data");
    
    if data_path.exists() && data_path.is_dir() {
        if let Ok(entries) = fs::read_dir(&data_path) {
            for entry in entries.flatten() {
                if entry.file_type().map(|ft| ft.is_dir()).unwrap_or(false) {
                    let db_name = entry.file_name().into_string().unwrap_or_default();
                    let mut tables = Vec::new();
                    
                    if let Ok(table_entries) = fs::read_dir(entry.path()) {
                        for t_entry in table_entries.flatten() {
                            let file_name = t_entry.file_name().into_string().unwrap_or_default();
                            if file_name.ends_with(".meta") {
                                let table_name = file_name.replace(".meta", "");
                                tables.push(TableInfo { name: table_name });
                            }
                        }
                    }
                    dbs.push(DatabaseInfo { name: db_name, tables });
                }
            }
        }
    }
    
    Ok(dbs)
}

#[derive(serde::Serialize)]
struct DashboardMetrics {
    total_queries: u32,
    avg_latency_ms: f64,
}

#[tauri::command]
fn get_dashboard_metrics() -> Result<DashboardMetrics, String> {
    let mut total_queries = 0;
    let mut total_latency = 0.0;
    
    let root = get_project_root();
    let data_path = root.join("data");
    if data_path.exists() && data_path.is_dir() {
        if let Ok(entries) = fs::read_dir(&data_path) {
            for entry in entries.flatten() {
                if entry.file_type().map(|ft| ft.is_dir()).unwrap_or(false) {
                    let log_path = entry.path().join("_query_log.csv");
                    if log_path.exists() {
                        if let Ok(contents) = fs::read_to_string(log_path) {
                            for line in contents.lines() {
                                let parts: Vec<&str> = line.split('|').collect();
                                if parts.len() >= 5 {
                                    total_queries += 1;
                                    if let Ok(lat) = parts[2].parse::<f64>() {
                                        total_latency += lat;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    let avg = if total_queries > 0 { total_latency / (total_queries as f64) } else { 0.0 };
    Ok(DashboardMetrics {
        total_queries,
        avg_latency_ms: (avg * 10.0).round() / 10.0,
    })
}

#[derive(serde::Serialize)]
struct TableData {
    columns: Vec<String>,
    rows: Vec<Vec<String>>,
    indexes: Vec<String>,
}

#[tauri::command]
fn get_table_data(db: String, table: String) -> Result<TableData, String> {
    let root = get_project_root();
    let meta_path = root.join(format!("data/{}/{}.meta", db, table));
    let meta_contents = fs::read_to_string(&meta_path)
        .map_err(|e| format!("Meta error: {} (path: {:?})", e, meta_path))?;
    
    let mut columns = Vec::new();
    for line in meta_contents.lines() {
        if line.starts_with("campo:") {
            let parts: Vec<&str> = line[6..].split('|').collect();
            if !parts.is_empty() {
                columns.push(parts[0].to_string());
            }
        }
    }
    
    let csv_path = root.join(format!("data/{}/{}.csv", db, table));
    let mut rows = Vec::new();
    
    if let Ok(data_contents) = fs::read_to_string(&csv_path) {
        for line in data_contents.lines() {
            if line.trim().is_empty() { continue; }
            let row: Vec<String> = line.split('|').map(|s| s.to_string()).collect();
            rows.push(row);
        }
    }
    
    let mut indexes = Vec::new();
    let db_dir = root.join(format!("data/{}", db));
    if let Ok(entries) = fs::read_dir(&db_dir) {
        let prefix = format!("{}_", table);
        for entry in entries.flatten() {
            let file_name = entry.file_name().into_string().unwrap_or_default();
            if file_name.starts_with(&prefix) && file_name.ends_with(".idx") {
                let field = &file_name[prefix.len()..file_name.len() - 4];
                indexes.push(field.to_string());
            }
        }
    }
    
    Ok(TableData { columns, rows, indexes })
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            execute_motor,
            get_databases,
            get_dashboard_metrics,
            get_table_data,
            debug_paths
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
