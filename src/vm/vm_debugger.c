#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vm/vm_debugger.h"

/*
 * ==============================================================================
 * MÓDULO: IMPLEMENTAÇÃO DO DEPURADOR INTERATIVO CLI (IJKLANG VM)
 * ==============================================================================
 * Fornece inspeção pedagógica de baixo nível com pontos de parada, execução
 * passo a passo, despejo de registradores, visualização de pilha e hex dump.
 * ==============================================================================
 */

bool ijkl_vm_debugger_initialize(IjklVmDebugger* debugger,
                                 IjklVirtualMachine* virtual_machine,
                                 bool enable_interactive_mode) {
    if (!debugger || !virtual_machine) return false;

    memset(debugger, 0, sizeof(IjklVmDebugger));
    debugger->virtual_machine = virtual_machine;
    debugger->is_interactive_mode_enabled = enable_interactive_mode;
    debugger->is_single_step_mode = enable_interactive_mode;
    debugger->has_breakpoint_hit = false;

    return true;
}

void ijkl_vm_debugger_destroy(IjklVmDebugger* debugger) {
    if (!debugger) return;
    memset(debugger, 0, sizeof(IjklVmDebugger));
}

bool ijkl_vm_debugger_add_breakpoint(IjklVmDebugger* debugger, size_t instruction_index) {
    if (!debugger || debugger->active_breakpoints_count >= IJKL_VM_MAX_BREAKPOINTS) {
        return false;
    }

    if (ijkl_vm_debugger_has_breakpoint(debugger, instruction_index)) {
        return true;
    }

    debugger->active_breakpoints[debugger->active_breakpoints_count++] = instruction_index;
    return true;
}

bool ijkl_vm_debugger_remove_breakpoint(IjklVmDebugger* debugger, size_t instruction_index) {
    if (!debugger) return false;

    for (size_t i = 0; i < debugger->active_breakpoints_count; ++i) {
        if (debugger->active_breakpoints[i] == instruction_index) {
            for (size_t j = i; j + 1 < debugger->active_breakpoints_count; ++j) {
                debugger->active_breakpoints[j] = debugger->active_breakpoints[j + 1];
            }
            debugger->active_breakpoints_count--;
            return true;
        }
    }

    return false;
}

void ijkl_vm_debugger_clear_all_breakpoints(IjklVmDebugger* debugger) {
    if (!debugger) return;
    debugger->active_breakpoints_count = 0;
}

bool ijkl_vm_debugger_has_breakpoint(const IjklVmDebugger* debugger, size_t instruction_index) {
    if (!debugger) return false;

    for (size_t i = 0; i < debugger->active_breakpoints_count; ++i) {
        if (debugger->active_breakpoints[i] == instruction_index) {
            return true;
        }
    }

    return false;
}

void ijkl_vm_debugger_print_registers(const IjklVirtualMachine* virtual_machine) {
    if (!virtual_machine) return;

    printf("\n--- Virtual Machine Registers ---\n");
    for (int reg = 0; reg < 5; ++reg) {
        const IjklVmRegisterValue* val = &virtual_machine->registers[reg];
        if (val->type == VM_VALUE_TYPE_FLOAT) {
            printf("  %s: %f (float)\n", ijkl_vm_register_to_string((IjklVmRegister)reg), val->as.float_value);
        } else if (val->type == VM_VALUE_TYPE_STRING) {
            const char* str = ijkl_virtual_machine_get_register_string(virtual_machine, (IjklVmRegister)reg);
            printf("  %s: \"%s\" (string)\n", ijkl_vm_register_to_string((IjklVmRegister)reg), str ? str : "");
        } else {
            printf("  %s: %lld (0x%016llX)\n",
                   ijkl_vm_register_to_string((IjklVmRegister)reg),
                   (long long)val->as.integer_value,
                   (unsigned long long)val->as.unsigned_value);
        }
    }

    printf("  ci: %zu (Instruction Pointer / PC)\n", virtual_machine->instruction_pointer);
    printf("  sp: 0x%016llX (Stack Pointer)\n", (unsigned long long)virtual_machine->memory.current_stack_pointer);
    printf("  ra: 0x%016llX (Activation Frame Pointer)\n", (unsigned long long)virtual_machine->memory.current_frame_pointer);
    printf("  Cycles executed: %llu\n", (unsigned long long)virtual_machine->total_executed_cycles_count);
    printf("----------------------------------\n\n");
}

