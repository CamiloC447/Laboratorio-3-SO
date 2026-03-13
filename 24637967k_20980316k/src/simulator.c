#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include "../include/simulator.h"
#include "../include/segmentacion.h"
#include "../include/paginacion.h"

/**
 * parse_arguments
 * Dominio: Inicialización del simulador - Parsing de línea de comandos
 * Recorrido: Define defaults, procesa opciones long_options, asigna parámetros
 * Descripción: Parsea argumentos de línea de comandos para configurar simulador,
 *              soportando modos segmentación/paginación y parámetros de workload.
 */
void parse_arguments(int argc, char *argv[], config_t *config) {
    // Valores por defecto
    config->mode = MODE_PAGING;  // Será sobrescrito obligatoriamente
    config->num_threads = 1;
    config->ops_per_thread = 1000;
    config->workload = WORKLOAD_UNIFORM;
    config->seed = 42;
    config->unsafe = 0;
    config->show_stats = 0;
    
    // Segmentación defaults
    config->num_segments = 4;
    config->seg_limits = NULL;
    
    // Paginación defaults
    config->num_pages = 64;
    config->num_frames = 32;
    config->page_size = 4096;
    config->tlb_size = 16;
    
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"threads", required_argument, 0, 't'},
        {"ops-per-thread", required_argument, 0, 'o'},
        {"workload", required_argument, 0, 'w'},
        {"seed", required_argument, 0, 's'},
        {"unsafe", no_argument, 0, 'u'},
        {"stats", no_argument, 0, 'S'},
        {"segments", required_argument, 0, 'g'},
        {"seg-limits", required_argument, 0, 'l'},
        {"pages", required_argument, 0, 'p'},
        {"frames", required_argument, 0, 'f'},
        {"page-size", required_argument, 0, 'z'},
        {"tlb-size", required_argument, 0, 'T'},
        {"tlb-policy", required_argument, 0, 'P'},
        {"evict-policy", required_argument, 0, 'E'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                if (strcmp(optarg, "seg") == 0) {
                    config->mode = MODE_SEGMENTATION;
                } else if (strcmp(optarg, "page") == 0) {
                    config->mode = MODE_PAGING;
                } else {
                    fprintf(stderr, "Modo inválido: %s. Use 'seg' o 'page'\n", optarg);
                    exit(1);
                }
                break;
            case 't':
                config->num_threads = atoi(optarg);
                break;
            case 'o':
                config->ops_per_thread = atoi(optarg);
                break;
            case 'w':
                if (strcmp(optarg, "uniform") == 0) {
                    config->workload = WORKLOAD_UNIFORM;
                } else if (strcmp(optarg, "80-20") == 0) {
                    config->workload = WORKLOAD_80_20;
                } else {
                    fprintf(stderr, "Workload inválido: %s\n", optarg);
                    exit(1);
                }
                break;
            case 's':
                config->seed = (unsigned int)atoi(optarg);
                break;
            case 'u':
                config->unsafe = 1;
                break;
            case 'S':
                config->show_stats = 1;
                break;
            case 'g':
                config->num_segments = atoi(optarg);
                break;
            case 'l':
                // Parse CSV de límites
                config->seg_limits = malloc(sizeof(uint64_t) * config->num_segments);
                char *token = strtok(optarg, ",");
                int idx = 0;
                while (token != NULL && idx < config->num_segments) {
                    config->seg_limits[idx++] = (uint64_t)atoll(token);
                    token = strtok(NULL, ",");
                }
                break;
            case 'p':
                config->num_pages = atoi(optarg);
                break;
            case 'f':
                config->num_frames = atoi(optarg);
                break;
            case 'z':
                config->page_size = atoi(optarg);
                break;
            case 'T':
                config->tlb_size = atoi(optarg);
                break;
            case 'P':
            case 'E':
                // Solo soportamos FIFO, ignorar por ahora
                break;
            default:
                fprintf(stderr, "Uso: %s --mode {seg|page} [opciones]\n", argv[0]);
                exit(1);
        }
    }
    
    // Si no se especificaron límites, usar default de 4096 para todos
    if (config->mode == MODE_SEGMENTATION && config->seg_limits == NULL) {
        config->seg_limits = malloc(sizeof(uint64_t) * config->num_segments);
        for (int i = 0; i < config->num_segments; i++) {
            config->seg_limits[i] = 4096;
        }
    }
}

