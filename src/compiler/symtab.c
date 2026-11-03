#include "symtab.h"
#include "semantic.h"
#include "vocabulary.h"
#include <ctype.h>

/*
 * ==============================================================================
 * MÓDULO: TABELA DE SÍMBOLOS E ESCOPOS (SYMTAB) DA IJKLANG
 * ==============================================================================
 * Implementa a estrutura encadeada de escopos em ISO C11.
 * Oferece suporte a:
 * - Escopos léxicos aninhados (Global -> Módulos -> Rotinas -> Blocos).
 * - Resolução estática de identificadores com sombreamento (shadowing).
 * - Resolução qualificada para submódulos pontuados (ex.: 'GeometriaApp.Calculos').
 * - Rastreamento de tipos, parâmetros, constantes e variáveis com cálculo de bytes.
 * - Gerenciamento de memória sem vazamentos por intermédio de IjklMemoryArena.
 *
 * Siglas técnicas empregadas:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

/* Função de dispersão DJB2 para identificadores sensíveis a maiúsculas e minúsculas */
static size_t calculate_identifier_hash(const char* identifier_name) {
    size_t hash_value = 5381;
    if (!identifier_name) return 0;
    while (*identifier_name) {
        unsigned char current_character = (unsigned char)*identifier_name;
        hash_value = ((hash_value << 5) + hash_value) + current_character;
        identifier_name++;
    }
    return hash_value % IJKL_SYMTAB_HASH_CAPACITY;
}

/* Cria uma cópia da string alocada na arena de memória */
static const char* duplicate_string_in_arena(IjklMemoryArena* memory_arena, const char* source_string) {
    if (!source_string) return NULL;
    return ijkl_memory_arena_duplicate_string(memory_arena, source_string);
}

/* Comparação exata de identificadores (case-sensitive) */
static bool are_identifiers_equal(const char* first_identifier, const char* second_identifier) {
    if (!first_identifier || !second_identifier) return false;
    return strcmp(first_identifier, second_identifier) == 0;
}

/* Calcula o tamanho em bytes de um tipo para estimativa de layout de memória */
size_t ijkl_symbol_table_calculate_type_size(const IjklTypeSpec* type_specification, const IjklSymbolTable* symbol_table) {
    if (!type_specification) return 8;

    switch (type_specification->kind) {
        case TYPE_KIND_PRIMITIVE:
            switch (type_specification->details.primitive_type) {
                case PRIMITIVE_INT: return 8;
                case PRIMITIVE_REAL: return 8;
                case PRIMITIVE_FLOAT: return 8;
                case PRIMITIVE_CHAR: return 8;
                case PRIMITIVE_STRING: return 8;
                case PRIMITIVE_BOOL: return 8;
                default: return 8;
            }
        case TYPE_KIND_POINTER:
            return 8; /* Endereço de memória de 64 bits */
        case TYPE_KIND_ARRAY: {
            size_t element_size = ijkl_symbol_table_calculate_type_size(type_specification->details.array_info.element_type, symbol_table);
            return type_specification->details.array_info.fixed_element_count * element_size;
        }
        case TYPE_KIND_STRUCT: {
            size_t total_structure_size = 0;
            const IjklAstNode* current_field = type_specification->details.struct_info.field_declarations;
            while (current_field) {
                if (current_field->kind == AST_NODE_PARAMETER) {
                    size_t field_size = ijkl_symbol_table_calculate_type_size(current_field->data.parameter.type_specification, symbol_table);
                    total_structure_size += field_size;
                } else if (current_field->kind == AST_NODE_DECL_VAR) {
                    size_t field_size = ijkl_symbol_table_calculate_type_size(current_field->data.variable_declaration.type_specification, symbol_table);
                    total_structure_size += field_size;
                }
                current_field = current_field->next_sibling;
            }
            return total_structure_size > 0 ? total_structure_size : 8;
        }
        case TYPE_KIND_USER_DEFINED: {
            if (symbol_table && type_specification->details.user_type_name) {
                IjklSymbol* type_symbol = ijkl_symbol_table_lookup_lexical(symbol_table, type_specification->details.user_type_name);
                if (type_symbol && type_symbol->symbol_kind == SYMBOL_KIND_TYPE && type_symbol->type_specification) {
                    return ijkl_symbol_table_calculate_type_size(type_symbol->type_specification, symbol_table);
                }
            }
            return 8;
        }
        default:
            return 8;
    }
}

/* --- Implementação da API de Escopos e Tabela de Símbolos --- */

IjklSymbol* ijkl_symbol_create(IjklMemoryArena* memory_arena,
                               IjklSymbolKind symbol_kind,
                               const char* identifier_name,
                               IjklTypeSpec* type_specification,
                               IjklAstNode* declaration_node,
                               size_t line_number,
                               size_t column_number) {
    IjklSymbol* symbol = (IjklSymbol*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklSymbol));
    memset(symbol, 0, sizeof(IjklSymbol));

    symbol->identifier_name = ijkl_memory_arena_duplicate_string(memory_arena, identifier_name);
    symbol->normalized_identifier_name = duplicate_string_in_arena(memory_arena, identifier_name);
    symbol->mangled_identifier_name = NULL;
    symbol->is_overloaded = false;
    symbol->symbol_kind = symbol_kind;
    symbol->type_specification = type_specification;
    symbol->declaration_node = declaration_node;
    symbol->line_number = line_number;
    symbol->column_number = column_number;
    symbol->memory_size_in_bytes = ijkl_symbol_table_calculate_type_size(type_specification, NULL);
    symbol->memory_offset_in_bytes = 0;
    symbol->internal_scope = NULL;
    symbol->enclosing_scope = NULL;
    symbol->next_in_hash_bucket = NULL;
    symbol->next_in_declaration_order = NULL;
    symbol->next_overload = NULL;

    return symbol;
}

static IjklScope* scope_create(IjklMemoryArena* memory_arena,
                               IjklScopeKind scope_kind,
                               const char* scope_name,
                               IjklScope* parent_scope) {
    IjklScope* scope = (IjklScope*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklScope));
    memset(scope, 0, sizeof(IjklScope));

    scope->scope_kind = scope_kind;
    scope->scope_name = ijkl_memory_arena_duplicate_string(memory_arena, scope_name ? scope_name : "UnnamedScope");
    scope->parent_scope = parent_scope;
    scope->nesting_depth_level = parent_scope ? parent_scope->nesting_depth_level + 1 : 0;
    scope->first_child_scope = NULL;
    scope->next_sibling_scope = NULL;
    scope->first_symbol_in_declaration_order = NULL;
    scope->last_symbol_in_declaration_order = NULL;
    scope->total_symbol_count = 0;
    scope->current_memory_offset = (parent_scope && scope_kind == SCOPE_KIND_BLOCK) ? parent_scope->current_memory_offset : 0;

    return scope;
}

static void register_single_builtin_function(IjklSymbolTable* symbol_table,
                                            const char* function_name,
                                            IjklPrimitiveType return_primitive_type,
                                            size_t parameter_count,
                                            const IjklPrimitiveType* parameter_primitive_types) {
    IjklMemoryArena* arena = symbol_table->memory_arena;

    IjklAstNode* routine_node = ijkl_ast_node_create(arena, AST_NODE_DECL_ROUTINE, 0, 0);
    routine_node->data.routine_declaration.routine_name = function_name;
    routine_node->data.routine_declaration.is_function = true;
    routine_node->data.routine_declaration.is_public = true;

    IjklTypeSpec* return_type_specification = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    return_type_specification->details.primitive_type = return_primitive_type;
    routine_node->data.routine_declaration.return_type_spec = return_type_specification;

    IjklAstNode* first_parameter_node = NULL;
    IjklAstNode* last_parameter_node = NULL;
    for (size_t parameter_index = 0; parameter_index < parameter_count; parameter_index++) {
        IjklAstNode* parameter_node = ijkl_ast_node_create(arena, AST_NODE_PARAMETER, 0, 0);
        IjklTypeSpec* parameter_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
        parameter_type->details.primitive_type = parameter_primitive_types[parameter_index];
        parameter_node->data.parameter.type_specification = parameter_type;
        parameter_node->data.parameter.parameter_name = "argument";

        if (!first_parameter_node) {
            first_parameter_node = parameter_node;
            last_parameter_node = parameter_node;
        } else {
            last_parameter_node->next_sibling = parameter_node;
            last_parameter_node = parameter_node;
        }
    }
    routine_node->data.routine_declaration.parameters = first_parameter_node;

    IjklSymbol* symbol = ijkl_symbol_create(arena,
                                           SYMBOL_KIND_FUNCTION,
                                           function_name,
                                           return_type_specification,
                                           routine_node,
                                           0,
                                           0);
    symbol->is_public = true;
    symbol->mangled_identifier_name = function_name;
    ijkl_symbol_table_define(symbol_table, symbol);
}

