#ifndef IJKL_LEXER_H
#define IJKL_LEXER_H

#include "common.h"
#include "memory.h"
#include "utf8.h"
#include "token.h"
#include "vocabulary.h"

/*
 * ==============================================================================
 * MÓDULO: ANALISADOR LÉXICO (SCANNER / LEXER) DA IJKLANG
 * ==============================================================================
 * Implementa o Autômato Finito Determinístico (AFD) para conversão de fluxo de
 * caracteres UTF-8 em uma sequência de tokens tipados e normalizados.
 * ==============================================================================
 */

/* Tipo opaco representativo do estado interno do analisador léxico */
typedef struct IjklLexer IjklLexer;

/*
 * Cria uma nova instância do analisador léxico.
 *
 * Parâmetros:
 * - source_code: Fluxo de texto com o código-fonte em formato UTF-8 (não toma posse do buffer).
 * - source_length_in_bytes: Comprimento total do texto em bytes.
 * - vocabulary: Dicionário de palavras reservadas e alfabeto complementar do idioma.
 * - memory_arena: Arena de memória utilizada para alocar os lexemas e strings de tokens.
 * - source_file_name: Nome do arquivo fonte para mensagens de diagnóstico e rastreabilidade.
 */
IjklLexer* ijkl_lexer_create(const char* source_code,
                             size_t source_length_in_bytes,
                             const IjklVocabulary* vocabulary,
                             IjklMemoryArena* memory_arena,
                             const char* source_file_name);

/* Libera a instância do analisador léxico (a memória dos tokens na Arena é mantida) */
void ijkl_lexer_destroy(IjklLexer* lexer);

/*
 * Extrai e consome o próximo token do fluxo de código-fonte.
 *
 * EFEITO COLATERAL / MUTAÇÃO:
 * Avança o cursor interno do analisador léxico e atualiza os contadores de linha e coluna.
 */
IjklToken ijkl_lexer_next_token(IjklLexer* lexer);

/*
 * Examina o próximo token do fluxo de código-fonte sem consumi-lo (Lookahead 1).
 */
IjklToken ijkl_lexer_peek_token(IjklLexer* lexer);

/*
 * Processa todo o código-fonte e imprime os tokens gerados no fluxo de saída especificado.
 * Retorna true se nenhum erro léxico foi encontrado, ou false caso contrário.
 */
bool ijkl_lexer_dump_tokens(const char* source_code,
                            size_t source_length_in_bytes,
                            const IjklVocabulary* vocabulary,
                            IjklMemoryArena* memory_arena,
                            const char* source_file_name,
                            FILE* output_stream);

/* Retorna o vocabulário vinculado a este analisador léxico */
const IjklVocabulary* ijkl_lexer_get_vocabulary(const IjklLexer* lexer);

/*
 * Captura o conteúdo textual bruto de um bloco delimitado por chaves até a chave correspondente '}'.
 * Lida com comentários, strings literais e chaves aninhadas.
 * Retorna o buffer alocado na arena do lexer e o tamanho em out_length.
 */
char* ijkl_lexer_scan_raw_block(IjklLexer* lexer, size_t* out_length);

#endif /* IJKL_LEXER_H */
