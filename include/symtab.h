#ifndef IJKL_SYMTAB_H
#define IJKL_SYMTAB_H

#include "common.h"
#include "memory.h"
#include "ast.h"

/*
 * ==============================================================================
 * MÓDULO: TABELA DE SÍMBOLOS E ESCOPOS (SYMTAB) DA IJKLANG
 * ==============================================================================
 * Gerencia a hierarquia encadeada de escopos (global, módulos, rotinas e blocos),
 * o registro de identificadores declarados e a resolução de nomes (lexical lookup,
 * shadowing e qualified access com notação ponto).
 *
 * Siglas técnicas empregadas:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

#define IJKL_SYMTAB_HASH_CAPACITY 64

/* Classificação do tipo de símbolo registrado */
typedef enum IjklSymbolKind {
    SYMBOL_KIND_VARIABLE,
    SYMBOL_KIND_CONSTANT,
    SYMBOL_KIND_TYPE,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_PROCEDURE,
    SYMBOL_KIND_PARAMETER,
    SYMBOL_KIND_MODULE
} IjklSymbolKind;

/* Classificação do tipo de escopo */
typedef enum IjklScopeKind {
    SCOPE_KIND_GLOBAL,
    SCOPE_KIND_MODULE,
    SCOPE_KIND_ROUTINE,
    SCOPE_KIND_BLOCK
} IjklScopeKind;

struct IjklScope;

/* Estrutura que representa um símbolo declarado */
typedef struct IjklSymbol {
    const char* identifier_name;
    const char* normalized_identifier_name;
    IjklSymbolKind symbol_kind;
    bool is_public;
    bool is_by_reference;
    IjklTypeSpec* type_specification;
    IjklAstNode* declaration_node;

    size_t line_number;
    size_t column_number;
    size_t memory_offset_in_bytes;
    size_t memory_size_in_bytes;

    const char* mangled_identifier_name;
    bool is_overloaded;

    struct IjklScope* internal_scope;
    struct IjklScope* enclosing_scope;

    struct IjklSymbol* next_in_hash_bucket;
    struct IjklSymbol* next_in_declaration_order;
    struct IjklSymbol* next_overload;
} IjklSymbol;

/* Estrutura que representa um nível de escopo encadeado */
typedef struct IjklScope {
    IjklScopeKind scope_kind;
    const char* scope_name;
    size_t nesting_depth_level;

    struct IjklScope* parent_scope;
    struct IjklScope* first_child_scope;
    struct IjklScope* next_sibling_scope;

    IjklSymbol* symbol_hash_buckets[IJKL_SYMTAB_HASH_CAPACITY];
    IjklSymbol* first_symbol_in_declaration_order;
    IjklSymbol* last_symbol_in_declaration_order;
    size_t total_symbol_count;
    size_t current_memory_offset;
} IjklScope;

struct IjklVocabulary;

/* Estrutura raiz de controle da Tabela de Símbolos */
typedef struct IjklSymbolTable {
    IjklMemoryArena* memory_arena;
    IjklScope* root_global_scope;
    IjklScope* current_active_scope;
    const struct IjklAstNode* active_compilation_unit;
    struct IjklVocabulary* active_vocabulary;
    size_t total_scopes_count;
    size_t total_errors_count;
} IjklSymbolTable;

/* --- Criação e Gerenciamento da Tabela e Escopos --- */

IjklSymbolTable* ijkl_symbol_table_create(IjklMemoryArena* memory_arena);

void ijkl_symbol_table_set_vocabulary(IjklSymbolTable* symbol_table, struct IjklVocabulary* vocabulary);

void ijkl_symbol_table_set_active_compilation_unit(IjklSymbolTable* symbol_table,
                                                   const struct IjklAstNode* unit_node);

IjklScope* ijkl_symbol_table_enter_scope(IjklSymbolTable* symbol_table,
                                        IjklScopeKind scope_kind,
                                        const char* scope_name);

IjklScope* ijkl_symbol_table_exit_scope(IjklSymbolTable* symbol_table);

IjklScope* ijkl_symbol_table_current_scope(const IjklSymbolTable* symbol_table);

IjklScope* ijkl_symbol_table_root_scope(const IjklSymbolTable* symbol_table);

/* --- Criação e Registro de Símbolos --- */

