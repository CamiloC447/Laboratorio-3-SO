/*
 * Dominio: Caché de traducción - Translation Lookaside Buffer
 * Recorrido: Búsqueda rápida de traducciones VPN->Frame, inserción e invalidación
 * Descripción: Módulo que implementa la TLB como caché circular FIFO para acelerar
 *              la traducción de direcciones virtuales a físicas.
 */

#ifndef TLB_H
#define TLB_H

#include <stdint.h>

/* Entrada de la TLB */
typedef struct {
    uint64_t vpn;          // Virtual Page Number
    uint64_t frame_number; // Frame físico correspondiente
    int valid;             // 1 si la entrada es válida, 0 si está vacía
} tlb_entry_t;

/* TLB (Translation Lookaside Buffer) */
typedef struct {
    tlb_entry_t *entries;  // Array circular para FIFO
    int size;              // Tamaño máximo de la TLB
    int next_index;        // Índice para la próxima inserción (FIFO)
    int count;             // Número de entradas válidas actuales
} tlb_t;

/* Funciones de la TLB */
tlb_t* create_tlb(int size);
void destroy_tlb(tlb_t *tlb);

int tlb_lookup(tlb_t *tlb, uint64_t vpn, uint64_t *frame_number);
void tlb_insert(tlb_t *tlb, uint64_t vpn, uint64_t frame_number);
void tlb_invalidate(tlb_t *tlb, uint64_t vpn);
void tlb_clear(tlb_t *tlb);

#endif /* TLB_H */
