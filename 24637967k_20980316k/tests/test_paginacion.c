/*
 * Test de Paginación
 * Verifica TLB, tabla de páginas, page faults y eviction
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/paginacion.h"
#include "../include/tlb.h"
#include "../include/simulator.h"

void test_address_decomposition() {
    printf("Descomposición de direcciones...\n");
    
    int page_size = 4096;
    uint64_t va = 8192 + 256;  // VPN=2, offset=256
    
    virtual_address_page_t decomposed = decompose_virtual_address(va, page_size);
    
    assert(decomposed.vpn == 2);
    assert(decomposed.offset == 256);
    printf(" Descomposición correcta: VPN=%lu, offset=%lu\n", 
           decomposed.vpn, decomposed.offset);
    
    // Test composición
    uint64_t pa = compose_physical_address(5, 256, page_size);
    assert(pa == 5 * 4096 + 256);
    printf(" Composición correcta: PA=%lu\n", pa);
    
    printf("Test pasado\n\n");
}

void test_tlb_operations() {
    printf("Operaciones de TLB...\n");
    
    tlb_t *tlb = create_tlb(4);
    
    // Test: inserción
    tlb_insert(tlb, 10, 5);
    tlb_insert(tlb, 20, 8);
    
    // Test: lookup exitoso
    uint64_t frame;
    int found = tlb_lookup(tlb, 10, &frame);
    assert(found == 1);
    assert(frame == 5);
    printf(" TLB lookup exitoso\n");
    
    // Test: lookup fallido
    found = tlb_lookup(tlb, 99, &frame);
    assert(found == 0);
    printf("TLB miss detectado correctamente\n");
    
    // Test: FIFO (llenar más allá de la capacidad)
    tlb_insert(tlb, 30, 12);
    tlb_insert(tlb, 40, 15);
    tlb_insert(tlb, 50, 18);  // Debe expulsar la primera entrada (VPN=10)
    
    found = tlb_lookup(tlb, 10, &frame);
    assert(found == 0);  // Ya no debe estar
    printf(" FIFO funcionando correctamente\n");
    
    destroy_tlb(tlb);
    printf("Test pasado\n\n");
}

void test_page_table() {
    printf("Tabla de páginas...\n");
    
    page_table_t *table = create_page_table(16);
    
    // Todas las páginas deben estar inicialmente inválidas
    for (int i = 0; i < 16; i++) {
        assert(table->entries[i].valid == 0);
    }
    printf("Inicialización correcta\n");
    
    // Marcar una página como válida
    table->entries[5].frame_number = 10;
    table->entries[5].valid = 1;
    
    assert(table->entries[5].valid == 1);
    assert(table->entries[5].frame_number == 10);
    printf(" Actualización de entrada exitosa\n");
    
    destroy_page_table(table);
    printf("Test  pasado\n\n");
}

void test_tlb_disabled() {
    printf("TLB deshabilitada (size=0)...\n");
    
    tlb_t *tlb = create_tlb(0);
    assert(tlb == NULL);
    
    uint64_t frame;
    int found = tlb_lookup(NULL, 10, &frame);
    assert(found == 0);  // Siempre miss con TLB deshabilitada
    printf(" TLB deshabilitada funciona correctamente\n");
    
    printf("Test pasado\n\n");
}

int main() {
    printf("========================================\n");
    printf("TESTS DE PAGINACIÓN\n");
    printf("========================================\n\n");
    
    test_address_decomposition();
    test_tlb_operations();
    test_page_table();
    test_tlb_disabled();
    
    printf("========================================\n");
    printf("TODOS LOS TESTS PASARON\n");
    printf("========================================\n");
    
    return 0;
}
