/*
 * Dominio: Gestión de memoria virtual - Segmentación
 * Recorrido: Traducción de direcciones virtuales con validación de límites de segmento
 * Descripción: Módulo que implementa el esquema de segmentación con tablas de segmentos,
 *              traducción de direcciones y detección de violations de límites.
 */

#ifndef SEGMENTACION_H
#define SEGMENTACION_H

#include <stdint.h>
#include "simulator.h"

/* Entrada de la tabla de segmentos */
typedef struct {
    uint64_t base;   // Dirección física base del segmento
    uint64_t limit;  // Límite del segmento en bytes
} segment_entry_t;

/* Tabla de segmentos por thread */
typedef struct {
    segment_entry_t *segments;
    int num_segments;
} segment_table_t;

/* Dirección virtual en modo segmentación */
typedef struct {
    int seg_id;      // ID del segmento
    uint64_t offset; // Offset dentro del segmento
} virtual_address_seg_t;

/* Funciones de segmentación */
segment_table_t* create_segment_table(int num_segments, uint64_t *limits);
void destroy_segment_table(segment_table_t *table);

int translate_address_seg(segment_table_t *table, virtual_address_seg_t va, 
                          uint64_t *pa, metrics_t *metrics, config_t *config);

void* segmentation_thread(void *arg);

#endif /* SEGMENTACION_H */
