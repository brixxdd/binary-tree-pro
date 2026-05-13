#ifndef QUERY_ANALYTICS_H
#define QUERY_ANALYTICS_H

void init_query_analytics(void);
void log_query(const char* db, const char* table, const char* query_text, double execution_time_ms, int rows_scanned);
void analyze_and_suggest_indices(const char* db);
void toggle_auto_index(const char* db, int on);
void process_auto_index(const char* db, const char* query_text, double exec_ms, const char* table);

#endif