IjklSymbol* ijkl_symbol_create(IjklMemoryArena* memory_arena,
                               IjklSymbolKind symbol_kind,
                               const char* identifier_name,
                               IjklTypeSpec* type_specification,
                               IjklAstNode* declaration_node,
                               size_t line_number,
                               size_t column_number);

/*
 * EFEITO COLATERAL:
 * Registra o símbolo no escopo ativo da tabela. Caso já exista identificador idêntico
 * no mesmo nível de escopo (comparação case-insensitive), emite erro e retorna falso.
 */
bool ijkl_symbol_table_define(IjklSymbolTable* symbol_table, IjklSymbol* symbol);

/* --- Mecanismos de Busca de Símbolos (Lookups) --- */

/* Busca restrita ao escopo indicado sem consultar pais */
IjklSymbol* ijkl_scope_lookup_direct(const IjklScope* scope, const char* identifier_name);

/* Busca no escopo ativo sem subir para pais */
IjklSymbol* ijkl_symbol_table_lookup_current_scope(const IjklSymbolTable* symbol_table,
                                                  const char* identifier_name);

/* Verifica se um identificador corresponde a uma rotina nativa/built-in registrada */
bool ijkl_symbol_table_is_known_builtin(const IjklSymbolTable* symbol_table,
                                       const char* identifier_name);

/* Busca léxica com encadeamento estático ascendente (suporte a sombreamento / shadowing) */
IjklSymbol* ijkl_symbol_table_lookup_lexical(const IjklSymbolTable* symbol_table,
                                            const char* identifier_name);

/* Busca qualificada com notação de ponto (ex.: 'GeometriaApp.Calculos.PI') */
IjklSymbol* ijkl_symbol_table_lookup_qualified(const IjklSymbolTable* symbol_table,
                                              const char* qualified_path);

/* Calcula o tamanho em bytes de um tipo para estimativa de layout de memória */
size_t ijkl_symbol_table_calculate_type_size(const IjklTypeSpec* type_specification,
                                            const IjklSymbolTable* symbol_table);

/* --- Construção Semântica da Tabela a partir da AST --- */

/*
 * EFEITO COLATERAL:
 * Percorre recursivamente a Árvore Sintática Abstrata (AST), criando os escopos
 * correspondentes e registrando todos os símbolos declarados com validação semântica.
 */
bool ijkl_symbol_table_build_from_ast(IjklSymbolTable* symbol_table,
                                     const IjklAstNode* compilation_unit_root);

/* Retorna a contagem total de erros semânticos detectados durante a montagem da tabela */
size_t ijkl_symbol_table_get_error_count(const IjklSymbolTable* symbol_table);

/* --- Formatação e Exibição Diagnóstica --- */

const char* ijkl_symbol_kind_name(IjklSymbolKind symbol_kind);

const char* ijkl_scope_kind_name(IjklScopeKind scope_kind);

/* Imprime a representação hierárquica e detalhada de todos os escopos e símbolos */
void ijkl_symbol_table_print(const IjklSymbolTable* symbol_table, FILE* output_stream);

/* --- Sobrecarga de Rotinas e Name Mangling --- */

/* Gera o identificador mangled determinístico para rotinas */
const char* ijkl_symbol_generate_mangled_name(IjklMemoryArena* memory_arena,
                                              const char* routine_name,
                                              const struct IjklAstNode* parameters_node);

/* Verifica se duas listas de parâmetros de rotinas possuem tipos estritamente idênticos */
bool ijkl_symbol_are_parameter_types_identical(const struct IjklAstNode* parameters_a,
                                               const struct IjklAstNode* parameters_b);

/* Resolução de sobrecarga de rotina com base na lista e tipos inferidos de argumentos */
IjklSymbol* ijkl_symbol_resolve_overload(IjklSymbol* routine_symbol_head,
                                        struct IjklTypeSpec** argument_types,
                                        size_t argument_count,
                                        bool* is_ambiguous);

/* --- Tipos Encapsulados (Boxed Types) --- */

/* Verifica se um especificador de tipo corresponde a um Boxed Type embutido */
bool ijkl_type_is_boxed_primitive(const IjklTypeSpec* type_specification,
                                  IjklPrimitiveType* out_primitive_type);

#endif /* IJKL_SYMTAB_H */

