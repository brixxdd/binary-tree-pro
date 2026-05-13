#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include "includes/llm.h"

int main() {
    if (llm_init(NULL) != 0) {
        printf("Error inicializando LLM\n");
        return 1;
    }
    
    const char* test_queries[] = {
        "BUSCAR clientes donde ciudad es Buenos Aires",
        "MOSTRAR todos los clientes con telefono que empieza con 555",
        "CONTAR ventas donde fecha es mayor a 2026-01-01",
        "Dime cuantos usuarios hay en la tabla users",
        "Busca en inventario los productos con stock menor a 10",
        "Muestra los empleados del departamento de ventas",
        "Top 5 clientes por compras",
        "Cuales son las facturas impagas",
        "Buscar pacientes con fiebre",
        "Mostrar autos rojos",
        "Contar cuantos perros tienen mas de 5 años",
        "Buscar en la tabla libros el autor Borges",
        "Top 3 peliculas por rating",
        "Quiero ver los alumnos aprobados",
        "Buscar ordenes de compra de hoy",
        "Mostrar transacciones rechazadas",
        "Cuantos tickets estan abiertos",
        "Buscar logs con nivel error",
        "Top 10 canciones mas escuchadas",
        "Mostrar productos sin stock"
    };
    
    int num_queries = sizeof(test_queries) / sizeof(test_queries[0]);
    
    printf("=========================================\n");
    printf("   Iniciando Test FASE 4 (20 queries)\n");
    printf("=========================================\n\n");
    
    double total_ms = 0;
    
    for (int i=0; i<num_queries; i++) {
        struct timeval start, end;
        
        char prompt[1024];
        snprintf(prompt, sizeof(prompt),
            "Traduce esta orden natural al dialecto SQL de este motor.\n"
            "Dialecto motor:\n"
            "- SELECCIONAR <tabla> WHERE <condicion>\n"
            "- INSERTAR <tabla> (val1, val2)\n"
            "Si pide 'CONTAR' o 'TOP', usa 'SELECCIONAR <tabla> WHERE <condicion>' y el motor lo entenderá.\n"
            "Solo responde con la query traducida, sin comillas extra ni backticks ni formato markdown.\n"
            "Orden: %s", test_queries[i]);
        
        gettimeofday(&start, NULL);
        const char* raw_res = llm_think(prompt);
        gettimeofday(&end, NULL);
        
        double exec_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        total_ms += exec_ms;
        
        const char* res = raw_res;
        while (*res == ' ' || *res == '\n' || *res == '\r') res++;
        
        printf("Test %02d:\n", i+1);
        printf("  NL   : %s\n", test_queries[i]);
        printf("  SQL  : %s\n", res);
        printf("  Latencia: %.2f ms\n\n", exec_ms);
    }
    
    printf("--- RESULTADOS FASE 4 ---\n");
    printf("Queries probadas: %d\n", num_queries);
    printf("Latencia total: %.2f ms\n", total_ms);
    printf("Latencia promedio: %.2f ms por query\n", total_ms / num_queries);
    
    return 0;
}
