#ifndef IJKL_SEMANTIC_H
#define IJKL_SEMANTIC_H

#include "common.h"
#include "memory.h"
#include "ast.h"
#include "symtab.h"

/*
 * ==============================================================================
 * MÓDULO: ANALISADOR SEMÂNTICO E VERIFICADOR DE TIPOS DA IJKLANG
 * ==============================================================================
 * Realiza a validação semântica e verificação estática de tipos sobre a Árvore
 * Sintática Abstrata (AST):
 * - Resolução de identificadores e consistência de escopos léxicos.
 * - Inferência e compatibilidade estrita de tipos em expressões e atribuições.
 * - Validação de referências de l-values, operadores de memória (&, @, $) e arranjos.
 * - Verificação de assinaturas de rotinas (número e tipos de argumentos e retornos).
 * - Imposição de restrições gramaticais (proibição de atribuição em condicionais,
 *   posicionamento válido de comandos de desvio como 'aborte').
 * ==============================================================================
 */

struct IjklVocabulary;

typedef struct IjklSemanticAnalyzer {
    IjklMemoryArena* memory_arena;
    IjklSymbolTable* symbol_table;
    const struct IjklVocabulary* vocabulary;
    size_t total_errors_count;
    size_t total_warnings_count;

    size_t loop_nesting_depth;
    size_t switch_nesting_depth;
    const IjklAstNode* current_routine_node;
} IjklSemanticAnalyzer;

/* Criação do analisador semântico associado à tabela de símbolos */
IjklSemanticAnalyzer* ijkl_semantic_analyzer_create(IjklMemoryArena* memory_arena,
                                                   IjklSymbolTable* symbol_table);

void ijkl_semantic_analyzer_set_vocabulary(IjklSemanticAnalyzer* analyzer,
                                           const struct IjklVocabulary* vocabulary);

/*
 * EFEITO COLATERAL:
 * Percorre recursivamente a AST, validando as regras semânticas de tipos e escopo.
 * Retorna true caso nenhum erro semântico seja detectado, ou false caso haja erros.
 */
bool ijkl_semantic_analyzer_analyze(IjklSemanticAnalyzer* analyzer,
                                   const IjklAstNode* compilation_unit_root);

/* Inferência estática de tipo de uma expressão arbitrária na AST */
IjklTypeSpec* ijkl_semantic_analyze_expression(IjklSemanticAnalyzer* analyzer,
                                              const IjklAstNode* expression_node);

/* Verifica a compatibilidade semântica entre tipos para atribuição e passagem de parâmetros */
bool ijkl_semantic_are_types_compatible(const IjklTypeSpec* target_type,
                                       const IjklTypeSpec* source_type);

/* Retorna a contagem total de erros e alertas semânticos detectados */
size_t ijkl_semantic_analyzer_get_error_count(const IjklSemanticAnalyzer* analyzer);
size_t ijkl_semantic_analyzer_get_warning_count(const IjklSemanticAnalyzer* analyzer);

#endif /* IJKL_SEMANTIC_H */