/**
 * init_metrics
 * Dominio: Inicialización del simulador - Métricas globales
 * Recorrido: Limpia estructura, inicializa mutex si es SAFE
 * Descripción: Inicializa la estructura de métricas globales, limpiando contadores
 *              y configurando sincronización según modo SAFE/UNSAFE.
 */
void init_metrics(metrics_t *metrics, int unsafe) {
    memset(metrics, 0, sizeof(metrics_t));
    if (!unsafe) {
        pthread_mutex_init(&metrics->lock, NULL);
    }
}

/**
 * update_metrics
 * Dominio: Sincronización - Actualización segura de métricas
 * Recorrido: Adquiere lock, ejecuta función de actualización, libera lock
 * Descripción: Patrón auxiliar para actualizar métricas de forma thread-safe,
 *              manejando sincronización automática en modo SAFE.
 */
void update_metrics(metrics_t *metrics, int unsafe, void (*update_fn)(metrics_t*)) {
    if (!unsafe) {
        pthread_mutex_lock(&metrics->lock);
    }
    update_fn(metrics);
    if (!unsafe) {
        pthread_mutex_unlock(&metrics->lock);
    }
}

/**
 * print_stats
 * Dominio: Reporte de resultados - Estadísticas en consola
 * Recorrido: Formatea e imprime configuración, métricas globales y por thread
 * Descripción: Imprime un reporte completo de estadísticas del simulador incluyendo
 *              configuración, métricas globales, TLB/page faults y throughput.
 */
void print_stats(config_t *config, metrics_t *metrics, double runtime_sec,
                 thread_metrics_t *thread_metrics) {
    printf("========================================\n");
    printf("SIMULADOR DE MEMORIA VIRTUAL\n");
    printf("Modo: %s\n", config->mode == MODE_SEGMENTATION ? "SEGMENTACIÓN" : "PAGINACIÓN");
    printf("========================================\n");
    printf("Configuración:\n");
    printf("  Threads: %d\n", config->num_threads);
    printf("  Ops por thread: %d\n", config->ops_per_thread);
    printf("  Workload: %s\n", config->workload == WORKLOAD_UNIFORM ? "uniform" : "80-20");
    printf("  Seed: %u\n", config->seed);
    printf("  Modo: %s\n", config->unsafe ? "UNSAFE" : "SAFE");
    
    if (config->mode == MODE_SEGMENTATION) {
        printf("  Segmentos: %d\n", config->num_segments);
        printf("  Límites: ");
        for (int i = 0; i < config->num_segments; i++) {
            printf("%lu%s", config->seg_limits[i], i < config->num_segments - 1 ? "," : "");
        }
        printf("\n");
    } else {
        printf("  Páginas: %d\n", config->num_pages);
        printf("  Frames: %d\n", config->num_frames);
        printf("  Tamaño de página: %d bytes\n", config->page_size);
        printf("  Tamaño TLB: %d\n", config->tlb_size);
    }
    
    printf("\nMétricas Globales:\n");
    
    if (config->mode == MODE_SEGMENTATION) {
        printf("  Traducciones exitosas: %lu\n", metrics->translations_ok);
        printf("  Segfaults: %lu\n", metrics->segfaults);
        printf("  Total operaciones: %lu\n", metrics->total_operations);
    } else {
        uint64_t total_accesses = metrics->tlb_hits + metrics->tlb_misses;
        double hit_rate = total_accesses > 0 ? 
            (double)metrics->tlb_hits / total_accesses * 100.0 : 0.0;
        
        printf("  TLB Hits: %lu\n", metrics->tlb_hits);
        printf("  TLB Misses: %lu\n", metrics->tlb_misses);
        printf("  Hit Rate: %.2f%%\n", hit_rate);
        printf("  Page Faults: %lu\n", metrics->page_faults);
        printf("  Evictions: %lu\n", metrics->evictions);
        printf("  Total operaciones: %lu\n", metrics->total_operations);
    }
    
    double avg_time_ns = metrics->total_operations > 0 ?
        (double)metrics->total_translation_time_ns / metrics->total_operations : 0.0;
    double throughput = runtime_sec > 0 ? metrics->total_operations / runtime_sec : 0.0;
    
    printf("  Tiempo promedio de traducción: %.2f ns\n", avg_time_ns);
    printf("  Throughput: %.2f ops/seg\n", throughput);
    
    if (thread_metrics) {
        printf("\nMétricas por Thread:\n");
        for (int i = 0; i < config->num_threads; i++) {
            printf("  Thread %d: ", thread_metrics[i].thread_id);
            if (config->mode == MODE_SEGMENTATION) {
                printf("translations=%lu, faults=%lu\n",
                       thread_metrics[i].translations, thread_metrics[i].faults);
            } else {
                printf("tlb_hits=%lu, tlb_misses=%lu, page_faults=%lu\n",
                       thread_metrics[i].tlb_hits, thread_metrics[i].tlb_misses,
                       thread_metrics[i].faults);
            }
        }
    }
    
    printf("\nTiempo total: %.3f segundos\n", runtime_sec);
    printf("========================================\n");
}