static void register_single_builtin_routine_with_types(IjklSymbolTable* symbol_table,
                                                       const char* routine_name,
                                                       IjklTypeSpec* return_type_spec,
                                                       size_t parameter_count,
                                                       IjklTypeSpec** parameter_types) {
    IjklMemoryArena* arena = symbol_table->memory_arena;
    IjklAstNode* routine_node = ijkl_ast_node_create(arena, AST_NODE_DECL_ROUTINE, 0, 0);
    routine_node->data.routine_declaration.routine_name = routine_name;
    routine_node->data.routine_declaration.is_public = true;
    routine_node->data.routine_declaration.return_type_spec = return_type_spec;

    IjklAstNode* first_parameter_node = NULL;
    IjklAstNode* last_parameter_node = NULL;
    for (size_t parameter_index = 0; parameter_index < parameter_count; parameter_index++) {
        IjklAstNode* parameter_node = ijkl_ast_node_create(arena, AST_NODE_PARAMETER, 0, 0);
        parameter_node->data.parameter.type_specification = parameter_types[parameter_index];
        parameter_node->data.parameter.parameter_name = "argument";

        if (!first_parameter_node) {
            first_parameter_node = parameter_node;
            last_parameter_node = parameter_node;
        } else {
            last_parameter_node->next_sibling = parameter_node;
            last_parameter_node = parameter_node;
        }
    }
    routine_node->data.routine_declaration.parameters = first_parameter_node;

    IjklSymbol* symbol = ijkl_symbol_create(arena,
                                           return_type_spec ? SYMBOL_KIND_FUNCTION : SYMBOL_KIND_PROCEDURE,
                                           routine_name,
                                           return_type_spec,
                                           routine_node,
                                           0,
                                           0);
    symbol->is_public = true;
    symbol->mangled_identifier_name = routine_name;
    ijkl_symbol_table_define(symbol_table, symbol);
}

