/*
 * Test de Segmentación
 * Verifica la correcta traducción de direcciones y detección de segfaults
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/segmentacion.h"
#include "../include/simulator.h"

void test_basic_translation() {
    printf("Traducción básica de direcciones...\n");
    
    uint64_t limits[] = {4096, 8192, 2048, 16384};
    segment_table_t *table = create_segment_table(4, limits);
    
    config_t config = {0};
    config.unsafe = 1;  // Sin locks para testing
    
    metrics_t metrics = {0};
    
    // Test: acceso válido al segmento 0
    virtual_address_seg_t va = {.seg_id = 0, .offset = 100};
    uint64_t pa;
    int result = translate_address_seg(table, va, &pa, &metrics, &config);
    
    assert(result == 0);  // Debe ser exitoso
    assert(metrics.translations_ok == 1);
    printf("  ✓ Traducción válida exitosa\n");
    
    // Test: acceso fuera del límite
    va.seg_id = 0;
    va.offset = 5000;  // Excede límite de 4096
    result = translate_address_seg(table, va, &pa, &metrics, &config);
    
    assert(result == -1);  // Debe fallar
    assert(metrics.segfaults == 1);
    printf("  ✓ Segfault detectado correctamente\n");
    
    destroy_segment_table(table);
    printf("Test pasado\n\n");
}

void test_all_segments() {
    printf("Acceso a todos los segmentos...\n");
    
    uint64_t limits[] = {1024, 2048, 4096, 8192};
    segment_table_t *table = create_segment_table(4, limits);
    
    config_t config = {0};
    config.unsafe = 1;
    metrics_t metrics = {0};
    
    for (int i = 0; i < 4; i++) {
        virtual_address_seg_t va = {.seg_id = i, .offset = 0};
        uint64_t pa;
        int result = translate_address_seg(table, va, &pa, &metrics, &config);
        assert(result == 0);
    }
    
    assert(metrics.translations_ok == 4);
    printf(" Acceso a todos los segmentos exitoso\n");
    
    destroy_segment_table(table);
    printf("Test pasado\n\n");
}

int main() {
    printf("========================================\n");
    printf("TESTS DE SEGMENTACIÓN\n");
    printf("========================================\n\n");
    
    test_basic_translation();
    test_all_segments();
    
    printf("========================================\n");
    printf("TODOS LOS TESTS PASARON\n");
    printf("========================================\n");
    
    return 0;
}