/**
 * save_summary_json
 * Dominio: Reporte de resultados - Exportación JSON
 * Recorrido: Abre archivo out/summary.json, escribe configuración y métricas
 * Descripción: Exporta un resumen completo del simulador en formato JSON para
 *              análisis posterior.
 */
void save_summary_json(config_t *config, metrics_t *metrics, double runtime_sec) {
    FILE *f = fopen("out/summary.json", "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear out/summary.json\n");
        return;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"mode\": \"%s\",\n", 
            config->mode == MODE_SEGMENTATION ? "segmentation" : "paging");
    fprintf(f, "  \"config\": {\n");
    fprintf(f, "    \"threads\": %d,\n", config->num_threads);
    fprintf(f, "    \"ops_per_thread\": %d,\n", config->ops_per_thread);
    fprintf(f, "    \"workload\": \"%s\",\n",
            config->workload == WORKLOAD_UNIFORM ? "uniform" : "80-20");
    fprintf(f, "    \"seed\": %u,\n", config->seed);
    fprintf(f, "    \"unsafe\": %s", config->unsafe ? "true" : "false");
    
    if (config->mode == MODE_SEGMENTATION) {
        fprintf(f, ",\n    \"num_segments\": %d", config->num_segments);
    } else {
        fprintf(f, ",\n    \"pages\": %d,\n", config->num_pages);
        fprintf(f, "    \"frames\": %d,\n", config->num_frames);
        fprintf(f, "    \"page_size\": %d,\n", config->page_size);
        fprintf(f, "    \"tlb_size\": %d,\n", config->tlb_size);
        fprintf(f, "    \"tlb_policy\": \"fifo\",\n");
        fprintf(f, "    \"evict_policy\": \"fifo\"");
    }
    
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"metrics\": {\n");
    
    if (config->mode == MODE_SEGMENTATION) {
        fprintf(f, "    \"translations_ok\": %lu,\n", metrics->translations_ok);
        fprintf(f, "    \"segfaults\": %lu,\n", metrics->segfaults);
    } else {
        uint64_t total = metrics->tlb_hits + metrics->tlb_misses;
        double hit_rate = total > 0 ? (double)metrics->tlb_hits / total : 0.0;
        
        fprintf(f, "    \"tlb_hits\": %lu,\n", metrics->tlb_hits);
        fprintf(f, "    \"tlb_misses\": %lu,\n", metrics->tlb_misses);
        fprintf(f, "    \"hit_rate\": %.4f,\n", hit_rate);
        fprintf(f, "    \"page_faults\": %lu,\n", metrics->page_faults);
        fprintf(f, "    \"evictions\": %lu,\n", metrics->evictions);
    }
    
    double avg_time = metrics->total_operations > 0 ?
        (double)metrics->total_translation_time_ns / metrics->total_operations : 0.0;
    double throughput = runtime_sec > 0 ? metrics->total_operations / runtime_sec : 0.0;
    
    fprintf(f, "    \"avg_translation_time_ns\": %.2f,\n", avg_time);
    fprintf(f, "    \"throughput_ops_sec\": %.2f\n", throughput);
    fprintf(f, "  },\n");
    fprintf(f, "  \"runtime_sec\": %.4f\n", runtime_sec);
    fprintf(f, "}\n");
    
    fclose(f);
}