static void register_builtin_routines(IjklSymbolTable* symbol_table) {
    const IjklPrimitiveType string_parameter[] = { PRIMITIVE_STRING };
    const IjklPrimitiveType two_strings_parameters[] = { PRIMITIVE_STRING, PRIMITIVE_STRING };
    const IjklPrimitiveType substring_parameters[] = { PRIMITIVE_STRING, PRIMITIVE_INT, PRIMITIVE_INT };
    const IjklPrimitiveType integer_parameter[] = { PRIMITIVE_INT };
    const IjklPrimitiveType real_parameter[] = { PRIMITIVE_REAL };
    const IjklPrimitiveType boolean_parameter[] = { PRIMITIVE_BOOL };
    const IjklPrimitiveType character_parameter[] = { PRIMITIVE_CHAR };

    const IjklPrimitiveType string_and_int_parameters[] = { PRIMITIVE_STRING, PRIMITIVE_INT };
    const IjklPrimitiveType string_int_char_parameters[] = { PRIMITIVE_STRING, PRIMITIVE_INT, PRIMITIVE_CHAR };

    /* Built-ins de Texto (ijkL.Text) */
    register_single_builtin_function(symbol_table, "__builtin_str_len", PRIMITIVE_INT, 1, string_parameter);
    register_single_builtin_function(symbol_table, "__builtin_str_cmp", PRIMITIVE_INT, 2, two_strings_parameters);
    register_single_builtin_function(symbol_table, "__builtin_str_concat", PRIMITIVE_STRING, 2, two_strings_parameters);
    register_single_builtin_function(symbol_table, "__builtin_str_sub", PRIMITIVE_STRING, 3, substring_parameters);
    register_single_builtin_function(symbol_table, "__builtin_str_create", PRIMITIVE_STRING, 1, integer_parameter);
    register_single_builtin_function(symbol_table, "__builtin_str_get_char", PRIMITIVE_CHAR, 2, string_and_int_parameters);
    register_single_builtin_function(symbol_table, "__builtin_str_set_char", PRIMITIVE_STRING, 3, string_int_char_parameters);

    /* Built-ins de Conversão (ijkL.Convert) */
    register_single_builtin_function(symbol_table, "__builtin_int_to_str", PRIMITIVE_STRING, 1, integer_parameter);
    register_single_builtin_function(symbol_table, "__builtin_real_to_str", PRIMITIVE_STRING, 1, real_parameter);
    register_single_builtin_function(symbol_table, "__builtin_str_to_int", PRIMITIVE_INT, 1, string_parameter);
    register_single_builtin_function(symbol_table, "__builtin_str_to_real", PRIMITIVE_REAL, 1, string_parameter);
    register_single_builtin_function(symbol_table, "__builtin_str_to_bool", PRIMITIVE_BOOL, 1, string_parameter);
    register_single_builtin_function(symbol_table, "__builtin_bool_to_str", PRIMITIVE_STRING, 1, boolean_parameter);
    register_single_builtin_function(symbol_table, "__builtin_char_to_str", PRIMITIVE_STRING, 1, character_parameter);

    /* Built-ins de Memória e DMA (ijkL.Memory) */
    IjklMemoryArena* arena = symbol_table->memory_arena;
    IjklTypeSpec* generic_pointer_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_POINTER, 0, 0);
    IjklTypeSpec* int_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    int_type->details.primitive_type = PRIMITIVE_INT;

    IjklTypeSpec* mem_copy_params[] = { generic_pointer_type, generic_pointer_type, int_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_mem_copy", generic_pointer_type, 3, mem_copy_params);

    IjklTypeSpec* mem_fill_params[] = { generic_pointer_type, int_type, int_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_mem_fill", generic_pointer_type, 3, mem_fill_params);

    IjklTypeSpec* mem_cmp_params[] = { generic_pointer_type, generic_pointer_type, int_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_mem_cmp", int_type, 3, mem_cmp_params);

    IjklTypeSpec* real_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    real_type->details.primitive_type = PRIMITIVE_REAL;

    /* Os ponteiros finais são estado oculto compartilhado com o módulo em CI. */
    IjklTypeSpec* random_seed_parameters[] = { int_type, generic_pointer_type, generic_pointer_type };
    IjklTypeSpec* random_state_parameters[] = { generic_pointer_type, generic_pointer_type };
    IjklTypeSpec* random_range_parameters[] = { int_type, int_type, generic_pointer_type, generic_pointer_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_random_seed", int_type, 3, random_seed_parameters);
    register_single_builtin_routine_with_types(symbol_table, "__builtin_random_seed_with_time", int_type, 2, random_state_parameters);
    register_single_builtin_routine_with_types(symbol_table, "__builtin_random_next", int_type, 2, random_state_parameters);
    register_single_builtin_routine_with_types(symbol_table, "__builtin_random_next_range", int_type, 4, random_range_parameters);
    register_single_builtin_routine_with_types(symbol_table, "__builtin_random_next_real", real_type, 2, random_state_parameters);

    /* Built-ins de Data e Hora (ijkL.Time.DateTime / ijkL.Tempo.DataHora) */
    register_single_builtin_routine_with_types(symbol_table, "__builtin_datetime_now", generic_pointer_type, 0, NULL);
    register_single_builtin_routine_with_types(symbol_table, "__builtin_datetime_utc_now", generic_pointer_type, 0, NULL);

    IjklTypeSpec* datetime_from_unix_params[] = { int_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_datetime_from_unix_timestamp", generic_pointer_type, 1, datetime_from_unix_params);

    IjklTypeSpec* datetime_single_param[] = { generic_pointer_type };
    register_single_builtin_routine_with_types(symbol_table, "__builtin_datetime_elapsed_seconds_of_day", int_type, 1, datetime_single_param);

    IjklTypeSpec* string_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    string_type->details.primitive_type = PRIMITIVE_STRING;
    register_single_builtin_routine_with_types(symbol_table, "__builtin_datetime_to_string", string_type, 1, datetime_single_param);
}

/* --- Suporte a Tipos Encapsulados (Boxed Types) --- */

bool ijkl_type_is_boxed_primitive(const IjklTypeSpec* type_specification,
                                  IjklPrimitiveType* out_primitive_type) {
    if (!type_specification) return false;

    const char* type_name = NULL;
    if (type_specification->kind == TYPE_KIND_USER_DEFINED) {
        type_name = type_specification->details.user_type_name;
    }

    if (!type_name) return false;

    /* Integer / Inteiro / Entier */
    if (strcmp(type_name, "Integer") == 0 ||
        strcmp(type_name, "Inteiro") == 0 ||
        strcmp(type_name, "Entier") == 0) {
        if (out_primitive_type) *out_primitive_type = PRIMITIVE_INT;
        return true;
    }

    /* Real / Reel */
    if (strcmp(type_name, "Real") == 0 ||
        strcmp(type_name, "Reel") == 0) {
        if (out_primitive_type) *out_primitive_type = PRIMITIVE_REAL;
        return true;
    }

    /* Boolean / Logico / Booleen */
    if (strcmp(type_name, "Boolean") == 0 ||
        strcmp(type_name, "Logico") == 0 ||
        strcmp(type_name, "Booleen") == 0) {
        if (out_primitive_type) *out_primitive_type = PRIMITIVE_BOOL;
        return true;
    }

    /* Character / Caracter / Caractere */
    if (strcmp(type_name, "Character") == 0 ||
        strcmp(type_name, "Caracter") == 0 ||
        strcmp(type_name, "Caractere") == 0) {
        if (out_primitive_type) *out_primitive_type = PRIMITIVE_CHAR;
        return true;
    }

    /* String / Texto / Texte */
    if (strcmp(type_name, "String") == 0 ||
        strcmp(type_name, "Texto") == 0 ||
        strcmp(type_name, "Texte") == 0) {
        if (out_primitive_type) *out_primitive_type = PRIMITIVE_STRING;
        return true;
    }

    return false;
}

static void register_single_boxed_type(IjklSymbolTable* symbol_table,
                                      const char* boxed_type_name,
                                      IjklPrimitiveType primitive_type) {
    IjklMemoryArena* arena = symbol_table->memory_arena;

    /* Cria os nós dos campos 'value' e 'valor' apontando para o offset 0 */
    IjklAstNode* field_value = ijkl_ast_node_create(arena, AST_NODE_PARAMETER, 0, 0);
    field_value->data.parameter.parameter_name = "value";
    IjklTypeSpec* field_value_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    field_value_type->details.primitive_type = primitive_type;
    field_value->data.parameter.type_specification = field_value_type;

    IjklAstNode* field_valor = ijkl_ast_node_create(arena, AST_NODE_PARAMETER, 0, 0);
    field_valor->data.parameter.parameter_name = "valor";
    IjklTypeSpec* field_valor_type = ijkl_ast_type_spec_create(arena, TYPE_KIND_PRIMITIVE, 0, 0);
    field_valor_type->details.primitive_type = primitive_type;
    field_valor->data.parameter.type_specification = field_valor_type;

    field_value->next_sibling = field_valor;

    /* Cria a estrutura interna que encapsula o primitivo */
    IjklTypeSpec* struct_type_spec = ijkl_ast_type_spec_create(arena, TYPE_KIND_STRUCT, 0, 0);
    struct_type_spec->details.struct_info.field_declarations = field_value;

    /* O tipo encapsulado é representado como um ponteiro para a estrutura na heap */
    IjklTypeSpec* pointer_type_spec = ijkl_ast_type_spec_create(arena, TYPE_KIND_POINTER, 0, 0);
    pointer_type_spec->details.pointer_info.base_type = struct_type_spec;

    /* Cria o símbolo de tipo no escopo global */
    IjklSymbol* type_symbol = ijkl_symbol_create(arena,
                                                 SYMBOL_KIND_TYPE,
                                                 boxed_type_name,
                                                 pointer_type_spec,
                                                 NULL,
                                                 0,
                                                 0);
    type_symbol->is_public = true;

    /* Cria o escopo interno para resolução de campos .value e .valor */
    char scope_name[128];
    snprintf(scope_name, sizeof(scope_name), "Boxed: %s", boxed_type_name);
    IjklScope* boxed_scope = ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_BLOCK, scope_name);
    type_symbol->internal_scope = boxed_scope;

    IjklSymbol* sym_value = ijkl_symbol_create(arena, SYMBOL_KIND_VARIABLE, "value", field_value_type, (IjklAstNode*)field_value, 0, 0);
    sym_value->memory_offset_in_bytes = 0;
    sym_value->memory_size_in_bytes = 8;
    ijkl_symbol_table_define(symbol_table, sym_value);

    IjklSymbol* sym_valor = ijkl_symbol_create(arena, SYMBOL_KIND_VARIABLE, "valor", field_valor_type, (IjklAstNode*)field_valor, 0, 0);
    sym_valor->memory_offset_in_bytes = 0;
    sym_valor->memory_size_in_bytes = 8;
    ijkl_symbol_table_define(symbol_table, sym_valor);

    ijkl_symbol_table_exit_scope(symbol_table);

    /* Registra o tipo no escopo global */
    ijkl_symbol_table_define(symbol_table, type_symbol);
}

static void register_builtin_boxed_types(IjklSymbolTable* symbol_table) {
    /* Inteiros */
    register_single_boxed_type(symbol_table, "Integer", PRIMITIVE_INT);
    register_single_boxed_type(symbol_table, "Inteiro", PRIMITIVE_INT);
    register_single_boxed_type(symbol_table, "Entier", PRIMITIVE_INT);

    /* Reais */
    register_single_boxed_type(symbol_table, "Real", PRIMITIVE_REAL);
    register_single_boxed_type(symbol_table, "Reel", PRIMITIVE_REAL);

    /* Lógicos */
    register_single_boxed_type(symbol_table, "Boolean", PRIMITIVE_BOOL);
    register_single_boxed_type(symbol_table, "Logico", PRIMITIVE_BOOL);
    register_single_boxed_type(symbol_table, "Booleen", PRIMITIVE_BOOL);

    /* Caracteres */
    register_single_boxed_type(symbol_table, "Character", PRIMITIVE_CHAR);
    register_single_boxed_type(symbol_table, "Caracter", PRIMITIVE_CHAR);
    register_single_boxed_type(symbol_table, "Caractere", PRIMITIVE_CHAR);

    /* Textos / Strings */
    register_single_boxed_type(symbol_table, "String", PRIMITIVE_STRING);
    register_single_boxed_type(symbol_table, "Texto", PRIMITIVE_STRING);
    register_single_boxed_type(symbol_table, "Texte", PRIMITIVE_STRING);
}

bool ijkl_symbol_table_is_known_builtin(const IjklSymbolTable* symbol_table, const char* identifier_name) {
    if (!symbol_table || !identifier_name) return false;
    if (strncmp(identifier_name, "__builtin_", 10) != 0) return false;

    IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(symbol_table, identifier_name);
    return (symbol != NULL && (symbol->symbol_kind == SYMBOL_KIND_FUNCTION || symbol->symbol_kind == SYMBOL_KIND_PROCEDURE));
}

IjklSymbolTable* ijkl_symbol_table_create(IjklMemoryArena* memory_arena) {
    IjklSymbolTable* symbol_table = (IjklSymbolTable*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklSymbolTable));
    memset(symbol_table, 0, sizeof(IjklSymbolTable));

    symbol_table->memory_arena = memory_arena;
    symbol_table->root_global_scope = scope_create(memory_arena, SCOPE_KIND_GLOBAL, "Global", NULL);
    symbol_table->current_active_scope = symbol_table->root_global_scope;
    symbol_table->active_vocabulary = NULL;
    symbol_table->total_scopes_count = 1;
    symbol_table->total_errors_count = 0;

    /* Registro dos tipos primitivos embutidos no escopo global */
    const char* primitive_type_names[] = { "int", "real", "float", "char", "string", "bool", "pointer" };
    for (size_t index = 0; index < sizeof(primitive_type_names) / sizeof(primitive_type_names[0]); index++) {
        IjklSymbol* primitive_type_symbol = ijkl_symbol_create(memory_arena,
                                                               SYMBOL_KIND_TYPE,
                                                               primitive_type_names[index],
                                                               NULL,
                                                               NULL,
                                                               0,
                                                               0);
        ijkl_symbol_table_define(symbol_table, primitive_type_symbol);
    }

    /* Registra tipos encapsulados (Boxed Types) embutidos no escopo global */
    register_builtin_boxed_types(symbol_table);

    /* Registra rotinas built-in embutidas no escopo global */
    register_builtin_routines(symbol_table);

    return symbol_table;
}

void ijkl_symbol_table_set_vocabulary(IjklSymbolTable* symbol_table, struct IjklVocabulary* vocabulary) {
    if (symbol_table) {
        symbol_table->active_vocabulary = vocabulary;
    }
}

IjklScope* ijkl_symbol_table_enter_scope(IjklSymbolTable* symbol_table,
                                        IjklScopeKind scope_kind,
                                        const char* scope_name) {
    IjklScope* parent_scope = symbol_table->current_active_scope;
    IjklScope* new_scope = scope_create(symbol_table->memory_arena, scope_kind, scope_name, parent_scope);

    /* Encadeia na lista de filhos do escopo pai */
    if (!parent_scope->first_child_scope) {
        parent_scope->first_child_scope = new_scope;
    } else {
        IjklScope* current_sibling = parent_scope->first_child_scope;
        while (current_sibling->next_sibling_scope) {
            current_sibling = current_sibling->next_sibling_scope;
        }
        current_sibling->next_sibling_scope = new_scope;
    }

    symbol_table->current_active_scope = new_scope;
    symbol_table->total_scopes_count++;
    return new_scope;
}

IjklScope* ijkl_symbol_table_exit_scope(IjklSymbolTable* symbol_table) {
    if (symbol_table->current_active_scope->parent_scope) {
        IjklScope* leaving_scope = symbol_table->current_active_scope;
        symbol_table->current_active_scope = symbol_table->current_active_scope->parent_scope;
        if (leaving_scope->scope_kind == SCOPE_KIND_BLOCK &&
            leaving_scope->current_memory_offset > symbol_table->current_active_scope->current_memory_offset) {
            symbol_table->current_active_scope->current_memory_offset = leaving_scope->current_memory_offset;
        }
    }
    return symbol_table->current_active_scope;
}

IjklScope* ijkl_symbol_table_current_scope(const IjklSymbolTable* symbol_table) {
    return symbol_table ? symbol_table->current_active_scope : NULL;
}

IjklScope* ijkl_symbol_table_root_scope(const IjklSymbolTable* symbol_table) {
    return symbol_table ? symbol_table->root_global_scope : NULL;
}

IjklSymbol* ijkl_scope_lookup_direct(const IjklScope* scope, const char* identifier_name) {
    if (!scope || !identifier_name) return NULL;
    size_t hash_index = calculate_identifier_hash(identifier_name);
    IjklSymbol* current_symbol = scope->symbol_hash_buckets[hash_index];

    while (current_symbol) {
        if (are_identifiers_equal(current_symbol->identifier_name, identifier_name)) {
            return current_symbol;
        }
        current_symbol = current_symbol->next_in_hash_bucket;
    }
    return NULL;
}

IjklSymbol* ijkl_symbol_table_lookup_current_scope(const IjklSymbolTable* symbol_table,
                                                  const char* identifier_name) {
    if (!symbol_table || !symbol_table->current_active_scope) return NULL;
    return ijkl_scope_lookup_direct(symbol_table->current_active_scope, identifier_name);
}

void ijkl_symbol_table_set_active_compilation_unit(IjklSymbolTable* symbol_table,
                                                   const struct IjklAstNode* unit_node) {
    if (symbol_table) {
        symbol_table->active_compilation_unit = unit_node;
    }
}

/*
 * Helper para busca estrita na hierarquia de escopos ativos (sem pesquisa em importações).
 */
static IjklSymbol* lookup_in_scope_chain(const IjklSymbolTable* symbol_table, const char* identifier_name) {
    if (!symbol_table || !identifier_name) return NULL;
    const IjklScope* current_scope = symbol_table->current_active_scope;

    while (current_scope) {
        IjklSymbol* found_symbol = ijkl_scope_lookup_direct(current_scope, identifier_name);
        if (found_symbol) {
            return found_symbol;
        }
        current_scope = current_scope->parent_scope;
    }
    return NULL;
}

IjklSymbol* ijkl_symbol_table_lookup_lexical(const IjklSymbolTable* symbol_table,
                                            const char* identifier_name) {
    if (!symbol_table || !identifier_name) return NULL;

    IjklSymbol* local_match = lookup_in_scope_chain(symbol_table, identifier_name);
    if (local_match) {
        return local_match;
    }

    /* Se o identificador contiver '.', trata-se de um caminho qualificado;
     * não deve pesquisar em escopos internos de módulos importados como membro simples */
    if (strchr(identifier_name, '.') != NULL) {
        return NULL;
    }

    /* Se não localizado na cadeia léxica local, pesquisa nas unidades importadas (Requisito 10) */
    if (symbol_table->active_compilation_unit) {
        const IjklAstNode* import_node = NULL;
        if (symbol_table->active_compilation_unit->kind == AST_NODE_PROGRAM) {
            import_node = symbol_table->active_compilation_unit->data.program.imports;
        } else if (symbol_table->active_compilation_unit->kind == AST_NODE_MODULE) {
            import_node = symbol_table->active_compilation_unit->data.module_declaration.imports;
        }

        IjklSymbol* resolved_imported_symbol = NULL;
        const char* first_matching_module_name = NULL;
        size_t matching_modules_count = 0;

        while (import_node) {
            if (import_node->kind == AST_NODE_IMPORT) {
                const char* imported_name = import_node->data.import_declaration.module_path;
                IjklSymbol* module_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, imported_name);
                if (module_symbol && module_symbol->internal_scope) {
                    IjklSymbol* candidate_symbol = ijkl_scope_lookup_direct(module_symbol->internal_scope, identifier_name);
                    if (!candidate_symbol && symbol_table->active_vocabulary) {
                        const char* canonical_member = ijkl_vocabulary_resolve_framework_member(
                            symbol_table->active_vocabulary, identifier_name);
                        if (canonical_member && canonical_member != identifier_name) {
                            candidate_symbol = ijkl_scope_lookup_direct(module_symbol->internal_scope, canonical_member);
                        }
                    }
                    if (candidate_symbol) {
                        if (!candidate_symbol->is_public) {
                            if (candidate_symbol->symbol_kind == SYMBOL_KIND_VARIABLE) {
                                fprintf(stderr, "Semantic error: Variable '%s' is private to unit '%s' and cannot be accessed from outside.\n",
                                        identifier_name, imported_name);
                            } else {
                                fprintf(stderr, "Semantic error: Routine '%s' in unit '%s' is private and cannot be accessed from outside.\n",
                                        identifier_name, imported_name);
                            }
                            ((IjklSymbolTable*)symbol_table)->total_errors_count++;
                            return NULL;
                        }
                        matching_modules_count++;
                        if (matching_modules_count == 1) {
                            resolved_imported_symbol = candidate_symbol;
                            first_matching_module_name = imported_name;
                        } else if (matching_modules_count == 2) {
                            fprintf(stderr, "Semantic error: Ambiguous identifier '%s' found in multiple imported modules ('%s' and '%s'). Explicit qualification required.\n",
                                     identifier_name, first_matching_module_name, imported_name);
                            ((IjklSymbolTable*)symbol_table)->total_errors_count++;
                        }
                    }
                }
            }
            import_node = import_node->next_sibling;
        }

        if (matching_modules_count == 1) {
            return resolved_imported_symbol;
        }
    }

    return NULL;
}

IjklSymbol* ijkl_symbol_table_lookup_qualified(const IjklSymbolTable* symbol_table,
                                              const char* qualified_path) {
    if (!symbol_table || !qualified_path) return NULL;

    /* 1. Busca direta no escopo corrente e ancestrais pelo caminho exato */
    IjklSymbol* direct_match = lookup_in_scope_chain(symbol_table, qualified_path);
    if (direct_match) {
        return direct_match;
    }

    /* Se não encontrou diretamente, tenta resolver alias canônico de unidade de framework */
    if (symbol_table->active_vocabulary) {
        const char* canonical_unit = ijkl_vocabulary_resolve_framework_unit(symbol_table->active_vocabulary, qualified_path);
        if (canonical_unit && canonical_unit != qualified_path) {
            direct_match = lookup_in_scope_chain(symbol_table, canonical_unit);
            if (direct_match) {
                return direct_match;
            }
        }
    }

    /* Se não houver ponto '.', trata-se de um identificador simples já verificado */
    const char* first_dot = strchr(qualified_path, '.');
    if (!first_dot) {
        return NULL;
    }

    /* 2. Decomposição baseada no módulo pai: ex.: 'GeometriaApp.Calculos.areaRetangulo' */
    const char* last_dot = strrchr(qualified_path, '.');
    if (last_dot && last_dot != first_dot) {
        size_t parent_length = (size_t)(last_dot - qualified_path);
        char parent_path[256];
        if (parent_length < sizeof(parent_path)) {
            strncpy(parent_path, qualified_path, parent_length);
            parent_path[parent_length] = '\0';
            const char* member_name = last_dot + 1;

            IjklSymbol* parent_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, parent_path);
            if (!parent_symbol && symbol_table->active_vocabulary) {
                const char* canonical_parent = ijkl_vocabulary_resolve_framework_unit(symbol_table->active_vocabulary, parent_path);
                if (canonical_parent && canonical_parent != parent_path) {
                    parent_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, canonical_parent);
                }
            }
            if (parent_symbol && parent_symbol->internal_scope) {
                IjklSymbol* member_symbol = ijkl_scope_lookup_direct(parent_symbol->internal_scope, member_name);
                if (!member_symbol && symbol_table->active_vocabulary) {
                    const char* canonical_member = ijkl_vocabulary_resolve_framework_member(symbol_table->active_vocabulary, member_name);
                    if (canonical_member && canonical_member != member_name) {
                        member_symbol = ijkl_scope_lookup_direct(parent_symbol->internal_scope, canonical_member);
                    }
                }
                if (member_symbol) {
                    if (parent_symbol->symbol_kind == SYMBOL_KIND_MODULE &&
                        symbol_table->active_compilation_unit &&
                        symbol_table->active_compilation_unit != parent_symbol->declaration_node) {
                        if (!member_symbol->is_public) {
                            if (member_symbol->symbol_kind == SYMBOL_KIND_VARIABLE) {
                                fprintf(stderr, "Semantic error: Variable '%s' is private to unit '%s' and cannot be accessed from outside.\n",
                                        member_name, parent_path);
                            } else {
                                fprintf(stderr, "Semantic error: Routine '%s' in unit '%s' is private and cannot be accessed from outside.\n",
                                        member_name, parent_path);
                            }
                            ((IjklSymbolTable*)symbol_table)->total_errors_count++;
                            return NULL;
                        }
                    }
                    return member_symbol;
                }
            }
        }
    }

    /* 3. Extrai o primeiro segmento do caminho */
    size_t first_segment_length = (size_t)(first_dot - qualified_path);
    char current_segment[256];
    if (first_segment_length >= sizeof(current_segment)) return NULL;
    strncpy(current_segment, qualified_path, first_segment_length);
    current_segment[first_segment_length] = '\0';

    IjklSymbol* current_symbol = lookup_in_scope_chain(symbol_table, current_segment);
    if (!current_symbol && symbol_table->active_vocabulary) {
        const char* canonical_segment_name = ijkl_vocabulary_resolve_framework_unit(symbol_table->active_vocabulary, current_segment);
        if (canonical_segment_name && canonical_segment_name != current_segment) {
            current_symbol = lookup_in_scope_chain(symbol_table, canonical_segment_name);
        }
    }
    if (!current_symbol && symbol_table->active_compilation_unit) {
        /* Verifica se o primeiro segmento corresponde ao nome curto de um módulo importado (ex. 'Conversao' de 'ijkL.Conversao') */
        const IjklAstNode* import_statement = (symbol_table->active_compilation_unit->kind == AST_NODE_PROGRAM) ?
            symbol_table->active_compilation_unit->data.program.imports :
            symbol_table->active_compilation_unit->data.module_declaration.imports;
        while (import_statement) {
            if (import_statement->kind == AST_NODE_IMPORT) {
                const char* imported_path = import_statement->data.import_declaration.module_path;
                const char* last_imported_dot = strrchr(imported_path, '.');
                const char* short_name = last_imported_dot ? last_imported_dot + 1 : imported_path;
                if (strcmp(short_name, current_segment) == 0) {
                    current_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, imported_path);
                    break;
                }
                if (symbol_table->active_vocabulary) {
                    const char* canonical_imported_path = ijkl_vocabulary_resolve_framework_unit(symbol_table->active_vocabulary, imported_path);
                    if (canonical_imported_path) {
                        const char* canonical_last_dot = strrchr(canonical_imported_path, '.');
                        const char* canonical_short_name = canonical_last_dot ? canonical_last_dot + 1 : canonical_imported_path;
                        if (strcmp(canonical_short_name, current_segment) == 0) {
                            current_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, canonical_imported_path);
                            break;
                        }
                    }
                }
            }
            import_statement = import_statement->next_sibling;
        }
    }
    if (!current_symbol) return NULL;

    const char* remaining_path = first_dot + 1;
    while (remaining_path && *remaining_path) {
        if (!current_symbol->internal_scope) {
            return NULL; /* Símbolo intermediário não possui escopo interno de membros */
        }

        const char* next_dot = strchr(remaining_path, '.');
        if (next_dot) {
            size_t segment_length = (size_t)(next_dot - remaining_path);
            if (segment_length >= sizeof(current_segment)) return NULL;
            strncpy(current_segment, remaining_path, segment_length);
            current_segment[segment_length] = '\0';
            remaining_path = next_dot + 1;
        } else {
            strncpy(current_segment, remaining_path, sizeof(current_segment) - 1);
            current_segment[sizeof(current_segment) - 1] = '\0';
            remaining_path = NULL;
        }

        IjklSymbol* member_symbol = ijkl_scope_lookup_direct(current_symbol->internal_scope, current_segment);
        if (!member_symbol && symbol_table->active_vocabulary) {
            const char* canonical_member_name = ijkl_vocabulary_resolve_framework_member(symbol_table->active_vocabulary, current_segment);
            if (canonical_member_name && canonical_member_name != current_segment) {
                member_symbol = ijkl_scope_lookup_direct(current_symbol->internal_scope, canonical_member_name);
            }
        }
        if (!member_symbol) return NULL;
        current_symbol = member_symbol;
    }

    return current_symbol;
}

