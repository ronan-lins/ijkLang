#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vm/ci_loader.h"
#include "common.h"

/*
 * ==============================================================================
 * MÓDULO: CARREGADOR E PARSER DE CÓDIGO INTERMEDIÁRIO (.CI)
 * ==============================================================================
 * Analisa a sintaxe textual do Código Intermediário, decodifica as instruções
 * lineares e resolve as referências cruzadas de rótulos simbólicos (#rotulo).
 *
 * Utiliza o modelo de duas passagens:
 * - Passagem 1: Decodificação léxica/sintática e registro dos rótulos e seus índices.
 * - Passagem 2: Resolução de endereços simbólicos (ligação/linking de saltos e chamadas).
 * ==============================================================================
 */

#define IJKL_MAX_LABELS_CAPACITY 4096
#define IJKL_INITIAL_INSTRUCTIONS_CAPACITY 1024

/* Registro de rótulo simbólico identificado no código */
typedef struct IjklLabelDefinition {
    const char* label_name;
    size_t target_instruction_index;
} IjklLabelDefinition;

/* Contexto interno de análise durante a carga do arquivo .ci */
typedef struct IjklCiLoaderContext {
    IjklMemoryArena* memory_arena;
    const char* source_text;
    const char* current_pointer;
    size_t current_line_number;

    IjklVmInstruction* instructions_array;
    size_t instructions_count;
    size_t instructions_capacity;

    IjklLabelDefinition label_table[IJKL_MAX_LABELS_CAPACITY];
    size_t label_table_count;

    char diagnostic_error_message[256];
} IjklCiLoaderContext;

/* Funções utilitárias de manipulação de caracteres e tokens */
static void skip_whitespace_and_comments(IjklCiLoaderContext* context) {
    while (*context->current_pointer != '\0') {
        char current_char = *context->current_pointer;

        if (current_char == '\n') {
            context->current_line_number++;
            context->current_pointer++;
            continue;
        }

        if (isspace((unsigned char)current_char)) {
            context->current_pointer++;
            continue;
        }

        /* Comentário de linha: // */
        if (current_char == '/' && *(context->current_pointer + 1) == '/') {
            context->current_pointer += 2;
            while (*context->current_pointer != '\0' && *context->current_pointer != '\n') {
                context->current_pointer++;
            }
            continue;
        }

        /* Comentário de bloco: estilo C (/ * ... * /) */
        if (current_char == '/' && *(context->current_pointer + 1) == '*') {
            context->current_pointer += 2;
            while (*context->current_pointer != '\0') {
                if (*context->current_pointer == '\n') {
                    context->current_line_number++;
                }
                if (*context->current_pointer == '*' && *(context->current_pointer + 1) == '/') {
                    context->current_pointer += 2;
                    break;
                }
                context->current_pointer++;
            }
            continue;
        }

        break;
    }
}

static bool match_keyword(IjklCiLoaderContext* context, const char* keyword) {
    size_t length = strlen(keyword);
    if (strncmp(context->current_pointer, keyword, length) == 0) {
        char next_char = *(context->current_pointer + length);
        if (!isalnum((unsigned char)next_char) && next_char != '_') {
            context->current_pointer += length;
            return true;
        }
    }
    return false;
}

static bool match_exact(IjklCiLoaderContext* context, const char* sequence) {
    size_t length = strlen(sequence);
    if (strncmp(context->current_pointer, sequence, length) == 0) {
        context->current_pointer += length;
        return true;
    }
    return false;
}

/* Leitura de identificadores alfanuméricos com pontos, underscores e cifrões (mangled names) */
static const char* parse_identifier_token(IjklCiLoaderContext* context) {
    skip_whitespace_and_comments(context);
    const char* start = context->current_pointer;

    if (!isalpha((unsigned char)*context->current_pointer) &&
        *context->current_pointer != '_' &&
        *context->current_pointer != '$') {
        return NULL;
    }

    while (isalnum((unsigned char)*context->current_pointer) ||
           *context->current_pointer == '_' ||
           *context->current_pointer == '.' ||
           *context->current_pointer == '$') {
        context->current_pointer++;
    }

    size_t length = (size_t)(context->current_pointer - start);
    char* identifier_buffer = (char*)ijkl_memory_arena_allocate(context->memory_arena, length + 1);
    memcpy(identifier_buffer, start, length);
    identifier_buffer[length] = '\0';
    return identifier_buffer;
}

