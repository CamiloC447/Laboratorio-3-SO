#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../include/paginacion.h"
#include "../include/tlb.h"
#include "../include/frame_allocator.h"
#include "../include/workloads.h"

// Frame allocator global (compartido por todos los threads)
static frame_allocator_t *global_frame_allocator = NULL;
static pthread_mutex_t allocator_init_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Dominio: Gestión de memoria virtual - Inicialización de página table
 * Recorrido: Aloca estructura, inicializa entradas como inválidas
 * Descripción: Crea una tabla de páginas para un thread con todas sus entradas
 *              marcadas como inválidas (no presentes en memoria).
 */
page_table_t* create_page_table(int num_pages) {
    page_table_t *table = malloc(sizeof(page_table_t));
    table->num_pages = num_pages;
    table->entries = malloc(sizeof(page_table_entry_t) * num_pages);
    
    // Inicializar todas las páginas como inválidas
    for (int i = 0; i < num_pages; i++) {
        table->entries[i].frame_number = INVALID_FRAME;
        table->entries[i].valid = 0;
        table->entries[i].referenced = 0;
        table->entries[i].dirty = 0;
    }
    
    return table;
}

/*
 * Dominio: Gestión de memoria virtual - Limpieza de página table
 * Recorrido: Libera array de entradas y estructura
 * Descripción: Libera todos los recursos de la tabla de páginas de un thread.
 */
void destroy_page_table(page_table_t *table) {
    if (table) {
        free(table->entries);
        free(table);
    }
}

/*
 * Dominio: Traducción de direcciones Descomposición virtual
 * Recorrido: Divide VA en VPN (índice de página) y offset (dentro de página)
 * Descripción: Descompone una dirección virtual en Virtual Page Number y
 *              offset usando el tamaño de página como divisor.
 */
virtual_address_page_t decompose_virtual_address(uint64_t va, int page_size) {
    virtual_address_page_t result;
    result.vpn = va / page_size;
    result.offset = va % page_size;
    return result;
}

/*
 * Dominio: Traducción de direcciones Composición física
 * Recorrido: Multiplica frame por page_size y suma offset
 * Descripción: Construye una dirección física combinando número de frame
 *              con el offset dentro de la página.
 */
uint64_t compose_physical_address(uint64_t frame_number, uint64_t offset, int page_size) {
    return frame_number * page_size + offset;
}

/*
 * Dominio: Simulación de E/S Emulación de latencia de disco
 * Recorrido: Genera delay aleatorio (1-5ms) usando nanosleep
 * Descripción: Simula la latencia de carga de página desde disco mediante
 *              un delay de 1-5 milisegundos.
 */
static void simulate_disk_load() {
    struct timespec delay;
    // Delay aleatorio entre 1-5 ms
    int delay_ms = 1 + (rand() % 5);
    delay.tv_sec = 0;
    delay.tv_nsec = delay_ms * 1000000;  // Convertir ms a ns
    nanosleep(&delay, NULL);
}

/*
 * Dominio: Manejo de excepciones - Page fault
 * Recorrido: Simula carga de disco, asigna frame, actualiza page table, detalla evictions
 * Descripción: Atiende un fallo de página simulando carga del disco, asignando
 *              un frame físico (con eviction si necesario) y actualizando la tabla.
 */
static int handle_page_fault(page_table_t *table, uint64_t vpn, int thread_id,
                             metrics_t *metrics, config_t *config) {
    // Incrementar contador de page faults
    if (!config->unsafe) {
        pthread_mutex_lock(&metrics->lock);
    }
    metrics->page_faults++;
    if (!config->unsafe) {
        pthread_mutex_unlock(&metrics->lock);
    }
    
    // Simular carga desde disco
    simulate_disk_load();
    
    // Asignar un frame
    uint64_t frame_number;
    page_info_t *evicted_page = NULL;
    
    int eviction_occurred = allocate_frame(global_frame_allocator, thread_id, vpn,
                                          &frame_number, &evicted_page);
    
    if (eviction_occurred < 0) {
        return -1;  // Error
    }
    
    if (eviction_occurred == 1) {
        // Hubo eviction
        if (!config->unsafe) {
            pthread_mutex_lock(&metrics->lock);
        }
        metrics->evictions++;
        if (!config->unsafe) {
            pthread_mutex_unlock(&metrics->lock);
        }
        
        // Nota: En un sistema real, aquí invalidaríamos la entrada en la page table
        // de la víctima. Como cada thread tiene su propia tabla y no tenemos acceso
        // cruzado, simplificamos: la eviction ya liberó el frame para reutilizar.
        free(evicted_page);
    }
    
    // Actualizar tabla de páginas
    table->entries[vpn].frame_number = frame_number;
    table->entries[vpn].valid = 1;
    table->entries[vpn].referenced = 1;
    
    return 0;
}

