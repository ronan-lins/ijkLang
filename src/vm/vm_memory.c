#include <stdlib.h>
#include <string.h>
#include "vm/vm_memory.h"

/*
 * ==============================================================================
 * MÓDULO: IMPLEMENTAÇÃO DO SUBSISTEMA DE MEMÓRIA LINEAR (IJKLANG VM)
 * ==============================================================================
 * Gerencia o espaço contíguo de 64 bits da VM, dividindo-o em:
 * - Página nula protegida (0x0000 - 0x0FFF)
 * - Dados globais estáticos (0x1000 - 0x0FFFFF)
 * - Heap dinâmico com alocação alinhada a 8 bytes (0x100000 - 0x7FFFFF)
 * - Pilha de execução e quadros de ativação (0x800000 - 0xFFFFFF)
 * ==============================================================================
 */

/* Alinhamento de endereços para palavras de 64 bits (8 bytes) */
static inline uint64_t align_address_to_8_bytes(uint64_t address) {
    return (address + 7ULL) & ~7ULL;
}

bool ijkl_vm_memory_initialize(IjklVirtualMemory* virtual_memory, size_t total_capacity_bytes) {
    if (!virtual_memory) return false;

    if (total_capacity_bytes < IJKL_VM_MEMORY_TOTAL_CAPACITY_BYTES) {
        total_capacity_bytes = IJKL_VM_MEMORY_TOTAL_CAPACITY_BYTES;
    }

    virtual_memory->raw_linear_memory_buffer = (uint8_t*)calloc(1, total_capacity_bytes);
    if (!virtual_memory->raw_linear_memory_buffer) {
        return false;
    }

    size_t words_count = total_capacity_bytes / sizeof(int64_t);
    virtual_memory->memory_type_tags_buffer = (uint8_t*)calloc(1, words_count);
    if (!virtual_memory->memory_type_tags_buffer) {
        free(virtual_memory->raw_linear_memory_buffer);
        virtual_memory->raw_linear_memory_buffer = NULL;
        return false;
    }

    virtual_memory->total_allocated_capacity_bytes = total_capacity_bytes;
    virtual_memory->stack_base_address = IJKL_VM_STACK_BASE_ADDRESS;
    virtual_memory->current_stack_pointer = IJKL_VM_STACK_BASE_ADDRESS;
    virtual_memory->current_frame_pointer = IJKL_VM_STACK_BASE_ADDRESS;

    virtual_memory->heap_next_free_address = IJKL_VM_HEAP_BASE_ADDRESS;
    virtual_memory->heap_first_block = NULL;

    virtual_memory->total_heap_allocated_bytes = 0;
    virtual_memory->total_peak_stack_bytes = 0;

    return true;
}

void ijkl_vm_memory_destroy(IjklVirtualMemory* virtual_memory) {
    if (!virtual_memory) return;

    if (virtual_memory->raw_linear_memory_buffer) {
        free(virtual_memory->raw_linear_memory_buffer);
        virtual_memory->raw_linear_memory_buffer = NULL;
    }

    if (virtual_memory->memory_type_tags_buffer) {
        free(virtual_memory->memory_type_tags_buffer);
        virtual_memory->memory_type_tags_buffer = NULL;
    }

    IjklHeapBlockHeader* current_block = virtual_memory->heap_first_block;
    while (current_block) {
        IjklHeapBlockHeader* next = current_block->next_block;
        free(current_block);
        current_block = next;
    }
    virtual_memory->heap_first_block = NULL;
    virtual_memory->total_allocated_capacity_bytes = 0;
}