/* Helper recursivo para serializar assinaturas de tipo de parâmetros */
static void append_type_signature(char* buffer, size_t buffer_size, const IjklTypeSpec* type_specification) {
    if (!type_specification) {
        strncat(buffer, "unknown", buffer_size - strlen(buffer) - 1);
        return;
    }

    switch (type_specification->kind) {
        case TYPE_KIND_PRIMITIVE:
            switch (type_specification->details.primitive_type) {
                case PRIMITIVE_INT:    strncat(buffer, "int", buffer_size - strlen(buffer) - 1); break;
                case PRIMITIVE_REAL:   strncat(buffer, "real", buffer_size - strlen(buffer) - 1); break;
                case PRIMITIVE_FLOAT:  strncat(buffer, "float", buffer_size - strlen(buffer) - 1); break;
                case PRIMITIVE_CHAR:   strncat(buffer, "char", buffer_size - strlen(buffer) - 1); break;
                case PRIMITIVE_STRING: strncat(buffer, "string", buffer_size - strlen(buffer) - 1); break;
                case PRIMITIVE_BOOL:   strncat(buffer, "bool", buffer_size - strlen(buffer) - 1); break;
                default:               strncat(buffer, "prim", buffer_size - strlen(buffer) - 1); break;
            }
            break;

        case TYPE_KIND_POINTER:
            strncat(buffer, "ptr_", buffer_size - strlen(buffer) - 1);
            append_type_signature(buffer, buffer_size, type_specification->details.pointer_info.base_type);
            break;

        case TYPE_KIND_ARRAY:
            strncat(buffer, "arr_", buffer_size - strlen(buffer) - 1);
            append_type_signature(buffer, buffer_size, type_specification->details.array_info.element_type);
            break;

        case TYPE_KIND_USER_DEFINED:
            strncat(buffer, "type_", buffer_size - strlen(buffer) - 1);
            if (type_specification->details.user_type_name) {
                strncat(buffer, type_specification->details.user_type_name, buffer_size - strlen(buffer) - 1);
            }
            break;

        default:
            strncat(buffer, "val", buffer_size - strlen(buffer) - 1);
            break;
    }
}