/**
 * cleanup_config
 * Dominio: Limpieza - Liberación de recursos de configuración
 * Recorrido: Libera array de límites de segmentos
 * Descripción: Libera dinámicamente los recursos asociados a la estructura
 *              de configuración del simulador.
 */
void cleanup_config(config_t *config) {
    if (config->seg_limits) {
        free(config->seg_limits);
        config->seg_limits = NULL;
    }
}

/**
 * timespec_diff_ns
 * Dominio: Utilidades de medición - Cálculo de diferencia temporal
 * Recorrido: Calcula diferencia de segundos y nanosegundos, retorna total en ns
 * Descripción: Calcula la diferencia temporal entre dos puntos usando struct timespec,
 *              retornando el resultado en nanosegundos.
 */
uint64_t timespec_diff_ns(struct timespec *start, struct timespec *end) {
    uint64_t ns = (end->tv_sec - start->tv_sec) * 1000000000ULL;
    ns += (end->tv_nsec - start->tv_nsec);
    return ns;
}

/**
 * timespec_to_seconds
 * Dominio: Utilidades de medición - Conversión a segundos
 * Recorrido: Convierte segundos y nanosegundos a valores de punto flotante
 * Descripción: Convierte una estructura struct timespec a su representación
 *              en segundos como número de punto flotante.
 */
double timespec_to_seconds(struct timespec *ts) {
    return ts->tv_sec + ts->tv_nsec / 1000000000.0;
}

/**
 * main
 * Dominio: Orquestación del simulador - Punto de entrada
 * Recorrido: Parsea args, inicializa métricas, crea threads, recopila resultados
 * Descripción: Función principal que orquesta toda la ejecución del simulador,
 *              creando threads de trabajo, esperando su finalización y generando reportes.
 */
int main(int argc, char *argv[]) {
    config_t config;
    metrics_t global_metrics;
    
    // Parse argumentos
    parse_arguments(argc, argv, &config);
    
    // Inicializar métricas
    init_metrics(&global_metrics, config.unsafe);
    
    // Inicializar generador aleatorio
    srand(config.seed);
    
    // Crear threads
    pthread_t *threads = malloc(sizeof(pthread_t) * config.num_threads);
    thread_context_t *contexts = malloc(sizeof(thread_context_t) * config.num_threads);
    
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    // Lanzar threads según el modo
    for (int i = 0; i < config.num_threads; i++) {
        contexts[i].thread_id = i;
        contexts[i].config = &config;
        contexts[i].global_metrics = &global_metrics;
        memset(&contexts[i].local_metrics, 0, sizeof(thread_metrics_t));
        contexts[i].local_metrics.thread_id = i;
        
        if (config.mode == MODE_SEGMENTATION) {
            pthread_create(&threads[i], NULL, segmentation_thread, &contexts[i]);
        } else {
            pthread_create(&threads[i], NULL, paging_thread, &contexts[i]);
        }
    }
    
    // Esperar a que terminen
    for (int i = 0; i < config.num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double runtime_sec = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
    
    // Recopilar métricas de threads
    thread_metrics_t *thread_metrics = malloc(sizeof(thread_metrics_t) * config.num_threads);
    for (int i = 0; i < config.num_threads; i++) {
        thread_metrics[i] = contexts[i].local_metrics;
    }
    
    // Imprimir stats si se solicitó
    if (config.show_stats) {
        print_stats(&config, &global_metrics, runtime_sec, thread_metrics);
    }
    
    // Guardar JSON
    save_summary_json(&config, &global_metrics, runtime_sec);
    
    // Cleanup
    free(threads);
    free(contexts);
    free(thread_metrics);
    cleanup_config(&config);
    
    if (!config.unsafe) {
        pthread_mutex_destroy(&global_metrics.lock);
    }
    
    return 0;
}