void ijkl_vm_memory_reset(IjklVirtualMemory* virtual_memory) {
    if (!virtual_memory || !virtual_memory->raw_linear_memory_buffer) return;

    memset(virtual_memory->raw_linear_memory_buffer, 0, virtual_memory->total_allocated_capacity_bytes);
    if (virtual_memory->memory_type_tags_buffer) {
        size_t words_count = virtual_memory->total_allocated_capacity_bytes / sizeof(int64_t);
        memset(virtual_memory->memory_type_tags_buffer, 0, words_count);
    }

    virtual_memory->current_stack_pointer = virtual_memory->stack_base_address;
    virtual_memory->current_frame_pointer = virtual_memory->stack_base_address;
    virtual_memory->heap_next_free_address = IJKL_VM_HEAP_BASE_ADDRESS;

    IjklHeapBlockHeader* current_block = virtual_memory->heap_first_block;
    while (current_block) {
        IjklHeapBlockHeader* next = current_block->next_block;
        free(current_block);
        current_block = next;
    }
    virtual_memory->heap_first_block = NULL;
    virtual_memory->total_heap_allocated_bytes = 0;
    virtual_memory->total_peak_stack_bytes = 0;
}

uint8_t ijkl_vm_memory_get_tag(const IjklVirtualMemory* virtual_memory, uint64_t memory_address) {
    if (!virtual_memory || !virtual_memory->memory_type_tags_buffer) return 0;
    size_t word_index = (size_t)(memory_address / sizeof(int64_t));
    size_t words_count = virtual_memory->total_allocated_capacity_bytes / sizeof(int64_t);
    if (word_index < words_count) {
        return virtual_memory->memory_type_tags_buffer[word_index];
    }
    return 0;
}

void ijkl_vm_memory_set_tag(IjklVirtualMemory* virtual_memory, uint64_t memory_address, uint8_t type_tag) {
    if (!virtual_memory || !virtual_memory->memory_type_tags_buffer) return;
    size_t word_index = (size_t)(memory_address / sizeof(int64_t));
    size_t words_count = virtual_memory->total_allocated_capacity_bytes / sizeof(int64_t);
    if (word_index < words_count) {
        virtual_memory->memory_type_tags_buffer[word_index] = type_tag;
    }
}

bool ijkl_vm_memory_is_valid_address(const IjklVirtualMemory* virtual_memory,
                                     uint64_t memory_address,
                                     size_t access_size_bytes) {
    if (!virtual_memory || !virtual_memory->raw_linear_memory_buffer) return false;

    /* Captura ponteiro nulo ou acesso à página 0 */
    if (memory_address < IJKL_VM_PAGE_NULL_END_ADDRESS) {
        return false;
    }

    /* Limite superior da memória física alocada */
    if (memory_address + access_size_bytes > virtual_memory->total_allocated_capacity_bytes) {
        return false;
    }

    return true;
}

bool ijkl_vm_memory_read_bytes(const IjklVirtualMemory* virtual_memory,
                               uint64_t memory_address,
                               void* destination_buffer,
                               size_t bytes_to_read) {
    if (!destination_buffer || !ijkl_vm_memory_is_valid_address(virtual_memory, memory_address, bytes_to_read)) {
        return false;
    }

    memcpy(destination_buffer, &virtual_memory->raw_linear_memory_buffer[memory_address], bytes_to_read);
    return true;
}

bool ijkl_vm_memory_write_bytes(IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                const void* source_buffer,
                                size_t bytes_to_write) {
    if (!source_buffer || !ijkl_vm_memory_is_valid_address(virtual_memory, memory_address, bytes_to_write)) {
        return false;
    }

    memcpy(&virtual_memory->raw_linear_memory_buffer[memory_address], source_buffer, bytes_to_write);
    return true;
}

bool ijkl_vm_memory_read_integer(const IjklVirtualMemory* virtual_memory,
                                 uint64_t memory_address,
                                 int64_t* output_integer_value) {
    if (!output_integer_value) return false;
    return ijkl_vm_memory_read_bytes(virtual_memory, memory_address, output_integer_value, sizeof(int64_t));
}

