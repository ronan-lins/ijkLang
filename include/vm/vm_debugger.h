#ifndef IJKL_VM_DEBUGGER_H
#define IJKL_VM_DEBUGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "vm/vm_core.h"

/*
 * ==============================================================================
 * MÓDULO: DEPURADOR INTERATIVO CLI DA MÁQUINA VIRTUAL (IJKLANG VM)
 * ==============================================================================
 * Proporciona recursos avançados de depuração e inspeção pedagógica:
 * - Pontos de parada (breakpoints) por índice de instrução ou rótulo simbólico.
 * - Execução passo a passo (single-step mode).
 * - Inspeção formatada do banco de registradores (r0..r4, ra, sp, ci).
 * - Despejo de memória (hex dump) e visualização hierárquica da pilha de chamadas.
 *
 * Siglas técnicas empregadas:
 * - CLI: Command-Line Interface (Interface de Linha de Comando).
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * ==============================================================================
 */

#define IJKL_VM_MAX_BREAKPOINTS 64

/* Estrutura central do depurador integrado */
typedef struct IjklVmDebugger {
    IjklVirtualMachine* virtual_machine;

    size_t active_breakpoints[IJKL_VM_MAX_BREAKPOINTS];
    size_t active_breakpoints_count;

    bool is_interactive_mode_enabled;
    bool is_single_step_mode;
    bool has_breakpoint_hit;
} IjklVmDebugger;

/* Inicialização e configuração do depurador */
bool ijkl_vm_debugger_initialize(IjklVmDebugger* debugger,
                                 IjklVirtualMachine* virtual_machine,
                                 bool enable_interactive_mode);

void ijkl_vm_debugger_destroy(IjklVmDebugger* debugger);

/*
 * EFEITOS COLATERAIS:
 * Gerenciamento de pontos de parada (breakpoints).
 */
bool ijkl_vm_debugger_add_breakpoint(IjklVmDebugger* debugger, size_t instruction_index);
bool ijkl_vm_debugger_remove_breakpoint(IjklVmDebugger* debugger, size_t instruction_index);
void ijkl_vm_debugger_clear_all_breakpoints(IjklVmDebugger* debugger);
bool ijkl_vm_debugger_has_breakpoint(const IjklVmDebugger* debugger, size_t instruction_index);

/*
 * EFEITO COLATERAL:
 * Intercepta o ciclo de execução antes de cada instrução.
 * Se houver breakpoint ativo ou modo passo a passo, suspende a VM e abre o prompt interativo.
 */
bool ijkl_vm_debugger_before_instruction_hook(IjklVmDebugger* debugger);

/*
 * EFEITO COLATERAL:
 * Despeja no terminal (stdout) o estado formatado dos registradores e ponteiros.
 */
void ijkl_vm_debugger_print_registers(const IjklVirtualMachine* virtual_machine);

/*
 * EFEITO COLATERAL:
 * Despeja no terminal a memória linear em formato hexadecimal e ASCII.
 */
void ijkl_vm_debugger_print_memory_hex_dump(const IjklVirtualMachine* virtual_machine,
                                            uint64_t starting_address,
                                            size_t bytes_count);

/*
 * EFEITO COLATERAL:
 * Imprime a pilha de execução com delimitação dos quadros de ativação (ra/sp).
 */
void ijkl_vm_debugger_print_stack_trace(const IjklVirtualMachine* virtual_machine);

#endif /* IJKL_VM_DEBUGGER_H */