/*
 * Dominio: Traducción de direcciones Paginación bajo demanda
 * Recorrido: Consulta TLB -> Page table -> Page fault handling -> Composición PA
 * Descripción: Realiza traducción completa de dirección virtual a física en modo
 *              paginación, incluyendo búsqueda TLB y manejo de page faults.
 */
int translate_address_page(page_table_t *table, virtual_address_page_t va,
                           uint64_t *pa, void *tlb_ptr, void *frame_allocator_ptr,
                           metrics_t *metrics, config_t *config, int thread_id) {
    tlb_t *tlb = (tlb_t*)tlb_ptr;
    uint64_t frame_number;
    
    // 1. Consultar TLB
    if (tlb_lookup(tlb, va.vpn, &frame_number)) {
        // TLB Hit
        if (!config->unsafe) {
            pthread_mutex_lock(&metrics->lock);
        }
        metrics->tlb_hits++;
        metrics->total_operations++;
        if (!config->unsafe) {
            pthread_mutex_unlock(&metrics->lock);
        }
        
        *pa = compose_physical_address(frame_number, va.offset, config->page_size);
        return 0;
    }
    
    // TLB Miss
    if (!config->unsafe) {
        pthread_mutex_lock(&metrics->lock);
    }
    metrics->tlb_misses++;
    if (!config->unsafe) {
        pthread_mutex_unlock(&metrics->lock);
    }
    
    // 2. Consultar tabla de páginas
    if (va.vpn >= (uint64_t)table->num_pages) {
        // VPN fuera de rango
        if (!config->unsafe) {
            pthread_mutex_lock(&metrics->lock);
        }
        metrics->total_operations++;
        if (!config->unsafe) {
            pthread_mutex_unlock(&metrics->lock);
        }
        return -1;
    }
    
    page_table_entry_t *pte = &table->entries[va.vpn];
    
    if (!pte->valid) {
        // Page Fault
        if (handle_page_fault(table, va.vpn, thread_id, metrics, config) < 0) {
            if (!config->unsafe) {
                pthread_mutex_lock(&metrics->lock);
            }
            metrics->total_operations++;
            if (!config->unsafe) {
                pthread_mutex_unlock(&metrics->lock);
            }
            return -1;
        }
        pte = &table->entries[va.vpn];  // Actualizar puntero
    }
    
    // 3. Obtener frame de la tabla de páginas
    frame_number = pte->frame_number;
    
    // 4. Actualizar TLB
    tlb_insert(tlb, va.vpn, frame_number);
    
    // 5. Calcular dirección física
    *pa = compose_physical_address(frame_number, va.offset, config->page_size);
    
    if (!config->unsafe) {
        pthread_mutex_lock(&metrics->lock);
    }
    metrics->total_operations++;
    if (!config->unsafe) {
        pthread_mutex_unlock(&metrics->lock);
    }
    
    return 0;
}

/*
 * Dominio: Ejecución de carga de trabajo
 * Recorrido: Inicializa estructuras por thread, genera direcciones, traduce, recolecta métricas
 * Descripción: Función ejecutada por cada thread en modo paginación, genera carga
 *              de trabajo y recolecta estadísticas de TLB y page faults.
 */
void* paging_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    config_t *config = ctx->config;
    
    // Inicializar frame allocator global (una sola vez)
    pthread_mutex_lock(&allocator_init_lock);
    if (global_frame_allocator == NULL) {
        global_frame_allocator = create_frame_allocator(config->num_frames, config->unsafe);
    }
    pthread_mutex_unlock(&allocator_init_lock);
    
    // Inicializar generador aleatorio para este thread
    init_thread_random(config->seed, ctx->thread_id);
    
    // Crear tabla de páginas para este thread
    page_table_t *table = create_page_table(config->num_pages);
    ctx->memory_structure = table;
    
    // Crear TLB para este thread
    tlb_t *tlb = create_tlb(config->tlb_size);
    ctx->tlb = tlb;
    
    clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
    
    // Realizar operaciones
    for (int i = 0; i < config->ops_per_thread; i++) {
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        
        // Generar dirección virtual según el workload
        virtual_address_page_t va = generate_address_paging(config, config->workload);
        
        // Traducir dirección
        uint64_t pa;
        int result = translate_address_page(table, va, &pa, tlb, global_frame_allocator,
                                           ctx->global_metrics, config, ctx->thread_id);
        
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
        
        // Actualizar métricas locales (se actualizan dentro de translate_address_page)
        if (result == 0) {
            ctx->local_metrics.translations++;
        } else {
            ctx->local_metrics.faults++;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);
    
    // Copiar métricas TLB locales
    // (Ya están en global_metrics, pero las copiamos para el reporte por thread)
    
    // Cleanup
    destroy_page_table(table);
    destroy_tlb(tlb);
    
    return NULL;
}
