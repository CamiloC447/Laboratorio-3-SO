#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tlb.h"

/*
 * Dominio: Caché de traducción Inicialización TLB
 * Recorrido: Aloca estructura, inicializa array circular, marca entradas inválidas
 * Descripción: Crea una TLB (Translation Lookaside Buffer) como caché circular
 *              FIFO para traducciones VPN->Frame.
 */
tlb_t* create_tlb(int size) {
    if (size <= 0) {
        return NULL;  // TLB deshabilitada
    }
    
    tlb_t *tlb = malloc(sizeof(tlb_t));
    tlb->size = size;
    tlb->entries = malloc(sizeof(tlb_entry_t) * size);
    tlb->next_index = 0;
    tlb->count = 0;
    
    // Inicializar todas las entradas como inválidas
    for (int i = 0; i < size; i++) {
        tlb->entries[i].valid = 0;
        tlb->entries[i].vpn = 0;
        tlb->entries[i].frame_number = 0;
    }
    
    return tlb;
}

/*
 * Dominio: Caché de traducción - Limpieza TLB
 * Recorrido: Libera array de entradas y estructura
 * Descripción: Libera los recursos asociados a una TLB.
 */
void destroy_tlb(tlb_t *tlb) {
    if (tlb) {
        free(tlb->entries);
        free(tlb);
    }
}

/*
 * Dominio: Caché de traducción - Búsqueda
 * Recorrido: Búsqueda lineal por VPN, retorna frame si es hit, 0 si miss
 * Descripción: Consulta la TLB buscando una traducción para un VPN específico,
 *              retornando el frame correspondiente si hay hit.
 */
int tlb_lookup(tlb_t *tlb, uint64_t vpn, uint64_t *frame_number) {
    if (!tlb) {
        return 0;  // TLB deshabilitada, siempre miss
    }
    
    // Búsqueda lineal en la TLB
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            *frame_number = tlb->entries[i].frame_number;
            return 1;  // TLB hit
        }
    }
    
    return 0;  // TLB miss
}

/*
 * Dominio: Caché de traducción - Inserción
 * Recorrido: Busca si existe (actualiza), sino inserta en posición FIFO circular
 * Descripción: Inserta o actualiza una entrada en la TLB usando política FIFO
 *              con buffer circular, reemplazando la entrada más antigua si está llena.
 */
void tlb_insert(tlb_t *tlb, uint64_t vpn, uint64_t frame_number) {
    if (!tlb) {
        return;  // TLB deshabilitada
    }
    
    // Buscar si ya existe (actualizar)
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            tlb->entries[i].frame_number = frame_number;
            return;
        }
    }
    
    // Insertar nueva entrada en posición FIFO
    tlb->entries[tlb->next_index].vpn = vpn;
    tlb->entries[tlb->next_index].frame_number = frame_number;
    tlb->entries[tlb->next_index].valid = 1;
    
    // Avanzar índice circular
    tlb->next_index = (tlb->next_index + 1) % tlb->size;
    
    // Incrementar count si aún no está llena
    if (tlb->count < tlb->size) {
        tlb->count++;
    }
}

/*
 * Dominio: Caché de traducción Invalidación selectiva
 * Recorrido: Busca entrada con VPN, marca como inválida, decrementa contador
 * Descripción: Invalida una entrada específica de la TLB por su VPN, necesaria
 *              cuando una página es evictada.
 */
void tlb_invalidate(tlb_t *tlb, uint64_t vpn) {
    if (!tlb) {
        return;
    }
    
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            tlb->entries[i].valid = 0;
            tlb->count--;
            return;
        }
    }
}

/**
 * Dominio: Caché de traducción Invalidación completa
 * Recorrido: Marca todas las entradas como inválidas, resetea índice y contador
 * Descripción: Limpia completamente la TLB invalidando todas sus entradas,
 *              útil cuando hay cambio de contexto.
 */
void tlb_clear(tlb_t *tlb) {
    if (!tlb) {
        return;
    }
    
    for (int i = 0; i < tlb->size; i++) {
        tlb->entries[i].valid = 0;
    }
    tlb->next_index = 0;
    tlb->count = 0;
}
