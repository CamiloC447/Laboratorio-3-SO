# Simulador de Memoria Virtual - Laboratorio 3
# Integrantes: Camilo Cuero 24637967-K
#              Matías Rojas 20980316-K

Simulador concurrente de memoria virtual que implementa segmentación y paginación usando pthreads.

## Requisitos

- Compilador C11 (gcc)
- POSIX Threads (pthreads)
- Sistema Linux/Unix

## Intrucciones de Compilación

```bash
make
```
Otros targets disponibles : `make clean`: Limpiar archivos generados y `make all`.

## Intrucciones de ejecucion
- Para ejecutar un ejemplo por defecto (configuración básica de paginación), utilice:

```bash
make run
```
- Para ejecución manual con parámetros específicos:

```bash
./simulator --mode {seg|page} [opciones]
```
## Instrucciones de reproducción

``bash
make reproduce
``

Esto ejecuta:
1. Experimento 1: Segmentación con detección de segfaults
2. Experimento 2: Impacto del TLB (con y sin TLB)
3. Experimento 3: Thrashing con múltiples threads

Los resultados se guardan en `out/exp*.json´

Cada ejecución genera `out/summary.json` con la configuración y metricas.

## Opciones Comunes

- `--mode {seg|page}`: Modo de operación (obligatorio)
- `--threads N`: Número de threads (default: 1)
- `--ops-per-thread N`: Operaciones por thread (default: 1000)
- `--workload {uniform|80-20}`: Patrón de acceso (default: uniform)
- `--seed N`: Semilla para generador aleatorio (default: 42)
- `--unsafe`: Desactivar locks (modo UNSAFE)
- `--stats`: Mostrar estadísticas detalladas

### Opciones de Segmentación

- `--segments N`: Número de segmentos (default: 4)
- `--seg-limits CSV`: Límites por segmento separados por comas (ej: 1024,2048,4096,8192)

### Opciones de Paginacion

- `--pages N`: Páginas virtuales por thread (default: 64)
- `--frames N`: Frames físicos disponibles (default: 32)
- `--page-size N`: Tamaño de página en bytes (default: 4096)
- `--tlb-size N`: Tamaño de TLB (0 para deshabilitar, default: 16)

## Ejemplos

### Segmentación
```bash
./simulator --mode seg --threads 1 --workload uniform \
  --ops-per-thread 10000 --segments 4 \
  --seg-limits 1024,2048,4096,8192 --seed 100 --stats
```

### Paginación
```bash
./simulator --mode page --threads 4 --workload 80-20 \
  --ops-per-thread 50000 --pages 128 --frames 64 \
  --page-size 4096 --tlb-size 32 --seed 200 --stats
```

### Thrashing
```bash
./simulator --mode page --threads 8 --workload uniform \
  --ops-per-thread 10000 --pages 64 --frames 8 \
  --page-size 4096 --tlb-size 16 --seed 300 --stats
```

