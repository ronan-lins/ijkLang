#ifndef IJKL_MEMORY_H
#define IJKL_MEMORY_H

#include "common.h"

/*
 * ==============================================================================
 * FUNDAMENTO ACADÊMICO: GERENCIAMENTO DE MEMÓRIA BASEADO EM REGIÕES (ARENA)
 * ==============================================================================
 * Na literatura acadêmica de Ciência da Computação e Engenharia de Compiladores
 * (Tofte & Talpin 1994; Hanson 1997; Aho, Lam, Sethi & Ullman, "Dragon Book"),
 * esta técnica é formalmente denominada "Region-Based Memory Management" e,
 * na prática de construção de compiladores (como em lcc e LLVM), é consagrada
 * pelo termo "Arena" ou "Memory Pool".
 *
 * Princípio de Funcionamento:
 * 1. Em vez de realizar centenas de milhares de pequenas chamadas individuais a
 *    malloc() e free() — o que causa fragmentação do heap e sobrecarga de CPU —,
 *    a Arena reserva grandes blocos contíguos de memória (chunks, e.g. 8 KB).
 * 2. Novas requisições de alocação de nós sintáticos (AST), strings e tokens
 *    apenas incrementam um ponteiro interno de deslocamento no bloco atual (O(1)).
 * 3. O ciclo de vida desses objetos é vinculado à fase de compilação: ao término
 *    da etapa, toda a memória da Arena é liberada instantaneamente em tempo O(1),
 *    eliminando qualquer risco de vazamento de memória (memory leaks).
 * ==============================================================================
 */

/* Bloco contíguo individual de memória que compõe a lista encadeada da Arena */
typedef struct IjklMemoryChunk {
    size_t bytes_capacity;
    size_t bytes_allocated;
    struct IjklMemoryChunk* next_chunk;
    /* Início dos bytes de dados do bloco na memória contígua */
    uint8_t data_bytes[1];
} IjklMemoryChunk;

/* Estrutura principal da Arena de Memória */
typedef struct IjklMemoryArena {
    IjklMemoryChunk* first_chunk;
    IjklMemoryChunk* current_chunk;
    size_t default_chunk_capacity;
    size_t total_allocated_bytes;
} IjklMemoryArena;

/* Criação e destruição do ciclo de vida da Arena */
IjklMemoryArena* ijkl_memory_arena_create(size_t default_chunk_capacity);
void ijkl_memory_arena_destroy(IjklMemoryArena* memory_arena);

/* Alocação linear dentro da Arena */
void* ijkl_memory_arena_allocate(IjklMemoryArena* memory_arena, size_t allocation_size_in_bytes);
char* ijkl_memory_arena_duplicate_string(IjklMemoryArena* memory_arena, const char* source_string);
char* ijkl_memory_arena_duplicate_string_slice(IjklMemoryArena* memory_arena, const char* source_string, size_t slice_length);

/* Alocação direta do Heap do sistema com verificação de ponteiro nulo */
void* ijkl_memory_heap_allocate(size_t allocation_size_in_bytes);
void* ijkl_memory_heap_allocate_zeroed(size_t element_count, size_t element_size_in_bytes);
void* ijkl_memory_heap_reallocate(void* memory_block, size_t new_size_in_bytes);
void ijkl_memory_heap_free(void* memory_block);

#endif /* IJKL_MEMORY_H */