/* Parse de literal de string entre aspas duplas com suporte a escapes básicos */
static const char* parse_string_literal(IjklCiLoaderContext* context) {
    if (*context->current_pointer != '"') return NULL;
    context->current_pointer++; /* Consome aspas iniciais */

    char buffer[4096];
    size_t buffer_index = 0;

    while (*context->current_pointer != '\0' && *context->current_pointer != '"') {
        if (*context->current_pointer == '\\') {
            context->current_pointer++;
            char escaped = *context->current_pointer;
            if (escaped == 'n') buffer[buffer_index++] = '\n';
            else if (escaped == 't') buffer[buffer_index++] = '\t';
            else if (escaped == 'r') buffer[buffer_index++] = '\r';
            else if (escaped == '\\') buffer[buffer_index++] = '\\';
            else if (escaped == '"') buffer[buffer_index++] = '"';
            else buffer[buffer_index++] = escaped;
            context->current_pointer++;
        } else {
            buffer[buffer_index++] = *context->current_pointer++;
        }
        if (buffer_index >= sizeof(buffer) - 1) break;
    }

    buffer[buffer_index] = '\0';
    if (*context->current_pointer == '"') {
        context->current_pointer++; /* Consome aspas finais */
    }

    char* allocated_string = (char*)ijkl_memory_arena_allocate(context->memory_arena, buffer_index + 1);
    memcpy(allocated_string, buffer, buffer_index + 1);
    return allocated_string;
}

/* Parse de literal de caractere entre aspas simples */
static char parse_character_literal(IjklCiLoaderContext* context) {
    if (*context->current_pointer != '\'') return '\0';
    context->current_pointer++;

    char result = '\0';
    if (*context->current_pointer == '\\') {
        context->current_pointer++;
        char escaped = *context->current_pointer;
        if (escaped == 'n') result = '\n';
        else if (escaped == 't') result = '\t';
        else if (escaped == 'r') result = '\r';
        else if (escaped == '0') result = '\0';
        else if (escaped == '\\') result = '\\';
        else if (escaped == '\'') result = '\'';
        else result = escaped;
        context->current_pointer++;
    } else {
        result = *context->current_pointer++;
    }

    if (*context->current_pointer == '\'') {
        context->current_pointer++;
    }
    return result;
}

/*
 * Decodifica um operando arbitrário do Código Intermediário:
 * Registrador, constante inteira, real, string, booleano, indireto de memória ou rótulo.
 */
