#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../include/frame_allocator.h"

/*
 * Dominio: Gestión de memoria física reservando memoria para frames
 * Recorrido: inicializa bitmap de frames y prepara mutex
 * Descripción: Crea e inicializa un frame allocator con los frames disponibles
 *              y la cola FIFO para política de eviction.
 */
frame_allocator_t* create_frame_allocator(int num_frames, int unsafe) {
    frame_allocator_t *allocator = malloc(sizeof(frame_allocator_t));
    allocator->num_frames = num_frames;
    allocator->free_frames = malloc(sizeof(int) * num_frames);
    allocator->free_count = num_frames;
    allocator->fifo_head = NULL;
    allocator->fifo_tail = NULL;
    allocator->unsafe = unsafe;
    
    // Inicialmente todos los frames están libres
    for (int i = 0; i < num_frames; i++) {
        allocator->free_frames[i] = 1;
    }
    
    // Inicializar mutex si está en modo SAFE
    if (!unsafe) {
        pthread_mutex_init(&allocator->lock, NULL);
    }
    
    return allocator;
}

/*
 * Dominio: Limpieza de allocador
 * Recorrido: Libera cola FIFO, bitmap y mutex, libera la estructura
 * Descripción: Libera todos los recursos asociados al frame allocator incluyendo
 *              la cola FIFO de páginas y sincronización.
 */
void destroy_frame_allocator(frame_allocator_t *allocator) {
    if (!allocator) {
        return;
    }
    
    // Liberar cola FIFO
    page_info_t *current = allocator->fifo_head;
    while (current) {
        page_info_t *next = current->next;
        free(current);
        current = next;
    }
    
    free(allocator->free_frames);
    
    if (!allocator->unsafe) {
        pthread_mutex_destroy(&allocator->lock);
    }
    
    free(allocator);
}

/*
 * Dominio: Mantenimiento de cola de eviction
 * Recorrido: Crea nodo de página, lo agrega al final de la cola FIFO
 * Descripción: Inserta una página recientemente cargada al final de la cola FIFO
 *              para seguimiento del orden de carga.
 */
void add_to_fifo(frame_allocator_t *allocator, int thread_id, uint64_t vpn,
                 uint64_t frame_number) {
    page_info_t *page = malloc(sizeof(page_info_t));
    page->thread_id = thread_id;
    page->vpn = vpn;
    page->frame_number = frame_number;
    page->next = NULL;
    
    if (allocator->fifo_tail) {
        allocator->fifo_tail->next = page;
    } else {
        allocator->fifo_head = page;
    }
    allocator->fifo_tail = page;
}

/*
 * Dominio: Gestión de memoria física - Asignación y eviction de frames
 * Recorrido: Busca frame libre, asigna o evicta usando FIFO, retorna página víctima
 * Descripción: Asigna un frame disponible a una página. Si no hay frames libres,
 *              evicta la página más antigua (FIFO) y retorna su información.
 */
int allocate_frame(frame_allocator_t *allocator, int thread_id, uint64_t vpn,
                   uint64_t *frame_number, page_info_t **evicted_page) {
    if (!allocator->unsafe) {
        pthread_mutex_lock(&allocator->lock);
    }
    
    *evicted_page = NULL;
    
    // Buscar un frame libre
    int frame_idx = -1;
    for (int i = 0; i < allocator->num_frames; i++) {
        if (allocator->free_frames[i]) {
            frame_idx = i;
            break;
        }
    }
    
    if (frame_idx >= 0) {
        // Frame libre encontrado
        allocator->free_frames[frame_idx] = 0;
        allocator->free_count--;
        *frame_number = (uint64_t)frame_idx;
        
        // Agregar a la cola FIFO
        add_to_fifo(allocator, thread_id, vpn, *frame_number);
        
        if (!allocator->unsafe) {
            pthread_mutex_unlock(&allocator->lock);
        }
        return 0;
    }
    
    // No hay frames libres: evictar usando FIFO
    if (allocator->fifo_head == NULL) {
        // No hay páginas para evictar (error)
        if (!allocator->unsafe) {
            pthread_mutex_unlock(&allocator->lock);
        }
        return -1;
    }
    
    // Evictar la página más antigua (head de FIFO)
    page_info_t *victim = allocator->fifo_head;
    allocator->fifo_head = victim->next;
    if (allocator->fifo_head == NULL) {
        allocator->fifo_tail = NULL;
    }
    
    // Retornar información de la víctima para que el caller invalide la page table
    *evicted_page = victim;
    *frame_number = victim->frame_number;
    
    // Agregar la nueva página a la cola FIFO
    add_to_fifo(allocator, thread_id, vpn, *frame_number);
    
    if (!allocator->unsafe) {
        pthread_mutex_unlock(&allocator->lock);
    }
    
    return 1;  // Indica que hubo eviction
}

/*
 * Dominio: Liberación de frames
 * Recorrido: Marca frame como libre en el bitmap, incrementa contador
 * Descripción: Libera un frame previamente asignado, permitiendo su reutilización
 *              para futuras asignaciones.
 */
void free_frame(frame_allocator_t *allocator, uint64_t frame_number) {
    if (!allocator->unsafe) {
        pthread_mutex_lock(&allocator->lock);
    }
    
    if (frame_number < (uint64_t)allocator->num_frames) {
        allocator->free_frames[frame_number] = 1;
        allocator->free_count++;
    }
    
    if (!allocator->unsafe) {
        pthread_mutex_unlock(&allocator->lock);
    }
}