void ijkl_vm_debugger_print_memory_hex_dump(const IjklVirtualMachine* virtual_machine,
                                            uint64_t starting_address,
                                            size_t bytes_count) {
    if (!virtual_machine || bytes_count == 0) return;

    printf("\n--- Memory Hex Dump (0x%08llX, %zu bytes) ---\n",
           (unsigned long long)starting_address, bytes_count);

    uint64_t current = starting_address;
    uint64_t end = starting_address + bytes_count;

    while (current < end) {
        printf("0x%08llX: ", (unsigned long long)current);

        size_t line_bytes = (end - current > 16) ? 16 : (size_t)(end - current);

        for (size_t i = 0; i < 16; ++i) {
            if (i < line_bytes) {
                uint8_t byte_val = 0;
                if (ijkl_vm_memory_read_bytes(&virtual_machine->memory, current + i, &byte_val, 1)) {
                    printf("%02X ", byte_val);
                } else {
                    printf("?? ");
                }
            } else {
                printf("   ");
            }
            if (i == 7) printf(" ");
        }

        printf(" |");
        for (size_t i = 0; i < line_bytes; ++i) {
            uint8_t byte_val = 0;
            if (ijkl_vm_memory_read_bytes(&virtual_machine->memory, current + i, &byte_val, 1)) {
                putchar(isprint(byte_val) ? byte_val : '.');
            } else {
                putchar('?');
            }
        }
        printf("|\n");

        current += line_bytes;
    }
    printf("--------------------------------------------------\n\n");
}

void ijkl_vm_debugger_print_stack_trace(const IjklVirtualMachine* virtual_machine) {
    if (!virtual_machine) return;

    printf("\n--- Call Stack Trace (%zu frames) ---\n", virtual_machine->call_stack_depth);
    for (size_t i = 0; i < virtual_machine->call_stack_depth; ++i) {
        const IjklCallFrame* frame = &virtual_machine->call_stack[i];
        printf("  Frame #%zu: Return PC = %zu, Saved RA = 0x%08llX, Caller Frame Top = 0x%08llX\n",
               i, frame->return_instruction_index,
               (unsigned long long)frame->previous_frame_pointer,
               (unsigned long long)frame->caller_frame_top);
    }
    printf("  Current Frame: RA = 0x%08llX, SP = 0x%08llX\n",
           (unsigned long long)virtual_machine->memory.current_frame_pointer,
           (unsigned long long)virtual_machine->memory.current_stack_pointer);
    printf("--------------------------------------\n\n");
}

/*
 * EFEITO COLATERAL:
 * Intercepta o ciclo de execução antes de cada instrução quando em depuração.
 */
bool ijkl_vm_debugger_before_instruction_hook(IjklVmDebugger* debugger) {
    if (!debugger || !debugger->is_interactive_mode_enabled) return true;

    size_t current_pc = debugger->virtual_machine->instruction_pointer;
    bool is_breakpoint = ijkl_vm_debugger_has_breakpoint(debugger, current_pc);

    if (debugger->is_single_step_mode || is_breakpoint) {
        if (is_breakpoint) {
            printf("\n[Breakpoint hit at instruction %zu]\n", current_pc);
        }

        if (current_pc < debugger->virtual_machine->instructions_count) {
            const IjklVmInstruction* inst = &debugger->virtual_machine->instruction_stream[current_pc];
            printf("[CI:%zu, L:%zu] %s\n",
                   current_pc, inst->source_line_number,
                   ijkl_vm_opcode_to_string(inst->opcode));
        }

        /* Loop interativo do prompt de comando */
        while (1) {
            printf("(ijk-dbg) ");
            fflush(stdout);

            char command_buffer[128];
            if (!fgets(command_buffer, sizeof(command_buffer), stdin)) {
                return false;
            }

            char cmd = command_buffer[0];
            if (cmd == 's' || cmd == '\n') {
                /* Passo a passo */
                debugger->is_single_step_mode = true;
                break;
            } else if (cmd == 'c') {
                /* Continua a execução sem passo a passo */
                debugger->is_single_step_mode = false;
                break;
            } else if (cmd == 'r') {
                ijkl_vm_debugger_print_registers(debugger->virtual_machine);
            } else if (cmd == 'k') {
                ijkl_vm_debugger_print_stack_trace(debugger->virtual_machine);
            } else if (cmd == 'm') {
                uint64_t addr = debugger->virtual_machine->memory.current_frame_pointer;
                size_t count = 32;
                sscanf(command_buffer + 1, "%llx %zu", (unsigned long long*)&addr, &count);
                ijkl_vm_debugger_print_memory_hex_dump(debugger->virtual_machine, addr, count);
            } else if (cmd == 'b') {
                size_t bp_index = 0;
                if (sscanf(command_buffer + 1, "%zu", &bp_index) == 1) {
                    ijkl_vm_debugger_add_breakpoint(debugger, bp_index);
                    printf("Breakpoint added at instruction %zu.\n", bp_index);
                }
            } else if (cmd == 'q') {
                debugger->virtual_machine->execution_state = VM_STATE_HALTED;
                return false;
            } else {
                printf("Available commands: [s]tep, [c]ontinue, [r]egisters, [k] stack trace, [m]emory <hex_addr> <count>, [b]reakpoint <index>, [q]uit\n");
            }
        }
    }

    return true;
}