/*
 * EFEITO COLATERAL:
 * Constrói o identificador mangled canônico para rotinas com base em seus parâmetros.
 */
const char* ijkl_symbol_generate_mangled_name(IjklMemoryArena* memory_arena,
                                              const char* routine_name,
                                              const struct IjklAstNode* parameters_node) {
    if (!routine_name) return "";

    /* Rotinas de ponto de entrada e ciclo de vida preservam seu nome canônico sem mangling */
    if (strcmp(routine_name, "main") == 0 ||
        strcmp(routine_name, "principal") == 0 ||
        strcmp(routine_name, "inicializar") == 0 ||
        strcmp(routine_name, "initialize") == 0 ||
        strcmp(routine_name, "initialiser") == 0 ||
        strcmp(routine_name, "finalizar") == 0 ||
        strcmp(routine_name, "finalize") == 0 ||
        strcmp(routine_name, "finaliser") == 0) {
        return ijkl_memory_arena_duplicate_string(memory_arena, routine_name);
    }

    char mangled_buffer[512];
    snprintf(mangled_buffer, sizeof(mangled_buffer), "%s$", routine_name);

    if (!parameters_node) {
        strncat(mangled_buffer, "void", sizeof(mangled_buffer) - strlen(mangled_buffer) - 1);
    } else {
        const IjklAstNode* current_parameter = parameters_node;
        bool is_first_parameter = true;
        while (current_parameter) {
            if (current_parameter->kind == AST_NODE_PARAMETER) {
                if (!is_first_parameter) {
                    strncat(mangled_buffer, "_", sizeof(mangled_buffer) - strlen(mangled_buffer) - 1);
                }
                if (current_parameter->data.parameter.is_by_reference) {
                    strncat(mangled_buffer, "ref_", sizeof(mangled_buffer) - strlen(mangled_buffer) - 1);
                }
                append_type_signature(mangled_buffer, sizeof(mangled_buffer), current_parameter->data.parameter.type_specification);
                is_first_parameter = false;
            }
            current_parameter = current_parameter->next_sibling;
        }
        if (is_first_parameter) {
            strncat(mangled_buffer, "void", sizeof(mangled_buffer) - strlen(mangled_buffer) - 1);
        }
    }

    return ijkl_memory_arena_duplicate_string(memory_arena, mangled_buffer);
}

/*
 * Compara estruturalmente as listas de parâmetros de duas rotinas candidatas a sobrecarga.
 */
bool ijkl_symbol_are_parameter_types_identical(const struct IjklAstNode* parameters_a,
                                               const struct IjklAstNode* parameters_b) {
    const struct IjklAstNode* current_a = parameters_a;
    const struct IjklAstNode* current_b = parameters_b;
    while (current_a && current_b) {
        if (current_a->data.parameter.is_by_reference != current_b->data.parameter.is_by_reference) {
            return false;
        }
        if (!ijkl_ast_are_types_identical(current_a->data.parameter.type_specification,
                                          current_b->data.parameter.type_specification)) {
            return false;
        }
        current_a = current_a->next_sibling;
        current_b = current_b->next_sibling;
    }
    return (current_a == NULL && current_b == NULL);
}

