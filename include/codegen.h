#ifndef IJKL_CODEGEN_H
#define IJKL_CODEGEN_H

#include "common.h"
#include "memory.h"
#include "ast.h"
#include "symtab.h"
#include <stdio.h>

/*
 * ==============================================================================
 * MÓDULO: GERADOR DE CÓDIGO INTERMEDIÁRIO (.CI) DA IJKLANG
 * ==============================================================================
 * Converte a Árvore Sintática Abstrata (AST) tipada e semanticamente validada em
 * uma sequência linear de instruções da linguagem intermediária (.ci), conforme
 * a especificação normativa formal em Codigo_Intermediario_CI.md.
 *
 * Suporta:
 * - Emissão de rótulos simbólicos para módulos, funções e pontos de entrada.
 * - Gerenciamento formal de quadros de ativação (alloc_frame <tamanho> e free_frame).
 * - Linearização de expressões aritmético-lógicas com uso de registradores.
 * - Mapeamento de estruturas de decisão e repetição para saltos condicionais.
 * - Emissão de chamadas de funções com passagem de argumentos na pilha.
 * - Instruções de barramento de E/S para consoles e periféricos didáticos (in/out).
 * ==============================================================================
 */

#define IJKL_CODEGEN_MAX_BREAK_TARGETS 64

typedef struct IjklCodeGenerator {
    IjklMemoryArena* memory_arena;
    IjklSymbolTable* symbol_table;
    FILE* output_stream;

    size_t label_counter;
    size_t temporary_register_index;
    size_t total_emitted_instructions_count;
    size_t current_spill_offset;
    size_t current_spill_depth;
    bool use_native_builtins;
    const char* current_routine_name;
    const char* current_module_path;
    const char* current_program_name;
    IjklScope* current_scope;
    const char* break_target_label_stack[IJKL_CODEGEN_MAX_BREAK_TARGETS];
    size_t break_target_label_depth;
} IjklCodeGenerator;

/* Criação do gerador de código intermediário */
IjklCodeGenerator* ijkl_code_generator_create(IjklMemoryArena* memory_arena,
                                             IjklSymbolTable* symbol_table,
                                             FILE* output_stream);

/* Configura se as rotinas built-in serão omitidas do .ci para resolução nativa pela VM */
void ijkl_code_generator_set_native_builtins(IjklCodeGenerator* code_generator,
                                             bool use_native_builtins);

/*
 * EFEITO COLATERAL:
 * Percorre a AST e emite todo o código intermediário linear (*.ci) para o fluxo de saída.
 */
bool ijkl_code_generator_generate(IjklCodeGenerator* code_generator,
                                  const IjklAstNode* compilation_unit_root);

/* Emissão direta para arquivo em disco com manipulação segura de I/O */
bool ijkl_code_generator_generate_to_file(IjklMemoryArena* memory_arena,
                                         IjklSymbolTable* symbol_table,
                                         const IjklAstNode* compilation_unit_root,
                                         const char* output_file_path,
                                         bool use_native_builtins);

#endif /* IJKL_CODEGEN_H */
