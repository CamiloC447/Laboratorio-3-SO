/*
 * Dominio: Gestión de memoria física - Frames/Marcos de página
 * Recorrido: Asignación y liberación de frames, gestión de política FIFO para eviction
 * Descripción: Módulo que gestiona la asignación de frames físicos compartidos entre
 *              múltiples threads, incluyendo eviction de páginas según política FIFO.
 */

#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include <stdint.h>
#include <pthread.h>

/* Información de una página cargada (para eviction) */
typedef struct page_info {
    int thread_id;
    uint64_t vpn;
    uint64_t frame_number;
    struct page_info *next;
} page_info_t;

/* Frame Allocator - gestiona frames físicos compartidos */
typedef struct {
    int num_frames;
    int *free_frames;      // Bitmap: 1 = libre, 0 = ocupado
    int free_count;
    
    // Cola FIFO para eviction
    page_info_t *fifo_head;
    page_info_t *fifo_tail;
    
    pthread_mutex_t lock;  // Protege el frame allocator en modo SAFE
    int unsafe;            // 1 = no usar locks
} frame_allocator_t;

/* Funciones del Frame Allocator */
frame_allocator_t* create_frame_allocator(int num_frames, int unsafe);
void destroy_frame_allocator(frame_allocator_t *allocator);

int allocate_frame(frame_allocator_t *allocator, int thread_id, uint64_t vpn,
                   uint64_t *frame_number, page_info_t **evicted_page);

void free_frame(frame_allocator_t *allocator, uint64_t frame_number);

void add_to_fifo(frame_allocator_t *allocator, int thread_id, uint64_t vpn, 
                 uint64_t frame_number);

#endif /* FRAME_ALLOCATOR_H */