bool ijkl_symbol_table_define(IjklSymbolTable* symbol_table, IjklSymbol* symbol) {
    if (!symbol_table || !symbol) return false;
    IjklScope* active_scope = symbol_table->current_active_scope;

    /* Verificação de identificadores previamente declarados no mesmo nível de escopo */
    IjklSymbol* existing_symbol = ijkl_scope_lookup_direct(active_scope, symbol->identifier_name);
    if (existing_symbol) {
        bool is_existing_routine = (existing_symbol->symbol_kind == SYMBOL_KIND_FUNCTION ||
                                    existing_symbol->symbol_kind == SYMBOL_KIND_PROCEDURE);
        bool is_new_routine = (symbol->symbol_kind == SYMBOL_KIND_FUNCTION ||
                               symbol->symbol_kind == SYMBOL_KIND_PROCEDURE);

        if (is_existing_routine && is_new_routine) {
            /* Ponto de entrada e rotinas de ciclo de vida não podem sofrer sobrecarga */
            const char* identifier = symbol->identifier_name;
            bool is_special_routine = (strcmp(identifier, "main") == 0 ||
                                       strcmp(identifier, "principal") == 0 ||
                                       strcmp(identifier, "inicializar") == 0 ||
                                       strcmp(identifier, "initialize") == 0 ||
                                       strcmp(identifier, "initialiser") == 0 ||
                                       strcmp(identifier, "finalizar") == 0 ||
                                       strcmp(identifier, "finalize") == 0 ||
                                       strcmp(identifier, "finaliser") == 0);

            if (is_special_routine) {
                fprintf(stderr, "%zu:%zu: Semantic error: Routine '%s' cannot be overloaded (previous declaration at %zu:%zu).\n",
                        symbol->line_number,
                        symbol->column_number,
                        symbol->identifier_name,
                        existing_symbol->line_number,
                        existing_symbol->column_number);
                symbol_table->total_errors_count++;
                return false;
            }

            /* Verifica duplicidade exata de parâmetros na cadeia de sobrecargas */
            const IjklAstNode* new_parameters = symbol->declaration_node ?
                symbol->declaration_node->data.routine_declaration.parameters : NULL;

            IjklSymbol* current_overload = existing_symbol;
            while (current_overload) {
                const IjklAstNode* existing_parameters = current_overload->declaration_node ?
                    current_overload->declaration_node->data.routine_declaration.parameters : NULL;

                if (ijkl_symbol_are_parameter_types_identical(existing_parameters, new_parameters)) {
                    fprintf(stderr, "%zu:%zu: Semantic error: Redeclaration of routine '%s' with identical parameter types in the same scope (previous declaration at %zu:%zu).\n",
                            symbol->line_number,
                            symbol->column_number,
                            symbol->identifier_name,
                            current_overload->line_number,
                            current_overload->column_number);
                    symbol_table->total_errors_count++;
                    return false;
                }
                current_overload = current_overload->next_overload;
            }

            /* Sobrecarga válida: gera nome mangled e anexa na lista de sobrecargas */
            symbol->mangled_identifier_name = ijkl_symbol_generate_mangled_name(
                symbol_table->memory_arena, symbol->identifier_name, new_parameters);
            symbol->is_overloaded = true;

            current_overload = existing_symbol;
            while (current_overload) {
                current_overload->is_overloaded = true;
                if (!current_overload->next_overload) {
                    current_overload->next_overload = symbol;
                    break;
                }
                current_overload = current_overload->next_overload;
            }

            /* Encadeia na ordem cronológica de declaração do escopo */
            if (!active_scope->first_symbol_in_declaration_order) {
                active_scope->first_symbol_in_declaration_order = symbol;
                active_scope->last_symbol_in_declaration_order = symbol;
            } else {
                active_scope->last_symbol_in_declaration_order->next_in_declaration_order = symbol;
                active_scope->last_symbol_in_declaration_order = symbol;
            }

            symbol->enclosing_scope = active_scope;
            symbol->memory_offset_in_bytes = 0;
            active_scope->total_symbol_count++;
            return true;
        } else {
            /* Conflito de identificador entre tipos heterogêneos de símbolos */
            fprintf(stderr, "%zu:%zu: Semantic error: Redeclaration of identifier '%s' in the same scope (previous declaration at %zu:%zu).\n",
                    symbol->line_number,
                    symbol->column_number,
                    symbol->identifier_name,
                    existing_symbol->line_number,
                    existing_symbol->column_number);
            symbol_table->total_errors_count++;
            return false;
        }
    }

    /* Primeiro registro do identificador no escopo */
    if (symbol->symbol_kind == SYMBOL_KIND_FUNCTION || symbol->symbol_kind == SYMBOL_KIND_PROCEDURE) {
        const IjklAstNode* parameters = symbol->declaration_node ?
            symbol->declaration_node->data.routine_declaration.parameters : NULL;
        symbol->mangled_identifier_name = ijkl_symbol_generate_mangled_name(
            symbol_table->memory_arena, symbol->identifier_name, parameters);
        symbol->is_overloaded = false;
    }

    size_t hash_index = calculate_identifier_hash(symbol->identifier_name);
    symbol->next_in_hash_bucket = active_scope->symbol_hash_buckets[hash_index];
    active_scope->symbol_hash_buckets[hash_index] = symbol;

    /* Encadeia na ordem de declaração para impressão pedagógica */
    if (!active_scope->first_symbol_in_declaration_order) {
        active_scope->first_symbol_in_declaration_order = symbol;
        active_scope->last_symbol_in_declaration_order = symbol;
    } else {
        active_scope->last_symbol_in_declaration_order->next_in_declaration_order = symbol;
        active_scope->last_symbol_in_declaration_order = symbol;
    }

    symbol->enclosing_scope = active_scope;
    if (symbol->symbol_kind == SYMBOL_KIND_VARIABLE ||
        symbol->symbol_kind == SYMBOL_KIND_PARAMETER) {
        if (symbol->is_by_reference) {
            symbol->memory_size_in_bytes = 8;
        } else if (symbol->type_specification) {
            symbol->memory_size_in_bytes = ijkl_symbol_table_calculate_type_size(symbol->type_specification, symbol_table);
        }
        IjklScope* allocation_scope = (active_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                                       active_scope->scope_kind == SCOPE_KIND_MODULE)
                                          ? symbol_table->root_global_scope
                                          : active_scope;
        size_t aligned_offset = (allocation_scope->current_memory_offset + 7) & ~7;
        symbol->memory_offset_in_bytes = aligned_offset;
        size_t raw_size = symbol->memory_size_in_bytes > 0 ? symbol->memory_size_in_bytes : 8;
        size_t aligned_size = (raw_size + 7) & ~7;
        allocation_scope->current_memory_offset = aligned_offset + aligned_size;
    } else {
        symbol->memory_offset_in_bytes = 0;
    }
    active_scope->total_symbol_count++;

    return true;
}

size_t ijkl_symbol_table_get_error_count(const IjklSymbolTable* symbol_table) {
    return symbol_table ? symbol_table->total_errors_count : 0;
}

/* --- Construção Semântica da Tabela a partir da AST --- */

static void build_statement_symbols(IjklSymbolTable* symbol_table, const IjklAstNode* statement_node);

/*
 * PASSAGEM 1: Registra tipos, registros, constantes, variáveis e assinaturas de rotinas.
 * Não analisa os corpos das rotinas neste momento para permitir referências cruzadas e mútuas.
 */
static void build_declaration_symbols_pass1(IjklSymbolTable* symbol_table, const IjklAstNode* declaration_node) {
    while (declaration_node) {
        switch (declaration_node->kind) {
            case AST_NODE_DECL_CONST: {
                IjklSymbol* constant_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                 SYMBOL_KIND_CONSTANT,
                                                                 declaration_node->data.constant_declaration.constant_name,
                                                                 declaration_node->data.constant_declaration.type_specification,
                                                                 (IjklAstNode*)declaration_node,
                                                                 declaration_node->line_number,
                                                                 declaration_node->column_number);
                constant_symbol->is_public = true;
                ijkl_symbol_table_define(symbol_table, constant_symbol);
                break;
            }

            case AST_NODE_DECL_TYPE: {
                IjklSymbol* type_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                             SYMBOL_KIND_TYPE,
                                                             declaration_node->data.type_declaration.defined_type_name,
                                                             declaration_node->data.type_declaration.type_specification,
                                                             (IjklAstNode*)declaration_node,
                                                             declaration_node->line_number,
                                                             declaration_node->column_number);
                type_symbol->is_public = true;

                /* Se o tipo for registro (struct) ou ponteiro para registro (reference type), cria escopo interno para campos */
                IjklTypeSpec* target_struct_type = declaration_node->data.type_declaration.type_specification;
                if (target_struct_type && target_struct_type->kind == TYPE_KIND_POINTER && target_struct_type->details.pointer_info.base_type) {
                    target_struct_type = target_struct_type->details.pointer_info.base_type;
                }

                if (target_struct_type && target_struct_type->kind == TYPE_KIND_STRUCT) {
                    char struct_scope_name[256];
                    snprintf(struct_scope_name, sizeof(struct_scope_name), "Struct: %s", declaration_node->data.type_declaration.defined_type_name);
                    IjklScope* struct_fields_scope = ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_BLOCK, struct_scope_name);
                    type_symbol->internal_scope = struct_fields_scope;

                    size_t struct_field_offset = 0;
                    const IjklAstNode* field_node = target_struct_type->details.struct_info.field_declarations;
                    while (field_node) {
                        const char* field_name = NULL;
                        IjklTypeSpec* field_type = NULL;
                        if (field_node->kind == AST_NODE_PARAMETER) {
                            field_name = field_node->data.parameter.parameter_name;
                            field_type = field_node->data.parameter.type_specification;
                        } else if (field_node->kind == AST_NODE_DECL_VAR) {
                            field_name = field_node->data.variable_declaration.variable_name;
                            field_type = field_node->data.variable_declaration.type_specification;
                        }

                        if (field_name) {
                            IjklSymbol* field_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                          SYMBOL_KIND_VARIABLE,
                                                                          field_name,
                                                                          field_type,
                                                                          (IjklAstNode*)field_node,
                                                                          field_node->line_number,
                                                                          field_node->column_number);
                            field_symbol->is_public = true;
                            field_symbol->memory_offset_in_bytes = struct_field_offset;
                            struct_field_offset += field_symbol->memory_size_in_bytes > 0 ? field_symbol->memory_size_in_bytes : 8;
                            ijkl_symbol_table_define(symbol_table, field_symbol);
                        }
                        field_node = field_node->next_sibling;
                    }
                    ijkl_symbol_table_exit_scope(symbol_table);
                }

                ijkl_symbol_table_define(symbol_table, type_symbol);
                break;
            }

            case AST_NODE_DECL_VAR: {
                IjklSymbol* variable_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                 SYMBOL_KIND_VARIABLE,
                                                                 declaration_node->data.variable_declaration.variable_name,
                                                                 declaration_node->data.variable_declaration.type_specification,
                                                                 (IjklAstNode*)declaration_node,
                                                                 declaration_node->line_number,
                                                                 declaration_node->column_number);
                variable_symbol->is_public = false;
                ijkl_symbol_table_define(symbol_table, variable_symbol);
                break;
            }

            case AST_NODE_DECL_ROUTINE: {
                bool is_function = (declaration_node->data.routine_declaration.return_type_spec != NULL);
                IjklSymbolKind routine_kind = is_function ? SYMBOL_KIND_FUNCTION : SYMBOL_KIND_PROCEDURE;
                IjklSymbol* routine_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                routine_kind,
                                                                declaration_node->data.routine_declaration.routine_name,
                                                                declaration_node->data.routine_declaration.return_type_spec,
                                                                (IjklAstNode*)declaration_node,
                                                                declaration_node->line_number,
                                                                declaration_node->column_number);
                routine_symbol->is_public = declaration_node->data.routine_declaration.is_public;
                if (!ijkl_symbol_table_define(symbol_table, routine_symbol)) {
                    break;
                }

                /* Abre escopo da rotina para registro de parâmetros formais */
                char routine_scope_name[256];
                snprintf(routine_scope_name, sizeof(routine_scope_name), "Routine: %s",
                         routine_symbol->mangled_identifier_name ? routine_symbol->mangled_identifier_name : declaration_node->data.routine_declaration.routine_name);
                IjklScope* routine_scope = ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_ROUTINE, routine_scope_name);
                routine_symbol->internal_scope = routine_scope;

                /* Registra parâmetros formais */
                const IjklAstNode* parameter_node = declaration_node->data.routine_declaration.parameters;
                while (parameter_node) {
                    if (parameter_node->kind == AST_NODE_PARAMETER) {
                        IjklSymbol* parameter_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                          SYMBOL_KIND_PARAMETER,
                                                                          parameter_node->data.parameter.parameter_name,
                                                                          parameter_node->data.parameter.type_specification,
                                                                          (IjklAstNode*)parameter_node,
                                                                          parameter_node->line_number,
                                                                          parameter_node->column_number);
                        parameter_symbol->is_by_reference = parameter_node->data.parameter.is_by_reference;
                        ijkl_symbol_table_define(symbol_table, parameter_symbol);
                    }
                    parameter_node = parameter_node->next_sibling;
                }

                ijkl_symbol_table_exit_scope(symbol_table);
                break;
            }

            default:
                break;
        }

        declaration_node = declaration_node->next_sibling;
    }
}