static bool parse_operand(IjklCiLoaderContext* context, IjklVmOperand* output_operand) {
    skip_whitespace_and_comments(context);
    memset(output_operand, 0, sizeof(IjklVmOperand));

    /* 1. Rótulo simbólico iniciado por '#' */
    if (*context->current_pointer == '#') {
        context->current_pointer++;
        const char* label_name = parse_identifier_token(context);
        if (!label_name) return false;

        output_operand->kind = OPERAND_KIND_LABEL_REFERENCE;
        output_operand->value.label_reference.label_name = label_name;
        output_operand->value.label_reference.target_instruction_index = 0;
        output_operand->value.label_reference.is_resolved = false;
        return true;
    }

    /* 2. Referência Indireta de Memória: [base, deslocamento], [base] ou byte [base, deslocamento] */
    bool is_byte_access = false;
    const char* saved_pointer = context->current_pointer;
    if (match_keyword(context, "byte")) {
        skip_whitespace_and_comments(context);
        if (*context->current_pointer == '[') {
            is_byte_access = true;
        } else {
            context->current_pointer = saved_pointer;
        }
    }

    if (*context->current_pointer == '[') {
        context->current_pointer++;
        skip_whitespace_and_comments(context);

        output_operand->kind = OPERAND_KIND_MEMORY_INDIRECT;
        output_operand->value.memory_reference.base_register = IJKL_VM_REG_NONE;
        output_operand->value.memory_reference.base_address = 0;
        output_operand->value.memory_reference.offset_register = IJKL_VM_REG_NONE;
        output_operand->value.memory_reference.displacement_bytes = 0;
        output_operand->value.memory_reference.is_byte_access = is_byte_access;

        /* Identifica se a base é registrador ou endereço imediato (ex: 0x1000) */
        if (isalpha((unsigned char)*context->current_pointer)) {
            const char* reg_name = parse_identifier_token(context);
            output_operand->value.memory_reference.base_register = ijkl_vm_register_from_string(reg_name);
        } else {
            char* end_ptr = NULL;
            output_operand->value.memory_reference.base_address = strtoull(context->current_pointer, &end_ptr, 0);
            context->current_pointer = end_ptr;
        }

        skip_whitespace_and_comments(context);

        /* Deslocamento opcional separado por vírgula */
        if (*context->current_pointer == ',') {
            context->current_pointer++;
            skip_whitespace_and_comments(context);

            if (isalpha((unsigned char)*context->current_pointer)) {
                const char* off_reg = parse_identifier_token(context);
                output_operand->value.memory_reference.offset_register = ijkl_vm_register_from_string(off_reg);
            } else {
                char* end_ptr = NULL;
                output_operand->value.memory_reference.displacement_bytes = strtoll(context->current_pointer, &end_ptr, 0);
                context->current_pointer = end_ptr;
            }
        }

        skip_whitespace_and_comments(context);
        if (*context->current_pointer == ']') {
            context->current_pointer++;
        }
        return true;
    }

    /* 3. Literal de texto entre aspas */
    if (*context->current_pointer == '"') {
        const char* string_literal = parse_string_literal(context);
        output_operand->kind = OPERAND_KIND_IMMEDIATE_STRING;
        output_operand->value.string_value = string_literal;
        return true;
    }

    /* 4. Literal de caractere entre aspas simples */
    if (*context->current_pointer == '\'') {
        char character_value = parse_character_literal(context);
        output_operand->kind = OPERAND_KIND_IMMEDIATE_CHAR;
        output_operand->value.character_value = character_value;
        return true;
    }

    /* 5. Booleanos e nulos */
    if (match_keyword(context, "true") || match_keyword(context, "verdadeiro")) {
        output_operand->kind = OPERAND_KIND_IMMEDIATE_BOOL;
        output_operand->value.boolean_value = true;
        return true;
    }
    if (match_keyword(context, "false") || match_keyword(context, "falso")) {
        output_operand->kind = OPERAND_KIND_IMMEDIATE_BOOL;
        output_operand->value.boolean_value = false;
        return true;
    }
    if (match_keyword(context, "null") || match_keyword(context, "nulo")) {
        output_operand->kind = OPERAND_KIND_IMMEDIATE_INT;
        output_operand->value.integer_value = 0;
        return true;
    }

    /* 6. Números inteiros ou em ponto flutuante */
    if (isdigit((unsigned char)*context->current_pointer) ||
        (*context->current_pointer == '-' && isdigit((unsigned char)*(context->current_pointer + 1)))) {
        char* end_ptr = NULL;
        const char* num_start = context->current_pointer;

        /* Verifica se há ponto decimal indicando float */
        bool is_float = false;
        const char* scan = num_start;
        if (*scan == '-') scan++;
        while (isdigit((unsigned char)*scan)) scan++;
        if (*scan == '.') is_float = true;

        if (is_float) {
            double float_val = strtod(num_start, &end_ptr);
            if (*end_ptr == 'f' || *end_ptr == 'F') end_ptr++;
            context->current_pointer = end_ptr;
            output_operand->kind = OPERAND_KIND_IMMEDIATE_FLOAT;
            output_operand->value.float_value = float_val;
        } else {
            int64_t int_val = strtoll(num_start, &end_ptr, 0);
            context->current_pointer = end_ptr;
            output_operand->kind = OPERAND_KIND_IMMEDIATE_INT;
            output_operand->value.integer_value = int_val;
        }
        return true;
    }

    /* 7. Registrador ou identificador */
    if (isalpha((unsigned char)*context->current_pointer) || *context->current_pointer == '_') {
        const char* identifier = parse_identifier_token(context);
        IjklVmRegister reg = ijkl_vm_register_from_string(identifier);
        if (reg != IJKL_VM_REG_NONE) {
            output_operand->kind = OPERAND_KIND_REGISTER;
            output_operand->value.register_identifier = reg;
            return true;
        }
    }

    return false;
}

