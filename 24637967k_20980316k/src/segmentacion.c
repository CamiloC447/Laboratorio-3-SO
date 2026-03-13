#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/segmentacion.h"
#include "../include/workloads.h"

/*
 * Dominio: Gestión de memoria virtual Inicialización de segmentación
 * Recorrido: Aloca estructura, asigna bases y límites a segmentos
 * Descripción: Crea una tabla de segmentos con bases y límites especificados,
 *              separando segmentos en el espacio de direcciones.
 */
segment_table_t* create_segment_table(int num_segments, uint64_t *limits) {
    segment_table_t *table = malloc(sizeof(segment_table_t));
    table->num_segments = num_segments;
    table->segments = malloc(sizeof(segment_entry_t) * num_segments);
    
    // Inicializar segmentos con bases y límites
    uint64_t current_base = 0x10000000;  // Base arbitraria
    for (int i = 0; i < num_segments; i++) {
        table->segments[i].base = current_base;
        table->segments[i].limit = limits[i];
        current_base += limits[i] + 0x1000;  // Separación entre segmentos
    }
    
    return table;
}

/**

 * Dominio: Gestión de memoria virtual Limpieza de segmentación
 * Recorrido: Libera array de segmentos y estructura
 * Descripción: Libera los recursos de la tabla de segmentos de un thread.
 */
void destroy_segment_table(segment_table_t *table) {
    if (table) {
        free(table->segments);
        free(table);
    }
}

/**
 * Dominio: Traducción de direcciones
 * Recorrido: Valida seg_id, verifica límite, calcula PA=base+offset
 * Descripción: Realiza traducción de dirección virtual a física en modo segmentación,
 *              validando ID de segmento y límites de acceso.
 */
int translate_address_seg(segment_table_t *table, virtual_address_seg_t va,
                          uint64_t *pa, metrics_t *metrics, config_t *config) {
    // Verificar que el seg_id sea válido
    if (va.seg_id < 0 || va.seg_id >= table->num_segments) {
        // Segfault: seg_id inválido
        if (!config->unsafe) {
            pthread_mutex_lock(&metrics->lock);
        }
        metrics->segfaults++;
        metrics->total_operations++;
        if (!config->unsafe) {
            pthread_mutex_unlock(&metrics->lock);
        }
        return -1;
    }
    
    segment_entry_t *seg = &table->segments[va.seg_id];
    
    // Verificar límite: offset debe ser < limit
    if (va.offset >= seg->limit) {
        // Segfault: violación de límite
        if (!config->unsafe) {
            pthread_mutex_lock(&metrics->lock);
        }
        metrics->segfaults++;
        metrics->total_operations++;
        if (!config->unsafe) {
            pthread_mutex_unlock(&metrics->lock);
        }
        return -1;
    }
    
    // Traducción exitosa: PA = base + offset
    *pa = seg->base + va.offset;
    
    if (!config->unsafe) {
        pthread_mutex_lock(&metrics->lock);
    }
    metrics->translations_ok++;
    metrics->total_operations++;
    if (!config->unsafe) {
        pthread_mutex_unlock(&metrics->lock);
    }
    
    return 0;
}

/*
 * Dominio: Ejecución de carga de trabajo - Modo segmentación
 * Recorrido: Inicializa tabla de segmentos, genera direcciones, traduce, recolecta métricas
 * Descripción: Función ejecutada por cada thread en modo segmentación, genera carga
 *              de trabajo y recolecta estadísticas de traducciones y segfaults.
 */
void* segmentation_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    config_t *config = ctx->config;
    
    // Inicializar generador aleatorio para este thread
    init_thread_random(config->seed, ctx->thread_id);
    
    // Crear tabla de segmentos para este thread
    segment_table_t *table = create_segment_table(config->num_segments, config->seg_limits);
    ctx->memory_structure = table;
    clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
    
    // Realizar operaciones
    for (int i = 0; i < config->ops_per_thread; i++) {
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        
        // Generar dirección virtual según el workload
        virtual_address_seg_t va = generate_address_segmentation(config, config->workload);
        
        // Traducir dirección
        uint64_t pa;
        int result = translate_address_seg(table, va, &pa, ctx->global_metrics, config);
        
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        uint64_t elapsed_ns = timespec_diff_ns(&t_start, &t_end);
        
        // Actualizar métricas globales de tiempo
        if (!config->unsafe) {
            pthread_mutex_lock(&ctx->global_metrics->lock);
        }
        ctx->global_metrics->total_translation_time_ns += elapsed_ns;
        if (!config->unsafe) {
            pthread_mutex_unlock(&ctx->global_metrics->lock);
        }
        
        // Actualizar métricas locales
        if (result == 0) {
            ctx->local_metrics.translations++;
        } else {
            ctx->local_metrics.faults++;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);
    
    // Cleanup
    destroy_segment_table(table);
    
    return NULL;
}
