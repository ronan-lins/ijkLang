#include "memory.h"

/* Alinhamento de memória para o tamanho da palavra da máquina (8 bytes em arquiteturas de 64 bits) */
#define IJKL_MEMORY_ALIGNMENT sizeof(void*)

static inline size_t align_size_to_machine_word(size_t size_in_bytes) {
    return (size_in_bytes + (IJKL_MEMORY_ALIGNMENT - 1)) & ~(IJKL_MEMORY_ALIGNMENT - 1);
}

void* ijkl_memory_heap_allocate(size_t allocation_size_in_bytes) {
    void* allocated_pointer = malloc(allocation_size_in_bytes);
    if (!allocated_pointer && allocation_size_in_bytes > 0) {
        fprintf(stderr, "Fatal error: memory heap allocation failure (%zu bytes requested).\n", allocation_size_in_bytes);
        exit(EXIT_FAILURE);
    }
    return allocated_pointer;
}

void* ijkl_memory_heap_allocate_zeroed(size_t element_count, size_t element_size_in_bytes) {
    void* allocated_pointer = calloc(element_count, element_size_in_bytes);
    if (!allocated_pointer && element_count > 0 && element_size_in_bytes > 0) {
        fprintf(stderr, "Fatal error: zeroed memory heap allocation failure.\n");
        exit(EXIT_FAILURE);
    }
    return allocated_pointer;
}

void* ijkl_memory_heap_reallocate(void* memory_block, size_t new_size_in_bytes) {
    void* reallocated_pointer = realloc(memory_block, new_size_in_bytes);
    if (!reallocated_pointer && new_size_in_bytes > 0) {
        fprintf(stderr, "Fatal error: memory heap reallocation failure (%zu bytes requested).\n", new_size_in_bytes);
        exit(EXIT_FAILURE);
    }
    return reallocated_pointer;
}

void ijkl_memory_heap_free(void* memory_block) {
    if (memory_block) {
        free(memory_block);
    }
}

IjklMemoryArena* ijkl_memory_arena_create(size_t default_chunk_capacity) {
    IjklMemoryArena* memory_arena = (IjklMemoryArena*)ijkl_memory_heap_allocate(sizeof(IjklMemoryArena));
    memory_arena->first_chunk = NULL;
    memory_arena->current_chunk = NULL;
    memory_arena->default_chunk_capacity = (default_chunk_capacity > 0) ? default_chunk_capacity : 8192;
    memory_arena->total_allocated_bytes = 0;
    return memory_arena;
}

void ijkl_memory_arena_destroy(IjklMemoryArena* memory_arena) {
    if (!memory_arena) return;

    /* Desaloca todos os blocos encadeados em um único laço O(N) */
    IjklMemoryChunk* current_chunk_iterator = memory_arena->first_chunk;
    while (current_chunk_iterator) {
        IjklMemoryChunk* next_chunk_to_free = current_chunk_iterator->next_chunk;
        ijkl_memory_heap_free(current_chunk_iterator);
        current_chunk_iterator = next_chunk_to_free;
    }

    ijkl_memory_heap_free(memory_arena);
}

void* ijkl_memory_arena_allocate(IjklMemoryArena* memory_arena, size_t allocation_size_in_bytes) {
    if (!memory_arena || allocation_size_in_bytes == 0) return NULL;

    size_t aligned_size = align_size_to_machine_word(allocation_size_in_bytes);

    /* Verifica se há espaço disponível no bloco atual; caso contrário, aloca um novo bloco */
    if (!memory_arena->current_chunk ||
        (memory_arena->current_chunk->bytes_allocated + aligned_size > memory_arena->current_chunk->bytes_capacity)) {

        /* Se a alocação solicitada for maior que a capacidade padrão, cria bloco sob medida */
        size_t chunk_capacity_to_use = memory_arena->default_chunk_capacity;
        if (aligned_size > chunk_capacity_to_use) {
            chunk_capacity_to_use = aligned_size;
        }

        size_t total_chunk_allocation_size = sizeof(IjklMemoryChunk) + chunk_capacity_to_use;
        IjklMemoryChunk* new_chunk = (IjklMemoryChunk*)ijkl_memory_heap_allocate(total_chunk_allocation_size);
        new_chunk->bytes_capacity = chunk_capacity_to_use;
        new_chunk->bytes_allocated = 0;
        new_chunk->next_chunk = NULL;

        if (!memory_arena->first_chunk) {
            memory_arena->first_chunk = new_chunk;
        } else {
            memory_arena->current_chunk->next_chunk = new_chunk;
        }
        memory_arena->current_chunk = new_chunk;
    }

    void* allocated_memory_address = &memory_arena->current_chunk->data_bytes[memory_arena->current_chunk->bytes_allocated];
    memory_arena->current_chunk->bytes_allocated += aligned_size;
    memory_arena->total_allocated_bytes += aligned_size;

    return allocated_memory_address;
}

char* ijkl_memory_arena_duplicate_string_slice(IjklMemoryArena* memory_arena, const char* source_string, size_t slice_length) {
    if (!source_string) return NULL;
    char* duplicated_string = (char*)ijkl_memory_arena_allocate(memory_arena, slice_length + 1);
    memcpy(duplicated_string, source_string, slice_length);
    duplicated_string[slice_length] = '\0';
    return duplicated_string;
}

char* ijkl_memory_arena_duplicate_string(IjklMemoryArena* memory_arena, const char* source_string) {
    if (!source_string) return NULL;
    return ijkl_memory_arena_duplicate_string_slice(memory_arena, source_string, strlen(source_string));
}