bool ijkl_vm_memory_write_integer(IjklVirtualMemory* virtual_memory,
                                  uint64_t memory_address,
                                  int64_t input_integer_value) {
    return ijkl_vm_memory_write_bytes(virtual_memory, memory_address, &input_integer_value, sizeof(int64_t));
}

bool ijkl_vm_memory_read_float(const IjklVirtualMemory* virtual_memory,
                               uint64_t memory_address,
                               double* output_float_value) {
    if (!output_float_value) return false;
    return ijkl_vm_memory_read_bytes(virtual_memory, memory_address, output_float_value, sizeof(double));
}

bool ijkl_vm_memory_write_float(IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                double input_float_value) {
    return ijkl_vm_memory_write_bytes(virtual_memory, memory_address, &input_float_value, sizeof(double));
}

bool ijkl_vm_memory_read_string(const IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                char* destination_buffer,
                                size_t maximum_buffer_length) {
    if (!destination_buffer || maximum_buffer_length == 0) return false;
    if (!ijkl_vm_memory_is_valid_address(virtual_memory, memory_address, 1)) return false;

    size_t char_index = 0;
    while (char_index < maximum_buffer_length - 1) {
        uint64_t current_address = memory_address + char_index;
        if (!ijkl_vm_memory_is_valid_address(virtual_memory, current_address, 1)) break;

        char c = (char)virtual_memory->raw_linear_memory_buffer[current_address];
        destination_buffer[char_index++] = c;
        if (c == '\0') break;
    }
    destination_buffer[char_index] = '\0';
    return true;
}

bool ijkl_vm_memory_write_string(IjklVirtualMemory* virtual_memory,
                                 uint64_t memory_address,
                                 const char* source_string) {
    if (!source_string) return false;
    size_t length = strlen(source_string) + 1;
    return ijkl_vm_memory_write_bytes(virtual_memory, memory_address, source_string, length);
}

/*
 * EFEITOS COLATERAIS:
 * Operações na Pilha de Avaliação (push / pop).
 */
bool ijkl_vm_memory_push_stack(IjklVirtualMemory* virtual_memory, int64_t value_to_push) {
    if (!virtual_memory) return false;

    if (!ijkl_vm_memory_write_integer(virtual_memory, virtual_memory->current_stack_pointer, value_to_push)) {
        return false;
    }

    virtual_memory->current_stack_pointer += sizeof(int64_t);

    size_t stack_used = (size_t)(virtual_memory->current_stack_pointer - virtual_memory->stack_base_address);
    if (stack_used > virtual_memory->total_peak_stack_bytes) {
        virtual_memory->total_peak_stack_bytes = stack_used;
    }

    return true;
}

bool ijkl_vm_memory_pop_stack(IjklVirtualMemory* virtual_memory, int64_t* output_popped_value) {
    if (!virtual_memory || !output_popped_value) return false;

    if (virtual_memory->current_stack_pointer < virtual_memory->stack_base_address + sizeof(int64_t)) {
        return false; /* Stack underflow */
    }

    virtual_memory->current_stack_pointer -= sizeof(int64_t);
    return ijkl_vm_memory_read_integer(virtual_memory, virtual_memory->current_stack_pointer, output_popped_value);
}

/*
 * EFEITO COLATERAL:
 * Aloca um quadro de ativação (alloc_frame) preservando o ra anterior e endereço de retorno.
 */
