/*
 * Test de Concurrencia
 * Verifica el comportamiento en modos SAFE y UNSAFE
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "../include/simulator.h"

#define NUM_THREADS 4
#define OPS_PER_THREAD 10000

typedef struct {
    metrics_t *metrics;
    int unsafe;
} test_context_t;

void* increment_thread(void *arg) {
    test_context_t *ctx = (test_context_t*)arg;
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        if (!ctx->unsafe) {
            pthread_mutex_lock(&ctx->metrics->lock);
        }
        ctx->metrics->total_operations++;
        if (!ctx->unsafe) {
            pthread_mutex_unlock(&ctx->metrics->lock);
        }
    }
    
    return NULL;
}

void test_safe_mode() {
    printf("Modo SAFE (con locks)...\n");
    
    metrics_t metrics = {0};
    pthread_mutex_init(&metrics.lock, NULL);
    
    test_context_t ctx = {.metrics = &metrics, .unsafe = 0};
    
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_thread, &ctx);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    uint64_t expected = NUM_THREADS * OPS_PER_THREAD;
    printf("  Esperado: %lu\n", expected);
    printf("  Obtenido: %lu\n", metrics.total_operations);
    
    assert(metrics.total_operations == expected);
    printf("Contadores correctos con locks\n");
    
    pthread_mutex_destroy(&metrics.lock);
    printf("Test Pasado\n\n");
}

void test_unsafe_mode() {
    printf("Modo UNSAFE (sin locks)...\n");
    printf("Este test podria mostrar condiciones de carrera\n");
    
    metrics_t metrics = {0};
    test_context_t ctx = {.metrics = &metrics, .unsafe = 1};
    
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_thread, &ctx);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    uint64_t expected = NUM_THREADS * OPS_PER_THREAD;
    printf(" Esperado: %lu\n", expected);
    printf(" Obtenido: %lu\n", metrics.total_operations);
    
    if (metrics.total_operations < expected) {
        printf(" Condición de carrera detectada (resultado incorrecto)\n");
        printf(" Esto es esperado en modo UNSAFE\n");
    } else {
        printf(" No se detectó condición de carrera en esta ejecución\n");
        printf(" Esto puede variar según el timing y número de cores\n");
    }
    
    printf("Test 2: PASSED\n\n");
}

void test_multiple_runs() {
    printf("Test 3: Múltiples ejecuciones en modo UNSAFE...\n");
    
    int different_results = 0;
    uint64_t last_result = 0;
    uint64_t expected = NUM_THREADS * OPS_PER_THREAD;
    
    for (int run = 0; run < 5; run++) {
        metrics_t metrics = {0};
        test_context_t ctx = {.metrics = &metrics, .unsafe = 1};
        
        pthread_t threads[NUM_THREADS];
        
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, increment_thread, &ctx);
        }
        
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        printf("  Run %d: %lu ", run + 1, metrics.total_operations);
        
        if (metrics.total_operations < expected) {
            printf("(carrera detectada)\n");
            different_results++;
        } else {
            printf("(correcto por suerte)\n");
        }
        
        if (run > 0 && metrics.total_operations != last_result) {
            different_results++;
        }
        
        last_result = metrics.total_operations;
    }
    
    printf("  ℹ En %d/5 ejecuciones se observaron inconsistencias\n", different_results);
    printf("Test 3: PASSED\n\n");
}

int main() {
    printf("========================================\n");
    printf("TESTS DE CONCURRENCIA\n");
    printf("========================================\n\n");
    
    test_safe_mode();
    test_unsafe_mode();
    test_multiple_runs();
    
    printf("========================================\n");
    printf("TODOS LOS TESTS COMPLETADOS\n");
    printf("========================================\n");
    
    return 0;
}
