#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/workloads.h"

// Generador aleatorio thread-local
static __thread unsigned int thread_seed;
static __thread int thread_initialized = 0;

/*
 * Dominio: Generación de números aleatorios - Inicialización por thread
 * Recorrido: Deriva semilla única por thread, inicializa generador
 * Descripción: Inicializa el generador de números aleatorios para un thread específico,
 *              derivando semilla única a partir de base_seed e ID del thread.
 */
void init_thread_random(unsigned int base_seed, int thread_id) {
    // Derivar semilla única por thread
    thread_seed = base_seed + thread_id * 1000;
    srand(thread_seed);
    thread_initialized = 1;
}

/*
 * Dominio: Generación de números aleatorios
 * Recorrido: Usa modulo para acotación del número aleatorio
 * Descripción: Genera un número aleatorio en el rango [min, max] inclusive,
 *              con distribución uniforme.
 */
int random_range(int min, int max) {
    if (!thread_initialized) {
        // Fallback: usar rand() estándar
        return min + (rand() % (max - min + 1));
    }
    return min + (rand() % (max - min + 1));
}

/*
 * Dominio: Generación de carga de trabajo direcciones de segmentación
 * Recorrido: Elige segmento según workload, genera offset, ocasionalmente causa segfault
 * Descripción: Genera direcciones virtuales para modo segmentación con patrones
 *              uniform o 80-20, incluyendo 5% de intentos fuera de límites.
 */
virtual_address_seg_t generate_address_segmentation(config_t *config, workload_t workload) {
    virtual_address_seg_t va;
    
    if (workload == WORKLOAD_UNIFORM) {
        // Uniform: seleccionar segmento uniformemente
        va.seg_id = random_range(0, config->num_segments - 1);
        
        // Seleccionar offset uniformemente dentro del límite
        uint64_t limit = config->seg_limits[va.seg_id];
        if (limit > 0) {
            va.offset = random_range(0, limit - 1);
        } else {
            va.offset = 0;
        }
    } else {
        // 80-20: 80% de probabilidad de acceder a los primeros 20% de segmentos
        int hotspot_size = (config->num_segments * 20) / 100;
        if (hotspot_size < 1) hotspot_size = 1;
        
        int prob = random_range(0, 99);  // 0-99
        
        if (prob < 80) {
            // 80% -> primeros 20% de segmentos
            va.seg_id = random_range(0, hotspot_size - 1);
        } else {
            // 20% -> resto de segmentos
            if (hotspot_size < config->num_segments) {
                va.seg_id = random_range(hotspot_size, config->num_segments - 1);
            } else {
                va.seg_id = 0;
            }
        }
        
        // Seleccionar offset uniformemente dentro del límite
        uint64_t limit = config->seg_limits[va.seg_id];
        if (limit > 0) {
            va.offset = random_range(0, limit - 1);
        } else {
            va.offset = 0;
        }
    }
    
    // Ocasionalmente generar offset fuera del límite para simular segfaults
    // (5% de probabilidad)
    if (random_range(0, 99) < 5) {
        uint64_t limit = config->seg_limits[va.seg_id];
        va.offset = limit + random_range(0, 100);
    }
    
    return va;
}

/*
 * Dominio: Generación de carga de trabajo direcciones de paginación
 * Recorrido: Elige VPN según workload, genera offset aleatorio dentro de página
 * Descripción: Genera direcciones virtuales para modo paginación con patrones
 *              uniform o 80-20, con offsets aleatorios dentro del tamaño de página.
 */
virtual_address_page_t generate_address_paging(config_t *config, workload_t workload) {
    virtual_address_page_t va;
    
    if (workload == WORKLOAD_UNIFORM) {
        // Uniform: seleccionar VPN uniformemente
        va.vpn = random_range(0, config->num_pages - 1);
    } else {
        // 80-20: 80% de probabilidad de acceder a los primeros 20% de páginas
        int hotspot_size = (config->num_pages * 20) / 100;
        if (hotspot_size < 1) hotspot_size = 1;
        
        int prob = random_range(0, 99);  // 0-99
        
        if (prob < 80) {
            // 80% -> primeros 20% de páginas
            va.vpn = random_range(0, hotspot_size - 1);
        } else {
            // 20% -> resto de páginas
            if (hotspot_size < config->num_pages) {
                va.vpn = random_range(hotspot_size, config->num_pages - 1);
            } else {
                va.vpn = 0;
            }
        }
    }
    
    // Seleccionar offset uniformemente dentro de la página
    va.offset = random_range(0, config->page_size - 1);
    
    return va;
}
