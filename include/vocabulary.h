#ifndef IJKL_VOCABULARY_H
#define IJKL_VOCABULARY_H

#include "common.h"
#include "memory.h"
#include "utf8.h"
#include "token.h"

/*
 * ==============================================================================
 * MÓDULO: VOCABULÁRIO E ALFABETO COMPLEMENTAR DA IJKLANG
 * ==============================================================================
 * Gerencia o dicionário de palavras reservadas e o mapeamento de caracteres
 * complementares (ex.: acentuação de idiomas como pt-BR para base ASCII)
 * carregados a partir dos arquivos de extensão .words.
 * ==============================================================================
 */

/* Entrada na tabela de dispersão do alfabeto complementar */
typedef struct IjklCharMapEntry {
    uint32_t unicode_codepoint;
    char ascii_base_character;
    struct IjklCharMapEntry* next_entry;
} IjklCharMapEntry;

/* Entrada na tabela de dispersão de palavras reservadas */
typedef struct IjklKeywordEntry {
    const char* normalized_word;
    IjklTokenType token_type;
    struct IjklKeywordEntry* next_entry;
} IjklKeywordEntry;

/* Entrada na tabela de dispersão de alias do framework (unidades e membros) */
typedef struct IjklFrameworkAliasEntry {
    const char* source_alias_name;
    const char* target_canonical_name;
    struct IjklFrameworkAliasEntry* next_entry;
} IjklFrameworkAliasEntry;

/* Tabela e repositório de vocabulário do idioma ativo */
typedef struct IjklVocabulary {
    IjklMemoryArena* memory_arena;
    IjklCharMapEntry* complementary_character_map[256];
    IjklKeywordEntry* reserved_words_map[256];
    IjklFrameworkAliasEntry* framework_units_map[256];
    IjklFrameworkAliasEntry* framework_members_map[256];
    const char* entry_point_name;
    const char* initialize_routine_name;
    const char* finalize_routine_name;
    const char* active_language_code;
    size_t total_complementary_characters;
    size_t total_reserved_words;
    size_t total_framework_units_count;
    size_t total_framework_members_count;
} IjklVocabulary;

/* Cria e inicializa um novo repositório de vocabulário vinculado à Arena de Memória */
IjklVocabulary* ijkl_vocabulary_create(IjklMemoryArena* memory_arena);

/* Carrega definições de [complementary_alphabet], [reserved_words], [framework_units] e [framework_members] de um arquivo .words */
bool ijkl_vocabulary_load_file(IjklVocabulary* vocabulary, const char* words_file_path);

/* Registra um mapeamento de caractere acentuado/especial para sua letra base ASCII */
void ijkl_vocabulary_add_complementary_char(IjklVocabulary* vocabulary, uint32_t unicode_codepoint, char ascii_base_character);

/* Registra uma palavra-chave normalizada e seu respectivo IjklTokenType */
void ijkl_vocabulary_add_reserved_word(IjklVocabulary* vocabulary, const char* normalized_word, IjklTokenType token_type);

/* Registra um mapeamento de unidade do framework (ex.: ijkL.Conversao -> ijkL.Convert) */
void ijkl_vocabulary_add_framework_unit(IjklVocabulary* vocabulary,
                                       const char* source_alias_name,
                                       const char* target_canonical_name);

/* Resolve o nome canônico de uma unidade do framework a partir de um alias */
const char* ijkl_vocabulary_resolve_framework_unit(const IjklVocabulary* vocabulary,
                                                  const char* candidate_unit_name);

/* Registra um mapeamento de membro/rotina do framework (ex.: paraInteiro -> toInt) */
void ijkl_vocabulary_add_framework_member(IjklVocabulary* vocabulary,
                                         const char* source_alias_name,
                                         const char* target_canonical_name);

/* Resolve o nome canônico de um membro/rotina do framework a partir de um alias */
const char* ijkl_vocabulary_resolve_framework_member(const IjklVocabulary* vocabulary,
                                                    const char* candidate_member_name);

/*
 * Normaliza um codepoint Unicode: se pertencer a A-Z converte para a-z; se estiver
 * registrado no alfabeto complementar retorna a letra base ASCII; caso contrário
 * retorna '\0' indicando que o caractere não possui equivalente direto no alfabeto.
 */
char ijkl_vocabulary_map_codepoint_to_ascii(const IjklVocabulary* vocabulary, uint32_t unicode_codepoint);

/*
 * Converte e normaliza uma cadeia UTF-8 (substituindo acentuações e maiúsculas),
 * alocando a string resultante na Arena de Memória fornecida.
 */
char* ijkl_vocabulary_normalize_string(const IjklVocabulary* vocabulary,
                                      IjklMemoryArena* destination_arena,
                                      const char* source_string,
                                      size_t source_length);

/*
 * Consulta uma palavra no dicionário de palavras reservadas.
 * Retorna true e atribui 'out_token_type' se for encontrada, ou false caso contrário.
 */
bool ijkl_vocabulary_lookup_keyword(const IjklVocabulary* vocabulary,
                                   const char* normalized_lexeme,
                                   IjklTokenType* out_token_type);

/* Converte o nome canônico em string (ex.: "KW_IF") para o respectivo IjklTokenType */
IjklTokenType ijkl_vocabulary_find_canonical_keyword_token(const char* canonical_keyword_name);

/* Retorna o nome da rotina de ponto de entrada mapeado pelo vocabulário (ex.: "principal" ou "main") */
const char* ijkl_vocabulary_get_entry_point_name(const IjklVocabulary* vocabulary);

/* Retorna o nome da rotina de inicialização de ciclo de vida (ex.: "inicializar" ou "initialize") */
const char* ijkl_vocabulary_get_initialize_name(const IjklVocabulary* vocabulary);

/* Retorna o nome da rotina de finalização de ciclo de vida (ex.: "finalizar" ou "finalize") */
const char* ijkl_vocabulary_get_finalize_name(const IjklVocabulary* vocabulary);

/* Retorna a representação textual da palavra-chave no vocabulário ativo para o token fornecido */
const char* ijkl_vocabulary_get_keyword_word(const IjklVocabulary* vocabulary, IjklTokenType token_type);

/*
 * Verifica se a palavra corresponde a um delimitador ou palavra obsoleta (ex.: inicio, fim, entao, fimse).
 */
bool ijkl_vocabulary_is_obsolete_keyword(const char* candidate_word,
                                         char* out_obsolete_info,
                                         size_t max_info_length);

#endif /* IJKL_VOCABULARY_H */
