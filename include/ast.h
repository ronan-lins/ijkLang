#ifndef IJKL_AST_H
#define IJKL_AST_H

#include "common.h"
#include "memory.h"
#include "token.h"

/*
 * ==============================================================================
 * MÓDULO: ÁRVORE SINTÁTICA ABSTRATA (AST - ABSTRACT SYNTAX TREE)
 * ==============================================================================
 * Define os tipos de nós, especificadores de tipo e estruturas que compõem
 * a representação intermediária em árvore da gramática normativa ijkLang.
 *
 * Siglas técnicas empregadas neste módulo:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

/* Categorias de nós da Árvore Sintática Abstrata */
typedef enum {
    /* Estrutura de compilação e espaços de nomes */
    AST_NODE_PROGRAM,
    AST_NODE_MODULE,
    AST_NODE_IMPORT,
    AST_NODE_ENTRY_POINT,

    /* Declarações */
    AST_NODE_DECL_CONST,
    AST_NODE_DECL_TYPE,
    AST_NODE_DECL_VAR,
    AST_NODE_DECL_ROUTINE,
    AST_NODE_PARAMETER,

    /* Comandos */
    AST_NODE_STMT_BLOCK,
    AST_NODE_STMT_ASSIGN,
    AST_NODE_STMT_CALL,
    AST_NODE_STMT_IF,
    AST_NODE_STMT_SWITCH,
    AST_NODE_CASE_BRANCH,
    AST_NODE_STMT_WHILE,
    AST_NODE_STMT_FOR,
    AST_NODE_STMT_REPEAT,
    AST_NODE_STMT_BREAK,
    AST_NODE_STMT_RETURN,
    AST_NODE_STMT_READ,
    AST_NODE_STMT_WRITE,
    AST_NODE_STMT_FREE,
    AST_NODE_STMT_ASM,

    /* Expressões */
    AST_NODE_EXPR_BINARY,
    AST_NODE_EXPR_UNARY,
    AST_NODE_EXPR_DEREFERENCE,
    AST_NODE_EXPR_MEMBER_ACCESS,
    AST_NODE_EXPR_INDEX_ACCESS,
    AST_NODE_EXPR_CALL,
    AST_NODE_EXPR_ALLOC,
    AST_NODE_EXPR_LITERAL_INT,
    AST_NODE_EXPR_LITERAL_REAL,
    AST_NODE_EXPR_LITERAL_FLOAT,
    AST_NODE_EXPR_LITERAL_CHAR,
    AST_NODE_EXPR_LITERAL_STRING,
    AST_NODE_EXPR_LITERAL_BOOL,
    AST_NODE_EXPR_LITERAL_NULL,
    AST_NODE_EXPR_IDENTIFIER
} IjklAstNodeKind;

/* Categorias de especificadores de tipo */
typedef enum {
    TYPE_KIND_PRIMITIVE,
    TYPE_KIND_ARRAY,
    TYPE_KIND_STRUCT,
    TYPE_KIND_POINTER,
    TYPE_KIND_USER_DEFINED
} IjklTypeKind;

/* Tipos primitivos suportados na ijkLang */
typedef enum {
    PRIMITIVE_INT,
    PRIMITIVE_REAL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_CHAR,
    PRIMITIVE_STRING,
    PRIMITIVE_BOOL
} IjklPrimitiveType;

/* Estrutura representativa de um especificador de tipo na AST */
typedef struct IjklTypeSpec {
    IjklTypeKind kind;
    size_t line_number;
    size_t column_number;

    union {
        IjklPrimitiveType primitive_type;
        const char* user_type_name;

        struct {
            struct IjklTypeSpec* element_type;
            size_t fixed_element_count;
        } array_info;

        struct {
            struct IjklAstNode* field_declarations;
        } struct_info;

        struct {
            struct IjklTypeSpec* base_type;
        } pointer_info;
    } details;
} IjklTypeSpec;

struct IjklSymbol;

