/*
 * Dominio: Gestión de memoria virtual - Paginación bajo demanda
 * Recorrido: Traducción de direcciones virtuales a físicas mediante tablas de páginas,
 *            manejo de TLB y fallos de página
 * Descripción: Módulo que implementa el esquema de paginación con búsqueda en TLB,
 *              traducción de direcciones y gestión de fallos de página.
 */

#ifndef PAGINACION_H
#define PAGINACION_H

#include <stdint.h>
#include "simulator.h"

#define INVALID_FRAME 0xFFFFFFFFFFFFFFFF

/* Entrada de la tabla de páginas */
typedef struct {
    uint64_t frame_number;  // Número del frame físico
    int valid;              // 1 si está en memoria, 0 si no
    int referenced;         // Bit de referencia (opcional)
    int dirty;              // Bit de modificación (bonus)
} page_table_entry_t;

/* Tabla de páginas por thread */
typedef struct {
    page_table_entry_t *entries;
    int num_pages;
} page_table_t;

/* Dirección virtual en modo paginación */
typedef struct {
    uint64_t vpn;    // Virtual Page Number
    uint64_t offset; // Offset dentro de la página
} virtual_address_page_t;

/* Funciones de paginación */
page_table_t* create_page_table(int num_pages);
void destroy_page_table(page_table_t *table);

virtual_address_page_t decompose_virtual_address(uint64_t va, int page_size);
uint64_t compose_physical_address(uint64_t frame_number, uint64_t offset, int page_size);

int translate_address_page(page_table_t *table, virtual_address_page_t va,
                           uint64_t *pa, void *tlb, void *frame_allocator,
                           metrics_t *metrics, config_t *config, int thread_id);

void* paging_thread(void *arg);

#endif /* PAGINACION_H */