/* Adiciona uma instrução decodificada ao vetor dinâmico da VM */
static void append_instruction(IjklCiLoaderContext* context, const IjklVmInstruction* instruction) {
    if (context->instructions_count >= context->instructions_capacity) {
        size_t new_capacity = context->instructions_capacity * 2;
        if (new_capacity < IJKL_INITIAL_INSTRUCTIONS_CAPACITY) {
            new_capacity = IJKL_INITIAL_INSTRUCTIONS_CAPACITY;
        }

        IjklVmInstruction* new_array = (IjklVmInstruction*)ijkl_memory_arena_allocate(
            context->memory_arena, sizeof(IjklVmInstruction) * new_capacity);

        if (context->instructions_array && context->instructions_count > 0) {
            memcpy(new_array, context->instructions_array, sizeof(IjklVmInstruction) * context->instructions_count);
        }

        context->instructions_array = new_array;
        context->instructions_capacity = new_capacity;
    }

    context->instructions_array[context->instructions_count] = *instruction;
    context->instructions_count++;
}

/* Registra um rótulo apontando para o índice da próxima instrução emitida */
static bool register_label(IjklCiLoaderContext* context, const char* label_name) {
    if (context->label_table_count >= IJKL_MAX_LABELS_CAPACITY) {
        snprintf(context->diagnostic_error_message, sizeof(context->diagnostic_error_message),
                 "Label table exceeded capacity of %d labels.", IJKL_MAX_LABELS_CAPACITY);
        return false;
    }

    context->label_table[context->label_table_count].label_name = label_name;
    context->label_table[context->label_table_count].target_instruction_index = context->instructions_count;
    context->label_table_count++;
    return true;
}

/*
 * EFEITO COLATERAL:
 * Decodifica uma única linha ou comando do arquivo .ci e acrescenta ao vetor de instruções.
 */
