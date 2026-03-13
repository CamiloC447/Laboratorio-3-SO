/*
 * Dominio: Generación de cargas de trabajo sintéticas
 * Recorrido: Generación de direcciones virtuales con distintos patrones de acceso
 * Descripción: Módulo que genera cargas de trabajo (direcciones virtuales) con patrones
 *              uniform y 80-20 para simular accesos a memoria de múltiples threads.
 */

#ifndef WORKLOADS_H
#define WORKLOADS_H

#include <stdint.h>
#include "simulator.h"
#include "segmentacion.h"
#include "paginacion.h"

/* Inicializa el generador aleatorio para un thread específico */
void init_thread_random(unsigned int base_seed, int thread_id);

/* Genera dirección virtual para modo segmentación */
virtual_address_seg_t generate_address_segmentation(config_t *config, workload_t workload);

/* Genera dirección virtual para modo paginación */
virtual_address_page_t generate_address_paging(config_t *config, workload_t workload);

/* Función auxiliar para generar números aleatorios thread-safe */
int random_range(int min, int max);

#endif /* WORKLOADS_H */