/*
 * PASSAGEM 2: Analisa os corpos das rotinas e constrói seus símbolos e blocos locais.
 */
static void build_declaration_symbols_pass2(IjklSymbolTable* symbol_table, const IjklAstNode* declaration_node) {
    while (declaration_node) {
        if (declaration_node->kind == AST_NODE_DECL_ROUTINE) {
            const char* routine_name = declaration_node->data.routine_declaration.routine_name;
            IjklSymbol* routine_symbol = ijkl_scope_lookup_direct(symbol_table->current_active_scope, routine_name);
            while (routine_symbol) {
                if (routine_symbol->declaration_node == declaration_node) {
                    break;
                }
                routine_symbol = routine_symbol->next_overload;
            }
            if (routine_symbol && routine_symbol->internal_scope) {
                IjklScope* previous_scope = symbol_table->current_active_scope;
                symbol_table->current_active_scope = routine_symbol->internal_scope;

                if (declaration_node->data.routine_declaration.routine_body) {
                    build_statement_symbols(symbol_table, declaration_node->data.routine_declaration.routine_body);
                }

                symbol_table->current_active_scope = previous_scope;
            }
        }
        declaration_node = declaration_node->next_sibling;
    }
}

static void build_statement_symbols(IjklSymbolTable* symbol_table, const IjklAstNode* statement_node) {
    while (statement_node) {
        switch (statement_node->kind) {
            case AST_NODE_STMT_BLOCK: {
                char block_scope_name[64];
                snprintf(block_scope_name, sizeof(block_scope_name), "Block_L%zu_C%zu", statement_node->line_number, statement_node->column_number);
                ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_BLOCK, block_scope_name);
                build_statement_symbols(symbol_table, statement_node->data.block_statement.statements);
                ijkl_symbol_table_exit_scope(symbol_table);
                break;
            }

            case AST_NODE_DECL_VAR: {
                IjklSymbol* local_variable_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                       SYMBOL_KIND_VARIABLE,
                                                                       statement_node->data.variable_declaration.variable_name,
                                                                       statement_node->data.variable_declaration.type_specification,
                                                                       (IjklAstNode*)statement_node,
                                                                       statement_node->line_number,
                                                                       statement_node->column_number);
                ijkl_symbol_table_define(symbol_table, local_variable_symbol);
                break;
            }

            case AST_NODE_STMT_IF: {
                if (statement_node->data.if_statement.then_branch_statement) {
                    build_statement_symbols(symbol_table, statement_node->data.if_statement.then_branch_statement);
                }
                if (statement_node->data.if_statement.else_branch_statement) {
                    build_statement_symbols(symbol_table, statement_node->data.if_statement.else_branch_statement);
                }
                break;
            }

            case AST_NODE_STMT_SWITCH: {
                const IjklAstNode* case_branch = statement_node->data.switch_statement.case_branches;
                while (case_branch) {
                    if (case_branch->data.case_branch.statements) {
                        build_statement_symbols(symbol_table, case_branch->data.case_branch.statements);
                    }
                    case_branch = case_branch->next_sibling;
                }
                if (statement_node->data.switch_statement.default_branch) {
                    build_statement_symbols(symbol_table, statement_node->data.switch_statement.default_branch);
                }
                break;
            }

            case AST_NODE_STMT_WHILE: {
                if (statement_node->data.while_statement.body_statement) {
                    build_statement_symbols(symbol_table, statement_node->data.while_statement.body_statement);
                }
                break;
            }

            case AST_NODE_STMT_FOR: {
                char for_scope_name[64];
                snprintf(for_scope_name, sizeof(for_scope_name), "ForLoop_L%zu", statement_node->line_number);
                ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_BLOCK, for_scope_name);

                /* Registra variável de controle do laço 'para' */
                if (statement_node->data.for_statement.control_variable_name) {
                    IjklSymbol* loop_variable_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                                          SYMBOL_KIND_VARIABLE,
                                                                          statement_node->data.for_statement.control_variable_name,
                                                                          statement_node->data.for_statement.control_variable_type,
                                                                          (IjklAstNode*)statement_node,
                                                                          statement_node->line_number,
                                                                          statement_node->column_number);
                    ijkl_symbol_table_define(symbol_table, loop_variable_symbol);
                }

                if (statement_node->data.for_statement.body_statement) {
                    build_statement_symbols(symbol_table, statement_node->data.for_statement.body_statement);
                }

                ijkl_symbol_table_exit_scope(symbol_table);
                break;
            }

            case AST_NODE_STMT_REPEAT: {
                if (statement_node->data.repeat_statement.body_block) {
                    build_statement_symbols(symbol_table, statement_node->data.repeat_statement.body_block);
                }
                break;
            }

            default:
                break;
        }

        statement_node = statement_node->next_sibling;
    }
}

bool ijkl_symbol_table_build_from_ast(IjklSymbolTable* symbol_table, const IjklAstNode* compilation_unit_root) {
    if (!symbol_table || !compilation_unit_root) return false;

    /* ========================================================================= */
    /* Passagem 1: Assinaturas, tipos, constantes e variáveis de todas as unidades*/
    /* ========================================================================= */
    const IjklAstNode* current_unit = compilation_unit_root;
    while (current_unit) {
        symbol_table->active_compilation_unit = current_unit;
        if (current_unit->kind == AST_NODE_PROGRAM) {
            const char* program_name = current_unit->data.program.program_name;

            IjklSymbol* program_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                            SYMBOL_KIND_MODULE,
                                                            program_name,
                                                            NULL,
                                                            (IjklAstNode*)current_unit,
                                                            current_unit->line_number,
                                                            current_unit->column_number);
            ijkl_symbol_table_define(symbol_table, program_symbol);

            char program_scope_name[256];
            snprintf(program_scope_name, sizeof(program_scope_name), "Program: %s", program_name ? program_name : "Main");
            IjklScope* program_scope = ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_MODULE, program_scope_name);
            program_symbol->internal_scope = program_scope;

            build_declaration_symbols_pass1(symbol_table, current_unit->data.program.declarations);
            ijkl_symbol_table_exit_scope(symbol_table);
        } else if (current_unit->kind == AST_NODE_MODULE) {
            const char* qualified_module_path = current_unit->data.module_declaration.qualified_module_path;

            /* Validação da hierarquia de submódulos pontuados */
            const char* last_dot = strrchr(qualified_module_path, '.');
            if (last_dot) {
                size_t parent_path_length = (size_t)(last_dot - qualified_module_path);
                char parent_module_path[256];
                if (parent_path_length < sizeof(parent_module_path)) {
                    strncpy(parent_module_path, qualified_module_path, parent_path_length);
                    parent_module_path[parent_path_length] = '\0';

                    IjklSymbol* parent_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, parent_module_path);
                    if (!parent_symbol || parent_symbol->symbol_kind != SYMBOL_KIND_MODULE) {
                        /* Notifica dependência do módulo pai (sem impedir a compilação local de subárvore) */
                    }
                }
            }

            IjklSymbol* module_symbol = ijkl_symbol_create(symbol_table->memory_arena,
                                                           SYMBOL_KIND_MODULE,
                                                           qualified_module_path,
                                                           NULL,
                                                           (IjklAstNode*)current_unit,
                                                           current_unit->line_number,
                                                           current_unit->column_number);
            ijkl_symbol_table_define(symbol_table, module_symbol);

            char module_scope_name[256];
            snprintf(module_scope_name, sizeof(module_scope_name), "Module: %s", qualified_module_path ? qualified_module_path : "Submodule");
            IjklScope* module_scope = ijkl_symbol_table_enter_scope(symbol_table, SCOPE_KIND_MODULE, module_scope_name);
            module_symbol->internal_scope = module_scope;

            build_declaration_symbols_pass1(symbol_table, current_unit->data.module_declaration.declarations);
            ijkl_symbol_table_exit_scope(symbol_table);
        }

        current_unit = current_unit->next_sibling;
    }

    /* ========================================================================= */
    /* Passagem 2: Corpos de rotinas e expressões de todas as unidades            */
    /* ========================================================================= */
    current_unit = compilation_unit_root;
    while (current_unit) {
        symbol_table->active_compilation_unit = current_unit;
        if (current_unit->kind == AST_NODE_PROGRAM) {
            const char* program_name = current_unit->data.program.program_name;
            IjklSymbol* program_symbol = ijkl_symbol_table_lookup_lexical(symbol_table, program_name);
            IjklScope* previous_scope = symbol_table->current_active_scope;
            if (program_symbol && program_symbol->internal_scope) {
                symbol_table->current_active_scope = program_symbol->internal_scope;
            }

            build_declaration_symbols_pass2(symbol_table, current_unit->data.program.declarations);

            if (current_unit->data.program.entry_point) {
                build_statement_symbols(symbol_table, current_unit->data.program.entry_point->data.entry_point.executable_statements);
            }

            symbol_table->current_active_scope = previous_scope;
        } else if (current_unit->kind == AST_NODE_MODULE) {
            const char* qualified_module_path = current_unit->data.module_declaration.qualified_module_path;
            IjklSymbol* module_symbol = ijkl_symbol_table_lookup_qualified(symbol_table, qualified_module_path);
            IjklScope* previous_scope = symbol_table->current_active_scope;
            if (module_symbol && module_symbol->internal_scope) {
                symbol_table->current_active_scope = module_symbol->internal_scope;
            }

            build_declaration_symbols_pass2(symbol_table, current_unit->data.module_declaration.declarations);

            symbol_table->current_active_scope = previous_scope;
        }

        current_unit = current_unit->next_sibling;
    }

    symbol_table->active_compilation_unit = NULL;
    return symbol_table->total_errors_count == 0;
}

