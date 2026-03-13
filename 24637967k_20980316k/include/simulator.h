/*
 * Dominio: Núcleo del simulador - Configuración, métricas y contexto de ejecución
 * Recorrido: Inicialización, recolección de estadísticas, gestión de threads
 * Descripción: Define estructuras globales de configuración, métricas del simulador
 *              y contexto de ejecución para cada thread de trabajo.
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>

/* Configuración del simulador */
typedef enum {
    MODE_SEGMENTATION,
    MODE_PAGING
} sim_mode_t;

typedef enum {
    WORKLOAD_UNIFORM,
    WORKLOAD_80_20
} workload_t;

/* Estructura de configuración global */
typedef struct {
    sim_mode_t mode;
    int num_threads;
    int ops_per_thread;
    workload_t workload;
    unsigned int seed;
    int unsafe;  // 1 para modo UNSAFE, 0 para SAFE
    int show_stats;
    
    // Segmentación
    int num_segments;
    uint64_t *seg_limits;  // Array de límites
    
    // Paginación
    int num_pages;
    int num_frames;
    int page_size;
    int tlb_size;
} config_t;

/* Métricas globales */
typedef struct {
    // Segmentación
    uint64_t translations_ok;
    uint64_t segfaults;
    
    // Paginación
    uint64_t tlb_hits;
    uint64_t tlb_misses;
    uint64_t page_faults;
    uint64_t evictions;
    
    // Comunes
    uint64_t total_translation_time_ns;
    uint64_t total_operations;
    
    pthread_mutex_t lock;  // Para proteger métricas en modo SAFE
} metrics_t;

/* Métricas por thread */
typedef struct {
    int thread_id;
    uint64_t translations;
    uint64_t faults;
    uint64_t tlb_hits;
    uint64_t tlb_misses;
} thread_metrics_t;

/* Contexto de ejecución de cada thread */
typedef struct {
    int thread_id;
    config_t *config;
    metrics_t *global_metrics;
    thread_metrics_t local_metrics;
    void *memory_structure;  // Apunta a segment_table o page_table
    void *tlb;               // Solo para paginación
    struct timespec start_time;
    struct timespec end_time;
} thread_context_t;

/* Funciones principales */
void parse_arguments(int argc, char *argv[], config_t *config);
void init_metrics(metrics_t *metrics, int unsafe);
void print_stats(config_t *config, metrics_t *metrics, double runtime_sec, 
                 thread_metrics_t *thread_metrics);
void save_summary_json(config_t *config, metrics_t *metrics, double runtime_sec);
void cleanup_config(config_t *config);

/* Utilidades de tiempo */
uint64_t timespec_diff_ns(struct timespec *start, struct timespec *end);
double timespec_to_seconds(struct timespec *ts);

#endif /* SIMULATOR_H */
