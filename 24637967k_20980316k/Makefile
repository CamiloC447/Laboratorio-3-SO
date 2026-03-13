# Makefile para Simulador de Memoria Virtual

CC = gcc
CFLAGS = -std=c11 -pthread -Wall -Wextra -I./include
LDFLAGS = -pthread -lm
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
OUT_DIR = out

# Archivos fuente
SOURCES = $(SRC_DIR)/simulator.c \
          $(SRC_DIR)/segmentacion.c \
          $(SRC_DIR)/paginacion.c \
          $(SRC_DIR)/tlb.c \
          $(SRC_DIR)/frame_allocator.c \
          $(SRC_DIR)/workloads.c

# Archivos objeto
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Ejecutable
TARGET = simulator

.PHONY: all clean run reproduce test

# Target principal
all: $(TARGET)

# Crear ejecutable
$(TARGET): $(OBJECTS) | $(OUT_DIR)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Compilación exitosa: ./$(TARGET)"

# Compilar archivos objeto
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Crear directorios necesarios
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Ejecutar simulador con parámetros por defecto
run: $(TARGET)
	@echo "=========================================="
	@echo "Ejecutando Simulador de Memoria Virtual"
	@echo "=========================================="
	./$(TARGET) --mode page --threads 2 --workload uniform \
		--ops-per-thread 10000 --pages 64 --frames 32 \
		--page-size 4096 --tlb-size 16 --seed 42 --stats
	@echo ""
	@echo "Resultados guardados en: out/summary.json"
	@echo "=========================================="

# Reproducir los 3 experimentos
reproduce: $(TARGET)
	@echo "=========================================="
	@echo "EXPERIMENTO 1: Segmentación con Segfaults"
	@echo "=========================================="
	./$(TARGET) --mode seg --threads 1 --workload uniform \
		--ops-per-thread 10000 --segments 4 \
		--seg-limits 1024,2048,4096,8192 --seed 100 --stats
	@mv out/summary.json out/exp1_segmentation.json
	@echo ""
	@echo "=========================================="
	@echo "EXPERIMENTO 2a: Paginación SIN TLB"
	@echo "=========================================="
	./$(TARGET) --mode page --threads 1 --workload 80-20 \
		--ops-per-thread 50000 --pages 128 --frames 64 \
		--page-size 4096 --tlb-size 0 --seed 200 --stats
	@mv out/summary.json out/exp2a_no_tlb.json
	@echo ""
	@echo "=========================================="
	@echo "EXPERIMENTO 2b: Paginación CON TLB"
	@echo "=========================================="
	./$(TARGET) --mode page --threads 1 --workload 80-20 \
		--ops-per-thread 50000 --pages 128 --frames 64 \
		--page-size 4096 --tlb-size 32 --seed 200 --stats
	@mv out/summary.json out/exp2b_with_tlb.json
	@echo ""
	@echo "=========================================="
	@echo "EXPERIMENTO 3a: Thrashing con 1 Thread"
	@echo "=========================================="
	./$(TARGET) --mode page --threads 1 --workload uniform \
		--ops-per-thread 10000 --pages 64 --frames 8 \
		--page-size 4096 --tlb-size 16 --seed 300 --stats
	@mv out/summary.json out/exp3a_1thread.json
	@echo ""
	@echo "=========================================="
	@echo "EXPERIMENTO 3b: Thrashing con 8 Threads"
	@echo "=========================================="
	./$(TARGET) --mode page --threads 8 --workload uniform \
		--ops-per-thread 10000 --pages 64 --frames 8 \
		--page-size 4096 --tlb-size 16 --seed 300 --stats
	@mv out/summary.json out/exp3b_8threads.json
	@echo ""
	@echo "=========================================="
	@echo "Todos los experimentos completados"
	@echo "Resultados guardados en la carpeta out/"
	@echo "=========================================="

# Test de concurrencia SAFE vs UNSAFE
test: $(TARGET)
	@echo "Test 1: Modo SAFE (con locks)"
	./$(TARGET) --mode page --threads 4 --ops-per-thread 5000 \
		--workload uniform --frames 16 --stats
	@mv out/summary.json out/test_safe.json
	@echo ""
	@echo "Test 2: Modo UNSAFE (sin locks)"
	./$(TARGET) --mode page --threads 4 --ops-per-thread 5000 \
		--workload uniform --frames 16 --unsafe --stats
	@mv out/summary.json out/test_unsafe.json
	@echo ""
	@echo "Comparar test_safe.json vs test_unsafe.json en out/"

# Limpiar archivos generados
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	rm -f out/*.json
	@echo "Limpieza completada"