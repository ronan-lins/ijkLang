#include <string.h>
#include "vm/ci_instruction.h"

/*
 * ==============================================================================
 * MÓDULO: TABELA DE NOMES E CONVERSÕES DE INSTRUÇÕES (IJKLANG VM)
 * ==============================================================================
 * Implementa conversores canônicos entre representações textuais do Código
 * Intermediário e suas respectivas estruturas enumeradas em memória.
 * ==============================================================================
 */

const char* ijkl_vm_register_to_string(IjklVmRegister register_identifier) {
    switch (register_identifier) {
        case IJKL_VM_REG_R0: return "r0";
        case IJKL_VM_REG_R1: return "r1";
        case IJKL_VM_REG_R2: return "r2";
        case IJKL_VM_REG_R3: return "r3";
        case IJKL_VM_REG_R4: return "r4";
        case IJKL_VM_REG_CI: return "ci";
        case IJKL_VM_REG_SP: return "sp";
        case IJKL_VM_REG_RA: return "ra";
        default:             return "none";
    }
}

IjklVmRegister ijkl_vm_register_from_string(const char* register_name) {
    if (!register_name) return IJKL_VM_REG_NONE;

    if (strcmp(register_name, "r0") == 0) return IJKL_VM_REG_R0;
    if (strcmp(register_name, "r1") == 0) return IJKL_VM_REG_R1;
    if (strcmp(register_name, "r2") == 0) return IJKL_VM_REG_R2;
    if (strcmp(register_name, "r3") == 0) return IJKL_VM_REG_R3;
    if (strcmp(register_name, "r4") == 0) return IJKL_VM_REG_R4;
    if (strcmp(register_name, "ci") == 0 || strcmp(register_name, "pc") == 0) return IJKL_VM_REG_CI;
    if (strcmp(register_name, "sp") == 0) return IJKL_VM_REG_SP;
    if (strcmp(register_name, "ra") == 0) return IJKL_VM_REG_RA;

    return IJKL_VM_REG_NONE;
}

const char* ijkl_vm_opcode_to_string(IjklVmOpcode opcode) {
    switch (opcode) {
        case IJKL_OPCODE_ASSIGN:        return ":=";
        case IJKL_OPCODE_ADD:           return "+";
        case IJKL_OPCODE_SUB:           return "-";
        case IJKL_OPCODE_MUL:           return "*";
        case IJKL_OPCODE_DIV:           return "/";
        case IJKL_OPCODE_MOD:           return "mod";
        case IJKL_OPCODE_EXP:           return "^";
        case IJKL_OPCODE_EQUAL:         return "=";
        case IJKL_OPCODE_NOT_EQUAL:     return "<>";
        case IJKL_OPCODE_LESS:          return "<";
        case IJKL_OPCODE_LESS_EQUAL:    return "<=";
        case IJKL_OPCODE_GREATER:       return ">";
        case IJKL_OPCODE_GREATER_EQUAL: return ">=";
        case IJKL_OPCODE_LOGICAL_AND:   return "and";
        case IJKL_OPCODE_LOGICAL_OR:    return "or";
        case IJKL_OPCODE_LOGICAL_XOR:   return "xor";
        case IJKL_OPCODE_BITWISE_AND:   return "&";
        case IJKL_OPCODE_BITWISE_OR:    return "|";
        case IJKL_OPCODE_SHIFT_LEFT:    return "<<";
        case IJKL_OPCODE_SHIFT_RIGHT:   return ">>";
        case IJKL_OPCODE_NEGATE:        return "- (neg)";
        case IJKL_OPCODE_LOGICAL_NOT:   return "not";
        case IJKL_OPCODE_ALLOC:         return "alloc";
        case IJKL_OPCODE_FREE:          return "free";
        case IJKL_OPCODE_JUMP:          return "jump";
        case IJKL_OPCODE_JUMP_IF:       return "jump_if";
        case IJKL_OPCODE_CALL:          return "call";
        case IJKL_OPCODE_RETURN:        return "return";
        case IJKL_OPCODE_HALT:          return "halt";
        case IJKL_OPCODE_PUSH:          return "push";
        case IJKL_OPCODE_POP:           return "pop";
        case IJKL_OPCODE_ALLOC_FRAME:   return "alloc_frame";
        case IJKL_OPCODE_FREE_FRAME:    return "free_frame";
        case IJKL_OPCODE_IN:            return "in";
        case IJKL_OPCODE_OUT:           return "out";
        case IJKL_OPCODE_IMPORT:        return "import";
        case IJKL_OPCODE_ERROR:         return "error";
        default:                        return "unknown";
    }
}
