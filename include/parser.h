#ifndef IJKL_PARSER_H
#define IJKL_PARSER_H

#include "common.h"
#include "memory.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"

/*
 * ==============================================================================
 * MÓDULO: ANALISADOR SINTÁTICO (PARSER) DA IJKLANG
 * ==============================================================================
 * Implementa o analisador sintático descendente recursivo em ISO C11, operando
 * sobre o fluxo de tokens para construir a Árvore Sintática Abstrata (AST).
 *
 * Siglas técnicas empregadas neste módulo:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

typedef struct IjklParser IjklParser;

/*
 * Cria uma nova instância do analisador sintático.
 *
 * Parâmetros:
 * - lexer_instance: Instância ativa do analisador léxico.
 * - memory_arena: Arena de memória onde os nós e strings da AST serão alocados.
 * - source_file_path: Caminho do arquivo fonte para relatórios de erro sintático.
 */
IjklParser* ijkl_parser_create(IjklLexer* lexer_instance,
                               IjklMemoryArena* memory_arena,
                               const char* source_file_path);

/* Destrói a estrutura do analisador sintático */
void ijkl_parser_destroy(IjklParser* parser);

/*
 * Realiza o parsing completo da unidade de compilação (programa ou módulo).
 * Retorna o nó raiz da AST (AST_NODE_PROGRAM ou AST_NODE_MODULE) ou NULL em caso de erro fatal.
 */
IjklAstNode* ijkl_parser_parse_compilation_unit(IjklParser* parser);

/* Retorna a quantidade de erros sintáticos detectados durante o parsing */
size_t ijkl_parser_get_error_count(const IjklParser* parser);

#endif /* IJKL_PARSER_H */