/* Estrutura genérica de nó da Árvore Sintática Abstrata (AST) */
typedef struct IjklAstNode {
    IjklAstNodeKind kind;
    size_t line_number;
    size_t column_number;
    struct IjklAstNode* next_sibling;
    struct IjklTypeSpec* inferred_type;
    struct IjklSymbol* resolved_symbol;

    union {
        /* AST_NODE_PROGRAM */
        struct {
            const char* program_name;
            struct IjklAstNode* imports;
            struct IjklAstNode* declarations;
            struct IjklAstNode* entry_point;
        } program;

        /* AST_NODE_MODULE */
        struct {
            const char* qualified_module_path;
            struct IjklAstNode* imports;
            struct IjklAstNode* declarations;
        } module_declaration;

        /* AST_NODE_IMPORT */
        struct {
            const char* module_path;
        } import_declaration;

        /* AST_NODE_ENTRY_POINT */
        struct {
            struct IjklAstNode* executable_statements;
        } entry_point;

        /* AST_NODE_DECL_CONST */
        struct {
            const char* constant_name;
            IjklTypeSpec* type_specification;
            struct IjklAstNode* initial_value_expression;
        } constant_declaration;

        /* AST_NODE_DECL_TYPE */
        struct {
            const char* defined_type_name;
            IjklTypeSpec* type_specification;
        } type_declaration;

        /* AST_NODE_DECL_VAR */
        struct {
            const char* variable_name;
            IjklTypeSpec* type_specification;
            struct IjklAstNode* initial_value_expression;
        } variable_declaration;

        /* AST_NODE_DECL_ROUTINE */
        struct {
            const char* routine_name;
            bool is_function;
            bool is_public;
            IjklTypeSpec* return_type_spec;
            struct IjklAstNode* parameters;
            struct IjklAstNode* routine_body;
        } routine_declaration;

        /* AST_NODE_PARAMETER */
        struct {
            const char* parameter_name;
            IjklTypeSpec* type_specification;
            bool is_by_reference;
        } parameter;

        /* AST_NODE_STMT_BLOCK */
        struct {
            struct IjklAstNode* statements;
        } block_statement;

        /* AST_NODE_STMT_ASSIGN */
        struct {
            struct IjklAstNode* target_lvalue;
            struct IjklAstNode* value_expression;
            bool is_pascal_assignment;
        } assignment_statement;

        /* AST_NODE_STMT_CALL */
        struct {
            struct IjklAstNode* callable_expression;
            struct IjklAstNode* argument_expressions;
        } call_statement;

        /* AST_NODE_STMT_IF */
        struct {
            struct IjklAstNode* condition_expression;
            struct IjklAstNode* then_branch_statement;
            struct IjklAstNode* else_branch_statement;
        } if_statement;

        /* AST_NODE_STMT_SWITCH */
        struct {
            struct IjklAstNode* condition_expression;
            struct IjklAstNode* case_branches;
            struct IjklAstNode* default_branch;
        } switch_statement;

        /* AST_NODE_CASE_BRANCH */
        struct {
            struct IjklAstNode* match_expression;
            struct IjklAstNode* statements;
        } case_branch;

        /* AST_NODE_STMT_WHILE */
        struct {
            struct IjklAstNode* condition_expression;
            struct IjklAstNode* body_statement;
        } while_statement;

        /* AST_NODE_STMT_FOR */
        struct {
            const char* control_variable_name;
            IjklTypeSpec* control_variable_type;
            struct IjklAstNode* start_expression;
            struct IjklAstNode* end_expression;
            struct IjklAstNode* step_expression;
            struct IjklAstNode* body_statement;
        } for_statement;

        /* AST_NODE_STMT_REPEAT */
        struct {
            struct IjklAstNode* body_block;
            struct IjklAstNode* condition_expression;
        } repeat_statement;

        /* AST_NODE_STMT_RETURN */
        struct {
            struct IjklAstNode* return_value_expression;
        } return_statement;

        /* AST_NODE_STMT_READ */
        struct {
            struct IjklAstNode* target_lvalues;
        } read_statement;

        /* AST_NODE_STMT_WRITE */
        struct {
            bool is_newline;
            struct IjklAstNode* argument_expressions;
        } write_statement;

        /* AST_NODE_STMT_FREE */
        struct {
            struct IjklAstNode* target_lvalue;
        } free_statement;

        /* AST_NODE_STMT_ASM */
        struct {
            const char* target_builtin_name;
            const char* ci_code_text;
            size_t ci_code_length;
        } asm_statement;

        /* AST_NODE_EXPR_BINARY */
        struct {
            IjklTokenType operator_type;
            struct IjklAstNode* left_operand;
            struct IjklAstNode* right_operand;
        } binary_expression;

        /* AST_NODE_EXPR_UNARY */
        struct {
            IjklTokenType operator_type;
            struct IjklAstNode* operand;
        } unary_expression;

        /* AST_NODE_EXPR_DEREFERENCE (@operand) */
        struct {
            struct IjklAstNode* operand;
        } dereference_expression;

        /* AST_NODE_EXPR_MEMBER_ACCESS (target.member) */
        struct {
            struct IjklAstNode* target_expression;
            const char* member_name;
        } member_access_expression;

        /* AST_NODE_EXPR_INDEX_ACCESS (target[index]) */
        struct {
            struct IjklAstNode* target_expression;
            struct IjklAstNode* index_expression;
        } index_access_expression;

        /* AST_NODE_EXPR_CALL (target(args)) */
        struct {
            struct IjklAstNode* callable_expression;
            struct IjklAstNode* argument_expressions;
        } call_expression;

        /* AST_NODE_EXPR_ALLOC (reserve(bytes) / alloc(bytes)) */
        struct {
            struct IjklAstNode* byte_count_expression;
        } allocation_expression;

        /* Literais */
        struct {
            int64_t integer_value;
        } literal_integer;

        struct {
            double real_value;
        } literal_real;

        struct {
            float float_value;
        } literal_float;

        struct {
            char char_value;
        } literal_character;

        struct {
            const char* string_value;
        } literal_string;

        struct {
            bool boolean_value;
        } literal_boolean;

        /* Identificadores */
        struct {
            const char* identifier_name;
        } identifier_expression;
    } data;
} IjklAstNode;

/* Criação de nós e especificadores de tipo alocados na Arena de Memória */
IjklAstNode* ijkl_ast_node_create(IjklMemoryArena* memory_arena,
                                  IjklAstNodeKind node_kind,
                                  size_t line_number,
                                  size_t column_number);

IjklTypeSpec* ijkl_ast_type_spec_create(IjklMemoryArena* memory_arena,
                                        IjklTypeKind type_kind,
                                        size_t line_number,
                                        size_t column_number);

/* Retorna uma representação em texto legível de um especificador de tipo */
const char* ijkl_ast_type_spec_to_string(const IjklTypeSpec* type_specification,
                                         IjklMemoryArena* memory_arena);

/* Verifica igualdade estrutural estrita entre dois especificadores de tipo */
bool ijkl_ast_are_types_identical(const IjklTypeSpec* first_type, const IjklTypeSpec* second_type);

/* Imprime a árvore sintática completa formatada com traços hierárquicos ASCII */
void ijkl_ast_print(const IjklAstNode* root_node, FILE* output_stream);

#endif /* IJKL_AST_H */
