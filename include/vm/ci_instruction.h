#ifndef IJKL_VM_CI_INSTRUCTION_H
#define IJKL_VM_CI_INSTRUCTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * ==============================================================================
 * MÓDULO: INSTRUÇÕES INTERMEDIÁRIAS LINEARES DA VM (IJKLANG VM)
 * ==============================================================================
 * Define os códigos operacionais (opcodes), operandos tipados, modos de
 * endereçamento e estruturas de dados de instruções decodificadas do
 * Código Intermediário (.ci) para execução na Máquina Virtual.
 *
 * Siglas técnicas empregadas:
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * - CI: Código Intermediário Linear textual (*.ci).
 * - ALU: Arithmetic Logic Unit (Unidade Lógica e Aritmética).
 * - LEA: Load Effective Address (Cálculo de endereço efetivo).
 * ==============================================================================
 */

/* Identificadores formais dos registradores da arquitetura da VM */
typedef enum IjklVmRegister {
    IJKL_VM_REG_NONE = -1,
    IJKL_VM_REG_R0 = 0,     /* Registrador de propósito geral / retorno de chamadas */
    IJKL_VM_REG_R1 = 1,     /* Registrador de propósito geral / segundo operando */
    IJKL_VM_REG_R2 = 2,     /* Registrador de propósito geral / auxiliar */
    IJKL_VM_REG_R3 = 3,     /* Registrador de propósito geral / endereçamento */
    IJKL_VM_REG_R4 = 4,     /* Registrador de propósito geral */
    IJKL_VM_REG_CI = 5,     /* Contador de Instrução (Program Counter / PC) */
    IJKL_VM_REG_SP = 6,     /* Apontador da Pilha de Execução (Stack Pointer) */
    IJKL_VM_REG_RA = 7,     /* Registro de Ativação / Quadro Atual (Frame Pointer) */
    IJKL_VM_REG_COUNT = 8
} IjklVmRegister;

/* Códigos operacionais das instruções decodificadas do Código Intermediário */
typedef enum IjklVmOpcode {
    /* 1. Atribuição e movimentação de dados */
    IJKL_OPCODE_ASSIGN,             /* destino := operando */

    /* 2. Operações aritméticas */
    IJKL_OPCODE_ADD,                /* destino := op1 + op2 */
    IJKL_OPCODE_SUB,                /* destino := op1 - op2 */
    IJKL_OPCODE_MUL,                /* destino := op1 * op2 */
    IJKL_OPCODE_DIV,                /* destino := op1 / op2 */
    IJKL_OPCODE_MOD,                /* destino := op1 mod op2 (ou %) */
    IJKL_OPCODE_EXP,                /* destino := op1 ^ op2 */

    /* 3. Operações relacionais */
    IJKL_OPCODE_EQUAL,              /* destino := op1 = op2 */
    IJKL_OPCODE_NOT_EQUAL,          /* destino := op1 <> op2 */
    IJKL_OPCODE_LESS,               /* destino := op1 < op2 */
    IJKL_OPCODE_LESS_EQUAL,         /* destino := op1 <= op2 */
    IJKL_OPCODE_GREATER,            /* destino := op1 > op2 */
    IJKL_OPCODE_GREATER_EQUAL,      /* destino := op1 >= op2 */

    /* 4. Operações lógicas e bitwise */
    IJKL_OPCODE_LOGICAL_AND,        /* destino := op1 and op2 */
    IJKL_OPCODE_LOGICAL_OR,         /* destino := op1 or op2 */
    IJKL_OPCODE_LOGICAL_XOR,        /* destino := op1 xor op2 */
    IJKL_OPCODE_BITWISE_AND,        /* destino := op1 & op2 */
    IJKL_OPCODE_BITWISE_OR,         /* destino := op1 | op2 */
    IJKL_OPCODE_SHIFT_LEFT,         /* destino := op1 << op2 */
    IJKL_OPCODE_SHIFT_RIGHT,        /* destino := op1 >> op2 */

    /* 5. Operações unárias */
    IJKL_OPCODE_NEGATE,             /* destino := -op */
    IJKL_OPCODE_LOGICAL_NOT,        /* destino := not op */

    /* 6. Alocação e desapontamento de memória dinâmica */
    IJKL_OPCODE_ALLOC,              /* destino := alloc(tamanho) */
    IJKL_OPCODE_FREE,               /* free(ponteiro) */

    /* 7. Controle de fluxo de execução */
    IJKL_OPCODE_JUMP,               /* jump #rotulo */
    IJKL_OPCODE_JUMP_IF,            /* jump_if condicao, #rotulo */
    IJKL_OPCODE_CALL,               /* call #rotulo */
    IJKL_OPCODE_RETURN,             /* return [valor] */
    IJKL_OPCODE_HALT,               /* halt */

    /* 8. Pilha e quadros de ativação */
    IJKL_OPCODE_PUSH,               /* push valor */
    IJKL_OPCODE_POP,                /* pop destino */
    IJKL_OPCODE_ALLOC_FRAME,        /* alloc_frame tamanho_em_bytes */
    IJKL_OPCODE_FREE_FRAME,         /* free_frame */

    /* 9. Barramento de entrada e saída (I/O) */
    IJKL_OPCODE_IN,                 /* in endereco_porta, destino */
    IJKL_OPCODE_OUT,                /* out endereco_porta, valor */

    /* 10. Extensibilidade e diagnósticos */
    IJKL_OPCODE_IMPORT,             /* import porta, id, "plugin" */
    IJKL_OPCODE_ERROR               /* error "mensagem" */
} IjklVmOpcode;

