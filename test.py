lines = [
    "=========================================",
    "   Motor de Base de Datos v0.4 Iniciado  ",
    "   CONSCIENCE EDITION                   ",
    "=========================================",
    "",
    "[LLM] MiniMax API mode initialized. Model: MiniMax-M2.7",
    "mibd> => Base de datos 'escuela' creada.",
    "mibd> Cerrando el motor de base de datos... Adios."
]

cleaned = ""
for line in lines:
    text = line
    idx = text.find("> ")
    if idx != -1 and "mibd" in text[:idx]:
        text = text[idx + 2:]
    else:
        idx = text.find(">")
        if idx != -1 and "mibd" in text[:idx]:
            text = text[idx + 1:]
            
    text = text.strip()
    
    if (not text or 
        "Motor de Base de Datos" in text or 
        "Escribe 'exit' para salir" in text or 
        "=========================================" in text or 
        "Cerrando el motor de base de datos" in text or 
        "CONSCIENCE EDITION" in text or 
        "[LLM]" in text):
        continue
        
    cleaned += text + "\n"

print(f"CLEANED: '{cleaned}'")
