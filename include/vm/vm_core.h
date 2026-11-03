#ifndef IJKL_VM_CORE_H
#define IJKL_VM_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "memory.h"
#include "vm/ci_instruction.h"
#include "vm/vm_memory.h"
#include "vm/vm_io.h"

/*
 * ==============================================================================
 * MÓDULO: NÚCLEO DE EXECUÇÃO DA MÁQUINA VIRTUAL (IJKLANG VM)
 * ==============================================================================
 * Define a estrutura do processador virtual, ciclo de busca e despacho
 * (fetch-decode-execute), banco de registradores de 64 bits e controle de estado.
 *
 * Siglas técnicas empregadas:
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * - PC / CI: Program Counter / Contador de Instrução.
 * - SP: Stack Pointer (Apontador da Pilha).
 * - RA: Registro de Ativação / Frame Pointer.
 * ==============================================================================
 */

/* Estados operacionais da Máquina Virtual */
typedef enum IjklVmExecutionState {
    VM_STATE_READY = 0,             /* Inicializada, programa carregado e pronta para executar */
    VM_STATE_RUNNING = 1,           /* Em execução contínua */
    VM_STATE_PAUSED = 2,            /* Pausada pelo depurador (modo passo a passo ou breakpoint) */
    VM_STATE_HALTED = 3,            /* Execução terminada com sucesso via instrução 'halt' */
    VM_STATE_ERROR = 4              /* Interrompida devido a falha crítica em tempo de execução */
} IjklVmExecutionState;

/* Códigos formais de erro de tempo de execução */
typedef enum IjklVmErrorCode {
    VM_ERROR_NONE = 0,
    VM_ERROR_DIVISION_BY_ZERO = 1,
    VM_ERROR_INVALID_OPCODE = 2,
    VM_ERROR_MEMORY_ACCESS_VIOLATION = 3,
    VM_ERROR_STACK_OVERFLOW = 4,
    VM_ERROR_STACK_UNDERFLOW = 5,
    VM_ERROR_UNRESOLVED_LABEL = 6,
    VM_ERROR_IO_PORT_FAULT = 7,
    VM_ERROR_USER_ASSERTION = 8,
    VM_ERROR_INSTRUCTION_OUT_OF_BOUNDS = 9
} IjklVmErrorCode;

/* Tipos dinâmicos suportados nos registradores da VM */
typedef enum IjklVmValueType {
    VM_VALUE_TYPE_INT = 0,
    VM_VALUE_TYPE_FLOAT = 1,
    VM_VALUE_TYPE_STRING = 2,
    VM_VALUE_TYPE_CHAR = 3,
    VM_VALUE_TYPE_BOOL = 4
} IjklVmValueType;

/* Representação interna para cada registrador da arquitetura */
typedef struct IjklVmRegisterValue {
    IjklVmValueType type;
    union {
        int64_t integer_value;
        uint64_t unsigned_value;
        double float_value;
        const char* string_pointer;
    } as;
} IjklVmRegisterValue;

#define IJKL_VM_MAX_CALL_STACK_DEPTH 1024

/* Registro de chamada de rotina para preservação de quadros e retorno */
typedef struct IjklCallFrame {
    size_t return_instruction_index;
    uint64_t previous_frame_pointer;
    uint64_t caller_frame_top;
    const char* called_routine_name;
} IjklCallFrame;

/* Estrutura central da Máquina Virtual */
typedef struct IjklVirtualMachine {
    IjklMemoryArena* memory_arena;

    /* Banco de Registradores de 64 bits */
    IjklVmRegisterValue registers[IJKL_VM_REG_COUNT];

    /* Segmento de Código e Apontador de Instrução (PC/CI) */
    IjklVmInstruction* instruction_stream;
    size_t instructions_count;
    size_t instruction_pointer;             /* Índice da instrução atual em execução */

    /* Subsistemas integrados */
    IjklVirtualMemory memory;
    uint64_t current_frame_top;
    IjklIoBus io_bus;
    bool console_has_pending_character;
    int console_pending_character;
    IjklDmaController dma_controller;
    IjklFileIoController file_io_controller;

    /* Pilha de controle de chamadas (Call Stack) */
    IjklCallFrame call_stack[IJKL_VM_MAX_CALL_STACK_DEPTH];
    size_t call_stack_depth;

    /* Estado de execução e diagnósticos */
    IjklVmExecutionState execution_state;
    IjklVmErrorCode last_error_code;
    char error_message_buffer[256];
    size_t error_line_number;

    /* Métricas de desempenho */
    uint64_t total_executed_cycles_count;
} IjklVirtualMachine;

/* Inicialização, ciclo de vida e reset da Máquina Virtual */
bool ijkl_virtual_machine_initialize(IjklVirtualMachine* machine,
                                     IjklMemoryArena* memory_arena,
                                     size_t memory_capacity_bytes);

void ijkl_virtual_machine_destroy(IjklVirtualMachine* machine);
void ijkl_virtual_machine_reset(IjklVirtualMachine* machine);

/*
 * EFEITO COLATERAL:
 * Carrega o vetor linear de instruções compiladas prontas para execução.
 */
bool ijkl_virtual_machine_load_instructions(IjklVirtualMachine* machine,
                                            IjklVmInstruction* instructions,
                                            size_t instructions_count);

/*
 * EFEITO COLATERAL:
 * Executa uma única instrução (ciclo de busca, decodificação e despacho).
 * Retorna true se a máquina continua apta a executar, ou false se parou (halt/erro).
 */
bool ijkl_virtual_machine_step(IjklVirtualMachine* machine);

/*
 * EFEITO COLATERAL:
 * Executa continuamente o programa até encontrar 'halt', erro ou breakpoint.
 */
bool ijkl_virtual_machine_run(IjklVirtualMachine* machine);

/* Acessores e modificadores de registradores */
int64_t ijkl_virtual_machine_get_register_integer(const IjklVirtualMachine* machine,
                                                  IjklVmRegister register_identifier);

void ijkl_virtual_machine_set_register_integer(IjklVirtualMachine* machine,
                                               IjklVmRegister register_identifier,
                                               int64_t integer_value);

double ijkl_virtual_machine_get_register_float(const IjklVirtualMachine* machine,
                                               IjklVmRegister register_identifier);

void ijkl_virtual_machine_set_register_float(IjklVirtualMachine* machine,
                                             IjklVmRegister register_identifier,
                                             double float_value);

const char* ijkl_virtual_machine_get_register_string(const IjklVirtualMachine* machine,
                                                     IjklVmRegister register_identifier);

void ijkl_virtual_machine_set_register_string(IjklVirtualMachine* machine,
                                              IjklVmRegister register_identifier,
                                              const char* string_value);

/* Consulta de mensagens e diagnósticos */
const char* ijkl_virtual_machine_get_error_message(const IjklVirtualMachine* machine);
const char* ijkl_virtual_machine_error_code_to_string(IjklVmErrorCode error_code);

#endif /* IJKL_VM_CORE_H */
