#ifndef IJKL_VM_MEMORY_H
#define IJKL_VM_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common.h"

/*
 * ==============================================================================
 * MÓDULO: SUBSISTEMA DE MEMÓRIA LINEAR DA VM (IJKLANG VM)
 * ==============================================================================
 * Gerencia o espaço linear de endereçamento de 64 bits da Máquina Virtual,
 * organizando as regiões de:
 * 1. Página Nula protegida (0x0000 - 0x0FFF): Captura ponteiros nulos.
 * 2. Memória Global Estática (iniciando em 0x1000): Variáveis globais do programa.
 * 3. Heap de Alocação Dinâmica (0x100000 em diante): Alocado via 'alloc'/'free'.
 * 4. Pilha de Execução e Quadros de Ativação (Stack & Frame Pointers: sp / ra).
 *
 * Siglas técnicas empregadas:
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * - SP: Stack Pointer (Apontador do topo da pilha de execução).
 * - RA: Registro de Ativação / Frame Pointer (Base do quadro da rotina atual).
 * ==============================================================================
 */

#define IJKL_VM_MEMORY_TOTAL_CAPACITY_BYTES (16 * 1024 * 1024) /* 16 MB de espaço linear */
#define IJKL_VM_PAGE_NULL_END_ADDRESS       0x00001000ULL      /* Página nula até 0x0FFF */
#define IJKL_VM_GLOBAL_BASE_ADDRESS         0x00001000ULL      /* Segmento de dados globais */
#define IJKL_VM_HEAP_BASE_ADDRESS           0x00100000ULL      /* Base do heap dinâmico (1 MB) */
#define IJKL_VM_STACK_BASE_ADDRESS          0x00800000ULL      /* Base da pilha de execução (8 MB) */

/* Nó de controle para blocos alocados dinamicamente no heap */
typedef struct IjklHeapBlockHeader {
    uint64_t block_address;
    size_t requested_size_in_bytes;
    bool is_allocated;
    struct IjklHeapBlockHeader* next_block;
} IjklHeapBlockHeader;

/* Estrutura central do subsistema de memória da Máquina Virtual */
typedef struct IjklVirtualMemory {
    uint8_t* raw_linear_memory_buffer;
    size_t total_allocated_capacity_bytes;

    /* Controle da pilha e quadros de ativação */
    uint64_t stack_base_address;
    uint64_t current_stack_pointer;         /* Valor atual do registrador sp */
    uint64_t current_frame_pointer;         /* Valor atual do registrador ra */

    /* Controle do heap dinâmico */
    uint64_t heap_next_free_address;
    IjklHeapBlockHeader* heap_first_block;

    /* Mapa de tipos para cada palavra de 64 bits da memória */
    uint8_t* memory_type_tags_buffer;

    /* Estatísticas diagnósticas */
    size_t total_heap_allocated_bytes;
    size_t total_peak_stack_bytes;
} IjklVirtualMemory;

/* Gerenciamento de tags de tipo associadas a palavras de 64 bits */
uint8_t ijkl_vm_memory_get_tag(const IjklVirtualMemory* virtual_memory, uint64_t memory_address);
void ijkl_vm_memory_set_tag(IjklVirtualMemory* virtual_memory, uint64_t memory_address, uint8_t type_tag);

/* Inicialização e destruição do subsistema de memória linear */
bool ijkl_vm_memory_initialize(IjklVirtualMemory* virtual_memory, size_t total_capacity_bytes);
void ijkl_vm_memory_destroy(IjklVirtualMemory* virtual_memory);
void ijkl_vm_memory_reset(IjklVirtualMemory* virtual_memory);

/* Validação de limites de acesso e proteção de página nula */
bool ijkl_vm_memory_is_valid_address(const IjklVirtualMemory* virtual_memory,
                                     uint64_t memory_address,
                                     size_t access_size_bytes);

/*
 * EFEITOS COLATERAIS:
 * Operações de leitura e escrita tipadas no espaço linear de endereçamento.
 */
bool ijkl_vm_memory_read_bytes(const IjklVirtualMemory* virtual_memory,
                               uint64_t memory_address,
                               void* destination_buffer,
                               size_t bytes_to_read);

bool ijkl_vm_memory_write_bytes(IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                const void* source_buffer,
                                size_t bytes_to_write);

bool ijkl_vm_memory_read_integer(const IjklVirtualMemory* virtual_memory,
                                 uint64_t memory_address,
                                 int64_t* output_integer_value);

bool ijkl_vm_memory_write_integer(IjklVirtualMemory* virtual_memory,
                                  uint64_t memory_address,
                                  int64_t input_integer_value);

bool ijkl_vm_memory_read_float(const IjklVirtualMemory* virtual_memory,
                               uint64_t memory_address,
                               double* output_float_value);

bool ijkl_vm_memory_write_float(IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                double input_float_value);

bool ijkl_vm_memory_read_string(const IjklVirtualMemory* virtual_memory,
                                uint64_t memory_address,
                                char* destination_buffer,
                                size_t maximum_buffer_length);

bool ijkl_vm_memory_write_string(IjklVirtualMemory* virtual_memory,
                                 uint64_t memory_address,
                                 const char* source_string);

/*
 * EFEITOS COLATERAIS:
 * Manipulação da pilha de avaliação e chamadas (push, pop, alloc_frame, free_frame).
 */
bool ijkl_vm_memory_push_stack(IjklVirtualMemory* virtual_memory, int64_t value_to_push);
bool ijkl_vm_memory_pop_stack(IjklVirtualMemory* virtual_memory, int64_t* output_popped_value);

bool ijkl_vm_memory_alloc_frame(IjklVirtualMemory* virtual_memory,
                                size_t frame_local_variables_size_bytes,
                                uint64_t return_address_instruction_index);

bool ijkl_vm_memory_free_frame(IjklVirtualMemory* virtual_memory,
                               uint64_t* output_saved_return_address);

/*
 * EFEITOS COLATERAIS:
 * Alocação e liberação dinâmica de memória gerenciada (heap).
 */
uint64_t ijkl_vm_memory_allocate_heap(IjklVirtualMemory* virtual_memory, size_t requested_size_in_bytes);
bool ijkl_vm_memory_free_heap(IjklVirtualMemory* virtual_memory, uint64_t heap_block_address);

#endif /* IJKL_VM_MEMORY_H */
