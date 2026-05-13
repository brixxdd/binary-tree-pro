use std::process::{Command, Stdio};
use std::io::Write;

#[tauri::command]
async fn execute_motor(query: String, db: Option<String>) -> Result<String, String> {
    // The working directory will be the root of the project (where the Tauri app is run from)
    // Since Tauri is inside 'desktop', the motor binary is in '../motor'
    let mut motor = Command::new("../motor")
        .current_dir("..")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Error iniciando el motor C: {}", e))?;

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

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![execute_motor])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