static bool parse_ci_statement(IjklCiLoaderContext* context) {
    skip_whitespace_and_comments(context);
    if (*context->current_pointer == '\0') return true;

    /* 1. Definição de rótulo: #nome: */
    if (*context->current_pointer == '#') {
        const char* label_start = context->current_pointer;
        context->current_pointer++;
        const char* label_name = parse_identifier_token(context);

        skip_whitespace_and_comments(context);
        if (*context->current_pointer == ':') {
            context->current_pointer++; /* Consome ':' */
            if (!register_label(context, label_name)) return false;
            return true;
        } else {
            /* Não é definição de rótulo, restaura o ponteiro para o início */
            context->current_pointer = label_start;
        }
    }

    IjklVmInstruction instruction;
    memset(&instruction, 0, sizeof(IjklVmInstruction));
    instruction.source_line_number = context->current_line_number;

    /* 2. alloc_frame <tamanho>; */
    if (match_keyword(context, "alloc_frame") || match_keyword(context, "alocar_quadro")) {
        instruction.opcode = IJKL_OPCODE_ALLOC_FRAME;
        skip_whitespace_and_comments(context);
        if (*context->current_pointer != ';') {
            parse_operand(context, &instruction.first_operand);
        } else {
            instruction.first_operand.kind = OPERAND_KIND_IMMEDIATE_INT;
            instruction.first_operand.value.integer_value = 0;
        }
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 3. free_frame; */
    if (match_keyword(context, "free_frame") || match_keyword(context, "liberar_quadro")) {
        instruction.opcode = IJKL_OPCODE_FREE_FRAME;
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 4. halt; */
    if (match_keyword(context, "halt") || match_keyword(context, "finalizar")) {
        instruction.opcode = IJKL_OPCODE_HALT;
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 5. return [valor]; */
    if (match_keyword(context, "return") || match_keyword(context, "retornar")) {
        instruction.opcode = IJKL_OPCODE_RETURN;
        skip_whitespace_and_comments(context);
        if (*context->current_pointer != ';') {
            parse_operand(context, &instruction.first_operand);
        }
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 6. jump #rotulo; */
    if (match_keyword(context, "jump") || match_keyword(context, "saltar")) {
        instruction.opcode = IJKL_OPCODE_JUMP;
        parse_operand(context, &instruction.destination);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 7. jump_if condicao, #rotulo; */
    if (match_keyword(context, "jump_if") || match_keyword(context, "saltarSe")) {
        instruction.opcode = IJKL_OPCODE_JUMP_IF;
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ",");
        parse_operand(context, &instruction.destination);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 8. call #rotulo; */
    if (match_keyword(context, "call") || match_keyword(context, "chamar")) {
        instruction.opcode = IJKL_OPCODE_CALL;
        parse_operand(context, &instruction.destination);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 9. push valor; */
    if (match_keyword(context, "push") || match_keyword(context, "empilhar")) {
        instruction.opcode = IJKL_OPCODE_PUSH;
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 10. pop destino; */
    if (match_keyword(context, "pop") || match_keyword(context, "desempilhar")) {
        instruction.opcode = IJKL_OPCODE_POP;
        parse_operand(context, &instruction.destination);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 11. in porta, destino; */
    if (match_keyword(context, "in") || match_keyword(context, "entrada")) {
        instruction.opcode = IJKL_OPCODE_IN;
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ",");
        parse_operand(context, &instruction.destination);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 12. out porta, valor; */
    if (match_keyword(context, "out") || match_keyword(context, "saida")) {
        instruction.opcode = IJKL_OPCODE_OUT;
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ",");
        parse_operand(context, &instruction.second_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 13. free(ponteiro) ou free ponteiro; */
    if (match_keyword(context, "free") || match_keyword(context, "desalocar")) {
        instruction.opcode = IJKL_OPCODE_FREE;
        skip_whitespace_and_comments(context);
        bool has_parentheses = match_exact(context, "(");
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        if (has_parentheses) {
            match_exact(context, ")");
            skip_whitespace_and_comments(context);
        }
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 14. error "mensagem"; */
    if (match_keyword(context, "error") || match_keyword(context, "erro")) {
        instruction.opcode = IJKL_OPCODE_ERROR;
        parse_operand(context, &instruction.first_operand);
        skip_whitespace_and_comments(context);
        match_exact(context, ";");
        append_instruction(context, &instruction);
        return true;
    }

    /* 15. Atribuição / Computação: destino := expressao; */
    if (parse_operand(context, &instruction.destination)) {
        skip_whitespace_and_comments(context);
        if (match_exact(context, ":=")) {
            skip_whitespace_and_comments(context);

            /* Caso especial: alloc(tamanho) ou alloc tamanho */
            if (match_keyword(context, "alloc") || match_keyword(context, "alocar")) {
                instruction.opcode = IJKL_OPCODE_ALLOC;
                skip_whitespace_and_comments(context);
                bool has_parentheses = match_exact(context, "(");
                parse_operand(context, &instruction.first_operand);
                skip_whitespace_and_comments(context);
                if (has_parentheses) {
                    match_exact(context, ")");
                    skip_whitespace_and_comments(context);
                }
                match_exact(context, ";");
                append_instruction(context, &instruction);
                return true;
            }

            /* Caso especial: not op */
            if (match_keyword(context, "not") || match_keyword(context, "nao")) {
                instruction.opcode = IJKL_OPCODE_LOGICAL_NOT;
                parse_operand(context, &instruction.first_operand);
                skip_whitespace_and_comments(context);
                match_exact(context, ";");
                append_instruction(context, &instruction);
                return true;
            }

            /* Caso especial: -op (unário negativo com espaço) */
            if (*context->current_pointer == '-' && !isdigit((unsigned char)*(context->current_pointer + 1))) {
                context->current_pointer++;
                instruction.opcode = IJKL_OPCODE_NEGATE;
                parse_operand(context, &instruction.first_operand);
                skip_whitespace_and_comments(context);
                match_exact(context, ";");
                append_instruction(context, &instruction);
                return true;
            }

            /* Primeiro operando do lado direito */
            if (!parse_operand(context, &instruction.first_operand)) {
                snprintf(context->diagnostic_error_message, sizeof(context->diagnostic_error_message),
                         "Expected right-hand expression after ':=' on line %zu.", context->current_line_number);
                return false;
            }

            skip_whitespace_and_comments(context);

            /* Verifica se há operador binário subsequente */
            IjklVmOpcode binary_op = IJKL_OPCODE_ASSIGN;
            if (match_exact(context, "<=")) binary_op = IJKL_OPCODE_LESS_EQUAL;
            else if (match_exact(context, ">=")) binary_op = IJKL_OPCODE_GREATER_EQUAL;
            else if (match_exact(context, "<>")) binary_op = IJKL_OPCODE_NOT_EQUAL;
            else if (match_exact(context, "<<")) binary_op = IJKL_OPCODE_SHIFT_LEFT;
            else if (match_exact(context, ">>")) binary_op = IJKL_OPCODE_SHIFT_RIGHT;
            else if (match_exact(context, "+"))  binary_op = IJKL_OPCODE_ADD;
            else if (match_exact(context, "-"))  binary_op = IJKL_OPCODE_SUB;
            else if (match_exact(context, "*"))  binary_op = IJKL_OPCODE_MUL;
            else if (match_exact(context, "/") || match_keyword(context, "div"))  binary_op = IJKL_OPCODE_DIV;
            else if (match_exact(context, "%") || match_keyword(context, "mod")) binary_op = IJKL_OPCODE_MOD;
            else if (match_exact(context, "^"))  binary_op = IJKL_OPCODE_EXP;
            else if (match_exact(context, "="))  binary_op = IJKL_OPCODE_EQUAL;
            else if (match_exact(context, "<"))  binary_op = IJKL_OPCODE_LESS;
            else if (match_exact(context, ">"))  binary_op = IJKL_OPCODE_GREATER;
            else if (match_keyword(context, "and") || match_keyword(context, "e")) binary_op = IJKL_OPCODE_LOGICAL_AND;
            else if (match_keyword(context, "or")  || match_keyword(context, "ou")) binary_op = IJKL_OPCODE_LOGICAL_OR;
            else if (match_keyword(context, "xor")) binary_op = IJKL_OPCODE_LOGICAL_XOR;
            else if (match_exact(context, "&"))  binary_op = IJKL_OPCODE_BITWISE_AND;
            else if (match_exact(context, "|"))  binary_op = IJKL_OPCODE_BITWISE_OR;

            if (binary_op != IJKL_OPCODE_ASSIGN) {
                instruction.opcode = binary_op;
                parse_operand(context, &instruction.second_operand);
            } else {
                instruction.opcode = IJKL_OPCODE_ASSIGN;
            }

            skip_whitespace_and_comments(context);
            match_exact(context, ";");
            append_instruction(context, &instruction);
            return true;
        }
    }

    /* Token desconhecido ou não identificado */
    context->current_pointer++;
    return true;
}

/*
 * EFEITO COLATERAL:
 * Segunda passagem (linking): resolve as referências de rótulos simbólicos
 * substituindo os nomes pelo índice numérico exato no vetor de instruções.
 */
static bool resolve_label_references(IjklCiLoaderContext* context, IjklCiProgram* output_program) {
    for (size_t i = 0; i < context->instructions_count; ++i) {
        IjklVmInstruction* inst = &context->instructions_array[i];

        /* Resolução no campo destination */
        if (inst->destination.kind == OPERAND_KIND_LABEL_REFERENCE &&
            !inst->destination.value.label_reference.is_resolved) {
            const char* target_name = inst->destination.value.label_reference.label_name;
            bool found = false;
            for (size_t l = 0; l < context->label_table_count; ++l) {
                if (ijkl_string_case_insensitive_equals(context->label_table[l].label_name, target_name)) {
                    inst->destination.value.label_reference.target_instruction_index = context->label_table[l].target_instruction_index;
                    inst->destination.value.label_reference.is_resolved = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (strncmp(target_name, "__builtin_", 10) == 0) {
                    inst->destination.value.label_reference.target_instruction_index = (size_t)-1;
                    inst->destination.value.label_reference.is_resolved = true;
                } else {
                    fprintf(stderr, "[Error] Line %zu: Unresolved label '#%s'.\n", inst->source_line_number, target_name);
                    return false;
                }
            }
        }

        /* Resolução no campo first_operand */
        if (inst->first_operand.kind == OPERAND_KIND_LABEL_REFERENCE &&
            !inst->first_operand.value.label_reference.is_resolved) {
            const char* target_name = inst->first_operand.value.label_reference.label_name;
            bool found = false;
            for (size_t l = 0; l < context->label_table_count; ++l) {
                if (ijkl_string_case_insensitive_equals(context->label_table[l].label_name, target_name)) {
                    inst->first_operand.value.label_reference.target_instruction_index = context->label_table[l].target_instruction_index;
                    inst->first_operand.value.label_reference.is_resolved = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (strncmp(target_name, "__builtin_", 10) == 0) {
                    inst->first_operand.value.label_reference.target_instruction_index = (size_t)-1;
                    inst->first_operand.value.label_reference.is_resolved = true;
                } else {
                    fprintf(stderr, "[Error] Line %zu: Unresolved label '#%s'.\n", inst->source_line_number, target_name);
                    return false;
                }
            }
        }
    }

    /* Identifica ponto de entrada (entry point): <NomeDoPrograma>.main, main (com fallback para .principal) */
    output_program->entry_point_instruction_index = 0;
    bool found_entry = false;
    for (size_t l = 0; l < context->label_table_count; ++l) {
        const char* name = context->label_table[l].label_name;
        size_t name_len = strlen(name);
        if (ijkl_string_case_insensitive_equals(name, "main") ||
            (name_len > 5 && ijkl_string_case_insensitive_equals(name + name_len - 5, ".main"))) {
            output_program->entry_point_instruction_index = context->label_table[l].target_instruction_index;
            output_program->program_name = name;
            found_entry = true;
            break;
        }
    }
    if (!found_entry) {
        for (size_t l = 0; l < context->label_table_count; ++l) {
            const char* name = context->label_table[l].label_name;
            size_t name_len = strlen(name);
            if (ijkl_string_case_insensitive_equals(name, "principal") ||
                (name_len > 10 && ijkl_string_case_insensitive_equals(name + name_len - 10, ".principal"))) {
                output_program->entry_point_instruction_index = context->label_table[l].target_instruction_index;
                output_program->program_name = name;
                found_entry = true;
                break;
            }
        }
    }

    output_program->instructions = context->instructions_array;
    output_program->instructions_count = context->instructions_count;
    return true;
}

/*
 * EFEITO COLATERAL:
 * Decodifica o código intermediário linear a partir de um buffer de texto em memória.
 */
bool ijkl_ci_loader_load_from_string(IjklMemoryArena* memory_arena,
                                     const char* source_code_text,
                                     const char* source_name_for_diagnostics,
                                     IjklCiProgram* output_program) {
    if (!memory_arena || !source_code_text || !output_program) return false;

    IjklCiLoaderContext context;
    memset(&context, 0, sizeof(IjklCiLoaderContext));
    context.memory_arena = memory_arena;
    context.source_text = source_code_text;
    context.current_pointer = source_code_text;
    context.current_line_number = 1;
    context.instructions_capacity = IJKL_INITIAL_INSTRUCTIONS_CAPACITY;
    context.instructions_array = (IjklVmInstruction*)ijkl_memory_arena_allocate(
        memory_arena, sizeof(IjklVmInstruction) * context.instructions_capacity);

    /* Passagem 1: Decodificação léxica/sintática e registro dos rótulos */
    while (*context.current_pointer != '\0') {
        skip_whitespace_and_comments(&context);
        if (*context.current_pointer == '\0') break;

        if (!parse_ci_statement(&context)) {
            fprintf(stderr, "[Error] [%s:%zu] %s\n",
                    source_name_for_diagnostics ? source_name_for_diagnostics : "CI",
                    context.current_line_number, context.diagnostic_error_message);
            return false;
        }
    }

    /* Passagem 2: Resolução de rótulos simbólicos */
    return resolve_label_references(&context, output_program);
}

/*
 * EFEITO COLATERAL:
 * Carrega e decodifica o arquivo .ci diretamente a partir do sistema de arquivos.
 */
bool ijkl_ci_loader_load_from_file(IjklMemoryArena* memory_arena,
                                   const char* file_path,
                                   IjklCiProgram* output_program) {
    if (!memory_arena || !file_path || !output_program) return false;

    FILE* file_handle = fopen(file_path, "rb");
    if (!file_handle) {
        fprintf(stderr, "[Error] Could not open intermediate code file '%s'.\n", file_path);
        return false;
    }

    fseek(file_handle, 0, SEEK_END);
    long file_size = ftell(file_handle);
    fseek(file_handle, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file_handle);
        return false;
    }

    char* source_buffer = (char*)ijkl_memory_arena_allocate(memory_arena, (size_t)file_size + 1);
    size_t read_bytes = fread(source_buffer, 1, (size_t)file_size, file_handle);
    source_buffer[read_bytes] = '\0';
    fclose(file_handle);

    return ijkl_ci_loader_load_from_string(memory_arena, source_buffer, file_path, output_program);
}