/* Classificação do tipo do operando em uma instrução */
typedef enum IjklVmOperandKind {
    OPERAND_KIND_NONE = 0,
    OPERAND_KIND_REGISTER,          /* Registrador direto: r0, r1, ra, sp, etc. */
    OPERAND_KIND_IMMEDIATE_INT,     /* Constante inteira imediata de 64 bits */
    OPERAND_KIND_IMMEDIATE_FLOAT,   /* Constante em ponto flutuante de precisão dupla (double) */
    OPERAND_KIND_IMMEDIATE_STRING,  /* Cadeia de caracteres literal (UTF-8) */
    OPERAND_KIND_IMMEDIATE_BOOL,    /* Constante booleana (true/false) */
    OPERAND_KIND_IMMEDIATE_CHAR,    /* Constante caractere ('c') */
    OPERAND_KIND_MEMORY_INDIRECT,   /* Acesso indireto à memória: [base, deslocamento] */
    OPERAND_KIND_LABEL_REFERENCE    /* Rótulo de salto ou chamada: #nome_do_rotulo */
} IjklVmOperandKind;

/* Estrutura de dados que descreve um operando da máquina virtual */
typedef struct IjklVmOperand {
    IjklVmOperandKind kind;
    union {
        IjklVmRegister register_identifier;
        int64_t integer_value;
        double float_value;
        const char* string_value;
        bool boolean_value;
        char character_value;

        /* Acesso indireto à memória com base e deslocamento: [base, deslocamento] ou byte [base, deslocamento] */
        struct {
            IjklVmRegister base_register;       /* Se IJKL_VM_REG_NONE, usa base_address absoluto */
            uint64_t base_address;
            IjklVmRegister offset_register;     /* Se IJKL_VM_REG_NONE, usa displacement_bytes */
            int64_t displacement_bytes;
            bool is_byte_access;                /* Verdadeiro se acesso for de 1 byte ('byte [...]') */
        } memory_reference;

        /* Rótulo simbólico pendente ou resolvido */
        struct {
            const char* label_name;
            size_t target_instruction_index;    /* Índice absoluto no vetor de instruções */
            bool is_resolved;
        } label_reference;
    } value;
    uint64_t string_virtual_address;            /* Endereço virtual na memória linear da VM para literais de texto */
} IjklVmOperand;

/* Instrução elementar decodificada e pronta para despacho pela VM */
typedef struct IjklVmInstruction {
    IjklVmOpcode opcode;
    IjklVmOperand destination;
    IjklVmOperand first_operand;
    IjklVmOperand second_operand;

    size_t source_line_number;
    const char* defined_label_name;             /* Rótulo apontando para esta instrução (se houver) */
} IjklVmInstruction;

/* Conversores e utilitários de registradores */
const char* ijkl_vm_register_to_string(IjklVmRegister register_identifier);
IjklVmRegister ijkl_vm_register_from_string(const char* register_name);

/* Conversores de opcodes */
const char* ijkl_vm_opcode_to_string(IjklVmOpcode opcode);

#endif /* IJKL_VM_CI_INSTRUCTION_H */