bool ijkl_vm_memory_alloc_frame(IjklVirtualMemory* virtual_memory,
                                size_t frame_local_variables_size_bytes,
                                uint64_t return_address_instruction_index) {
    if (!virtual_memory) return false;

    /* 1. Salva o endereço de retorno e o frame pointer anterior na pilha */
    if (!ijkl_vm_memory_push_stack(virtual_memory, (int64_t)return_address_instruction_index)) {
        return false;
    }
    if (!ijkl_vm_memory_push_stack(virtual_memory, (int64_t)virtual_memory->current_frame_pointer)) {
        return false;
    }

    /* 2. Define o novo frame pointer (ra) como o topo atual da pilha */
    virtual_memory->current_frame_pointer = virtual_memory->current_stack_pointer;

    /* 3. Aloca o espaço contíguo para as variáveis locais da rotina */
    size_t aligned_size = align_address_to_8_bytes(frame_local_variables_size_bytes);
    virtual_memory->current_stack_pointer += aligned_size;

    /* Zera a memória do novo quadro local */
    if (aligned_size > 0 &&
        virtual_memory->current_frame_pointer + aligned_size <= virtual_memory->total_allocated_capacity_bytes) {
        memset(&virtual_memory->raw_linear_memory_buffer[virtual_memory->current_frame_pointer], 0, aligned_size);
    }

    return true;
}

/*
 * EFEITO COLATERAL:
 * Libera o quadro de ativação atual (free_frame) e recupera o ra e endereço de retorno do chamador.
 */
bool ijkl_vm_memory_free_frame(IjklVirtualMemory* virtual_memory,
                               uint64_t* output_saved_return_address) {
    if (!virtual_memory) return false;

    /* 1. Retorna o stack pointer para a base do quadro local atual */
    virtual_memory->current_stack_pointer = virtual_memory->current_frame_pointer;

    /* 2. Restaura o frame pointer (ra) anterior */
    int64_t previous_frame_pointer = 0;
    if (!ijkl_vm_memory_pop_stack(virtual_memory, &previous_frame_pointer)) {
        return false;
    }
    virtual_memory->current_frame_pointer = (uint64_t)previous_frame_pointer;

    /* 3. Restaura o endereço de retorno salvo */
    int64_t return_address = 0;
    if (!ijkl_vm_memory_pop_stack(virtual_memory, &return_address)) {
        return false;
    }

    if (output_saved_return_address) {
        *output_saved_return_address = (uint64_t)return_address;
    }

    return true;
}

/*
 * EFEITO COLATERAL:
 * Aloca um novo bloco contíguo de bytes no heap dinâmico (alloc).
 */
uint64_t ijkl_vm_memory_allocate_heap(IjklVirtualMemory* virtual_memory, size_t requested_size_in_bytes) {
    if (!virtual_memory || requested_size_in_bytes == 0) return 0;

    size_t aligned_size = align_address_to_8_bytes(requested_size_in_bytes);
    uint64_t allocated_address = virtual_memory->heap_next_free_address;

    /* Verifica se ultrapassa o início da pilha */
    if (allocated_address + aligned_size >= virtual_memory->stack_base_address) {
        return 0; /* Heap out of memory */
    }

    virtual_memory->heap_next_free_address += aligned_size;
    virtual_memory->total_heap_allocated_bytes += aligned_size;

    /* Registra cabeçalho do bloco alocado */
    IjklHeapBlockHeader* header = (IjklHeapBlockHeader*)malloc(sizeof(IjklHeapBlockHeader));
    if (header) {
        header->block_address = allocated_address;
        header->requested_size_in_bytes = requested_size_in_bytes;
        header->is_allocated = true;
        header->next_block = virtual_memory->heap_first_block;
        virtual_memory->heap_first_block = header;
    }

    /* Zera a memória do bloco alocado */
    memset(&virtual_memory->raw_linear_memory_buffer[allocated_address], 0, aligned_size);

    return allocated_address;
}

/*
 * EFEITO COLATERAL:
 * Libera um bloco anteriormente alocado no heap (free).
 */
bool ijkl_vm_memory_free_heap(IjklVirtualMemory* virtual_memory, uint64_t heap_block_address) {
    if (!virtual_memory || heap_block_address < IJKL_VM_HEAP_BASE_ADDRESS) {
        return false;
    }

    IjklHeapBlockHeader* current = virtual_memory->heap_first_block;
    while (current) {
        if (current->block_address == heap_block_address && current->is_allocated) {
            current->is_allocated = false;
            return true;
        }
        current = current->next_block;
    }

    return false;
}