/* --- Formatação e Exibição Diagnóstica --- */

const char* ijkl_symbol_kind_name(IjklSymbolKind symbol_kind) {
    switch (symbol_kind) {
        case SYMBOL_KIND_VARIABLE: return "Variable";
        case SYMBOL_KIND_CONSTANT: return "Constant";
        case SYMBOL_KIND_TYPE: return "Type";
        case SYMBOL_KIND_FUNCTION: return "Function";
        case SYMBOL_KIND_PROCEDURE: return "Procedure";
        case SYMBOL_KIND_PARAMETER: return "Parameter";
        case SYMBOL_KIND_MODULE: return "Module";
        default: return "UnknownSymbol";
    }
}

const char* ijkl_scope_kind_name(IjklScopeKind scope_kind) {
    switch (scope_kind) {
        case SCOPE_KIND_GLOBAL: return "Global";
        case SCOPE_KIND_MODULE: return "Module";
        case SCOPE_KIND_ROUTINE: return "Routine";
        case SCOPE_KIND_BLOCK: return "Block";
        default: return "UnknownScope";
    }
}

static void print_hierarchical_scope(const IjklScope* scope, FILE* output_stream, const char* indentation_prefix, bool is_last_child) {
    if (!scope) return;

    /* Imprime o cabeçalho do escopo */
    fprintf(output_stream, "%s%s[Scope: %s] (Kind: %s, Depth: %zu, Symbols: %zu)\n",
            indentation_prefix,
            is_last_child ? "└── " : "├── ",
            scope->scope_name,
            ijkl_scope_kind_name(scope->scope_kind),
            scope->nesting_depth_level,
            scope->total_symbol_count);

    char child_indentation[256];
    snprintf(child_indentation, sizeof(child_indentation), "%s%s",
             indentation_prefix,
             is_last_child ? "    " : "│   ");

    /* Imprime os símbolos locais em ordem de declaração */
    const IjklSymbol* current_symbol = scope->first_symbol_in_declaration_order;
    while (current_symbol) {
        bool is_last_symbol = (current_symbol->next_in_declaration_order == NULL) && (scope->first_child_scope == NULL);
        fprintf(output_stream, "%s%s%s: %s (Offset: %zuB, Size: %zuB, Declared at L%zu:C%zu)\n",
                child_indentation,
                is_last_symbol ? "└── " : "├── ",
                ijkl_symbol_kind_name(current_symbol->symbol_kind),
                current_symbol->identifier_name,
                current_symbol->memory_offset_in_bytes,
                current_symbol->memory_size_in_bytes,
                current_symbol->line_number,
                current_symbol->column_number);

        current_symbol = current_symbol->next_in_declaration_order;
    }

    /* Imprime recursivamente os subescopos filhos */
    const IjklScope* current_child = scope->first_child_scope;
    while (current_child) {
        bool is_last_subscope = (current_child->next_sibling_scope == NULL);
        print_hierarchical_scope(current_child, output_stream, child_indentation, is_last_subscope);
        current_child = current_child->next_sibling_scope;
    }
}

void ijkl_symbol_table_print(const IjklSymbolTable* symbol_table, FILE* output_stream) {
    if (!symbol_table || !output_stream) return;

    fprintf(output_stream, "=== Symbol Table & Scopes Analysis ===\n");
    fprintf(output_stream, "Total scopes registered: %zu | Semantic errors: %zu\n\n",
            symbol_table->total_scopes_count,
            symbol_table->total_errors_count);

    print_hierarchical_scope(symbol_table->root_global_scope, output_stream, "", true);
}

/*
 * EFEITO COLATERAL:
 * Resolve estaticamente a rotina mais adequada para chamada em conformidade com os tipos
 * dos argumentos, calculando penalidades de coerção e detectando ambiguidades.
 */
IjklSymbol* ijkl_symbol_resolve_overload(IjklSymbol* routine_symbol_head,
                                        struct IjklTypeSpec** argument_types,
                                        size_t argument_count,
                                        bool* is_ambiguous) {
    if (is_ambiguous) *is_ambiguous = false;
    if (!routine_symbol_head) return NULL;

    IjklSymbol* best_candidate = NULL;
    int best_penalty = 999999;
    bool ambiguity_detected = false;

    IjklSymbol* current_candidate = routine_symbol_head;
    while (current_candidate) {
        if (current_candidate->symbol_kind != SYMBOL_KIND_FUNCTION &&
            current_candidate->symbol_kind != SYMBOL_KIND_PROCEDURE) {
            current_candidate = current_candidate->next_overload;
            continue;
        }

        const IjklAstNode* parameters = current_candidate->declaration_node ?
            current_candidate->declaration_node->data.routine_declaration.parameters : NULL;

        size_t parameter_count = 0;
        const IjklAstNode* param_cursor = parameters;
        while (param_cursor) {
            parameter_count++;
            param_cursor = param_cursor->next_sibling;
        }

        if (parameter_count == argument_count) {
            bool candidate_matches = true;
            int current_penalty = 0;

            const IjklAstNode* current_param = parameters;
            for (size_t index = 0; index < argument_count; index++) {
                IjklTypeSpec* target_type = current_param->data.parameter.type_specification;
                IjklTypeSpec* source_type = argument_types[index];
                bool is_ref_param = current_param->data.parameter.is_by_reference;

                if (ijkl_ast_are_types_identical(target_type, source_type)) {
                    /* Casamento exato: penalidade zero */
                } else if (!is_ref_param && target_type && source_type &&
                           target_type->kind == TYPE_KIND_PRIMITIVE && source_type->kind == TYPE_KIND_PRIMITIVE &&
                           target_type->details.primitive_type == PRIMITIVE_REAL &&
                           source_type->details.primitive_type == PRIMITIVE_FLOAT) {
                    current_penalty += 1; /* float promovido para real */
                } else if (!is_ref_param && target_type && source_type &&
                           target_type->kind == TYPE_KIND_PRIMITIVE && source_type->kind == TYPE_KIND_PRIMITIVE &&
                           (target_type->details.primitive_type == PRIMITIVE_REAL || target_type->details.primitive_type == PRIMITIVE_FLOAT) &&
                           source_type->details.primitive_type == PRIMITIVE_INT) {
                    current_penalty += 2; /* int alargado para real ou flutuante */
                } else if (!is_ref_param && ijkl_semantic_are_types_compatible(target_type, source_type)) {
                    current_penalty += 3;
                } else {
                    candidate_matches = false;
                    break;
                }
                current_param = current_param->next_sibling;
            }

            if (candidate_matches) {
                if (current_penalty == 0) {
                    /* Casamento idêntico imediato */
                    if (is_ambiguous) *is_ambiguous = false;
                    return current_candidate;
                }

                if (current_penalty < best_penalty) {
                    best_penalty = current_penalty;
                    best_candidate = current_candidate;
                    ambiguity_detected = false;
                } else if (current_penalty == best_penalty) {
                    ambiguity_detected = true;
                }
            }
        }

        current_candidate = current_candidate->next_overload;
    }

    if (ambiguity_detected) {
        if (is_ambiguous) *is_ambiguous = true;
        return NULL;
    }

    return best_candidate;
}
