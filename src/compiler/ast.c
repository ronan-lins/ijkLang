#include "ast.h"

/*
 * ==============================================================================
 * MÓDULO: ÁRVORE SINTÁTICA ABSTRATA (AST - ABSTRACT SYNTAX TREE)
 * ==============================================================================
 * Implementa a alocação de nós e tipos na Arena de Memória, bem como
 * a formatação visual hierárquica em árvore ASCII com traços ('├──', '└──').
 *
 * Siglas técnicas empregadas:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

IjklAstNode* ijkl_ast_node_create(IjklMemoryArena* memory_arena,
                                  IjklAstNodeKind node_kind,
                                  size_t line_number,
                                  size_t column_number) {
    IjklAstNode* created_node;
    if (memory_arena) {
        created_node = (IjklAstNode*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklAstNode));
    } else {
        created_node = (IjklAstNode*)ijkl_memory_heap_allocate(sizeof(IjklAstNode));
    }

    memset(created_node, 0, sizeof(IjklAstNode));
    created_node->kind = node_kind;
    created_node->line_number = line_number;
    created_node->column_number = column_number;
    created_node->next_sibling = NULL;
    return created_node;
}

IjklTypeSpec* ijkl_ast_type_spec_create(IjklMemoryArena* memory_arena,
                                        IjklTypeKind type_kind,
                                        size_t line_number,
                                        size_t column_number) {
    IjklTypeSpec* type_specification;
    if (memory_arena) {
        type_specification = (IjklTypeSpec*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklTypeSpec));
    } else {
        type_specification = (IjklTypeSpec*)ijkl_memory_heap_allocate(sizeof(IjklTypeSpec));
    }

    memset(type_specification, 0, sizeof(IjklTypeSpec));
    type_specification->kind = type_kind;
    type_specification->line_number = line_number;
    type_specification->column_number = column_number;
    return type_specification;
}

const char* ijkl_ast_type_spec_to_string(const IjklTypeSpec* type_specification,
                                         IjklMemoryArena* memory_arena) {
    if (!type_specification) return "vazio";

    char formatting_buffer[256];
    switch (type_specification->kind) {
        case TYPE_KIND_PRIMITIVE:
            switch (type_specification->details.primitive_type) {
                case PRIMITIVE_INT:    return "inteiro";
                case PRIMITIVE_REAL:   return "real";
                case PRIMITIVE_FLOAT:  return "flutuante";
                case PRIMITIVE_CHAR:   return "caracter";
                case PRIMITIVE_STRING: return "texto";
                case PRIMITIVE_BOOL:   return "logico";
                default:               return "desconhecido";
            }
        case TYPE_KIND_USER_DEFINED:
            return type_specification->details.user_type_name ? type_specification->details.user_type_name : "tipo_definido";
        case TYPE_KIND_POINTER: {
            if (!type_specification->details.pointer_info.base_type) {
                return "ponteiro";
            }
            const char* base_type_string = ijkl_ast_type_spec_to_string(type_specification->details.pointer_info.base_type, memory_arena);
            snprintf(formatting_buffer, sizeof(formatting_buffer), "ponteiro %s", base_type_string);
            return memory_arena ? ijkl_memory_arena_duplicate_string(memory_arena, formatting_buffer) : ijkl_strdup(formatting_buffer);
        }
        case TYPE_KIND_ARRAY: {
            const char* element_type_string = ijkl_ast_type_spec_to_string(type_specification->details.array_info.element_type, memory_arena);
            snprintf(formatting_buffer, sizeof(formatting_buffer), "arranjo[%zu] %s", type_specification->details.array_info.fixed_element_count, element_type_string);
            return memory_arena ? ijkl_memory_arena_duplicate_string(memory_arena, formatting_buffer) : ijkl_strdup(formatting_buffer);
        }
        case TYPE_KIND_STRUCT:
            return "registro { ... }";
        default:
            return "indefinido";
    }
}

bool ijkl_ast_are_types_identical(const IjklTypeSpec* first_type, const IjklTypeSpec* second_type) {
    if (first_type == second_type) return true;
    if (!first_type || !second_type) return false;

    if (first_type->kind != second_type->kind) return false;

    switch (first_type->kind) {
        case TYPE_KIND_PRIMITIVE:
            return first_type->details.primitive_type == second_type->details.primitive_type;

        case TYPE_KIND_POINTER:
            return ijkl_ast_are_types_identical(first_type->details.pointer_info.base_type,
                                                second_type->details.pointer_info.base_type);

        case TYPE_KIND_ARRAY:
            if (first_type->details.array_info.fixed_element_count !=
                second_type->details.array_info.fixed_element_count) {
                return false;
            }
            return ijkl_ast_are_types_identical(first_type->details.array_info.element_type,
                                                second_type->details.array_info.element_type);

        case TYPE_KIND_USER_DEFINED:
            if (!first_type->details.user_type_name || !second_type->details.user_type_name) {
                return false;
            }
            if (ijkl_string_case_insensitive_equals(first_type->details.user_type_name,
                                                   second_type->details.user_type_name)) {
                return true;
            }
            if ((strcmp(first_type->details.user_type_name, "DateTime") == 0 && strcmp(second_type->details.user_type_name, "DataHora") == 0) ||
                (strcmp(first_type->details.user_type_name, "DataHora") == 0 && strcmp(second_type->details.user_type_name, "DateTime") == 0) ||
                (strcmp(first_type->details.user_type_name, "DateTime") == 0 && strcmp(second_type->details.user_type_name, "DateHeure") == 0) ||
                (strcmp(first_type->details.user_type_name, "DateHeure") == 0 && strcmp(second_type->details.user_type_name, "DateTime") == 0)) {
                return true;
            }
            return false;

        case TYPE_KIND_STRUCT:
            return first_type == second_type;

        default:
            return false;
    }
}

static void ast_print_node(const IjklAstNode* current_node,
                           FILE* output_stream,
                           const char* indentation_prefix,
                           bool is_last_sibling);

static void print_hierarchical_indentation(FILE* output_stream, const char* indentation_prefix, bool is_last_sibling) {
    fprintf(output_stream, "%s%s", indentation_prefix, is_last_sibling ? "└── " : "├── ");
}

/* Cria o prefixo de indentação para os nós filhos baseado na condição de último irmão do pai */
static char* create_child_indentation_prefix(const char* current_indentation_prefix, bool is_last_sibling) {
    size_t current_prefix_length = strlen(current_indentation_prefix);
    char* new_indentation_prefix = (char*)malloc(current_prefix_length + 5);
    if (!new_indentation_prefix) return NULL;
    snprintf(new_indentation_prefix, current_prefix_length + 5, "%s%s", current_indentation_prefix, is_last_sibling ? "    " : "│   ");
    return new_indentation_prefix;
}

static void ast_print_child_nodes(const IjklAstNode* first_child_node,
                                  FILE* output_stream,
                                  const char* parent_indentation_prefix,
                                  bool is_parent_last_sibling) {
    if (!first_child_node) return;

    char* child_indentation_prefix = create_child_indentation_prefix(parent_indentation_prefix, is_parent_last_sibling);
    if (!child_indentation_prefix) return;

    const IjklAstNode* current_child = first_child_node;
    while (current_child) {
        bool is_child_last = (current_child->next_sibling == NULL);
        ast_print_node(current_child, output_stream, child_indentation_prefix, is_child_last);
        current_child = current_child->next_sibling;
    }

    free(child_indentation_prefix);
}

static void ast_print_node(const IjklAstNode* current_node,
                           FILE* output_stream,
                           const char* indentation_prefix,
                           bool is_last_sibling) {
    if (!current_node) return;

    print_hierarchical_indentation(output_stream, indentation_prefix, is_last_sibling);

    switch (current_node->kind) {
        case AST_NODE_PROGRAM:
            fprintf(output_stream, "Programa: %s\n", current_node->data.program.program_name ? current_node->data.program.program_name : "<anonimo>");
            if (current_node->data.program.imports) {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Importacoes:\n");
                ast_print_child_nodes(current_node->data.program.imports, output_stream, child_prefix, false);
                free(child_prefix);
            }
            if (current_node->data.program.declarations) {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, current_node->data.program.entry_point == NULL);
                fprintf(output_stream, "Declaracoes:\n");
                ast_print_child_nodes(current_node->data.program.declarations, output_stream, child_prefix, current_node->data.program.entry_point == NULL);
                free(child_prefix);
            }
            if (current_node->data.program.entry_point) {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                ast_print_node(current_node->data.program.entry_point, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_MODULE:
            fprintf(output_stream, "Modulo: %s\n", current_node->data.module_declaration.qualified_module_path ? current_node->data.module_declaration.qualified_module_path : "<caminho>");
            if (current_node->data.module_declaration.imports) {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Importacoes:\n");
                ast_print_child_nodes(current_node->data.module_declaration.imports, output_stream, child_prefix, false);
                free(child_prefix);
            }
            if (current_node->data.module_declaration.declarations) {
                ast_print_child_nodes(current_node->data.module_declaration.declarations, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_IMPORT:
            fprintf(output_stream, "Importacao: %s\n", current_node->data.import_declaration.module_path ? current_node->data.import_declaration.module_path : "<caminho>");
            break;

        case AST_NODE_ENTRY_POINT:
            fprintf(output_stream, "PontoDeEntrada (Execucao Principal)\n");
            if (current_node->data.entry_point.executable_statements) {
                ast_print_child_nodes(current_node->data.entry_point.executable_statements, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_DECL_CONST:
            fprintf(output_stream, "Constante: %s", current_node->data.constant_declaration.constant_name);
            if (current_node->data.constant_declaration.type_specification) {
                fprintf(output_stream, " : %s", ijkl_ast_type_spec_to_string(current_node->data.constant_declaration.type_specification, NULL));
            }
            fprintf(output_stream, "\n");
            if (current_node->data.constant_declaration.initial_value_expression) {
                ast_print_child_nodes(current_node->data.constant_declaration.initial_value_expression, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_DECL_TYPE:
            fprintf(output_stream, "Tipo: %s = %s\n",
                    current_node->data.type_declaration.defined_type_name,
                    ijkl_ast_type_spec_to_string(current_node->data.type_declaration.type_specification, NULL));
            if (current_node->data.type_declaration.type_specification &&
                current_node->data.type_declaration.type_specification->kind == TYPE_KIND_STRUCT &&
                current_node->data.type_declaration.type_specification->details.struct_info.field_declarations) {
                ast_print_child_nodes(current_node->data.type_declaration.type_specification->details.struct_info.field_declarations, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_DECL_VAR:
            fprintf(output_stream, "Variavel: %s : %s\n",
                    current_node->data.variable_declaration.variable_name,
                    ijkl_ast_type_spec_to_string(current_node->data.variable_declaration.type_specification, NULL));
            if (current_node->data.variable_declaration.initial_value_expression) {
                ast_print_child_nodes(current_node->data.variable_declaration.initial_value_expression, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_DECL_ROUTINE:
            if (current_node->data.routine_declaration.is_function) {
                fprintf(output_stream, "Funcao [%s]: %s (retorna %s)\n",
                        current_node->data.routine_declaration.is_public ? "publico" : "privado",
                        current_node->data.routine_declaration.routine_name,
                        ijkl_ast_type_spec_to_string(current_node->data.routine_declaration.return_type_spec, NULL));
            } else {
                fprintf(output_stream, "Procedimento [%s]: %s\n",
                        current_node->data.routine_declaration.is_public ? "publico" : "privado",
                        current_node->data.routine_declaration.routine_name);
            }
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                if (current_node->data.routine_declaration.parameters) {
                    print_hierarchical_indentation(output_stream, child_prefix, current_node->data.routine_declaration.routine_body == NULL);
                    fprintf(output_stream, "Parametros:\n");
                    ast_print_child_nodes(current_node->data.routine_declaration.parameters, output_stream, child_prefix, current_node->data.routine_declaration.routine_body == NULL);
                }
                if (current_node->data.routine_declaration.routine_body) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "Corpo:\n");
                    ast_print_child_nodes(current_node->data.routine_declaration.routine_body, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_PARAMETER:
            fprintf(output_stream, "Parametro: %s%s : %s\n",
                    current_node->data.parameter.is_by_reference ? "ref " : "",
                    current_node->data.parameter.parameter_name,
                    ijkl_ast_type_spec_to_string(current_node->data.parameter.type_specification, NULL));
            break;

        case AST_NODE_STMT_BLOCK:
            fprintf(output_stream, "Bloco { ... }\n");
            if (current_node->data.block_statement.statements) {
                ast_print_child_nodes(current_node->data.block_statement.statements, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_STMT_ASSIGN:
            fprintf(output_stream, "Atribuicao (%s)\n", current_node->data.assignment_statement.is_pascal_assignment ? ":=" : "=");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Alvo:\n");
                ast_print_child_nodes(current_node->data.assignment_statement.target_lvalue, output_stream, child_prefix, false);

                print_hierarchical_indentation(output_stream, child_prefix, true);
                fprintf(output_stream, "Valor:\n");
                ast_print_child_nodes(current_node->data.assignment_statement.value_expression, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_CALL:
            fprintf(output_stream, "ChamadaProcedimento\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, current_node->data.call_statement.argument_expressions == NULL);
                fprintf(output_stream, "Rotina:\n");
                ast_print_child_nodes(current_node->data.call_statement.callable_expression, output_stream, child_prefix, current_node->data.call_statement.argument_expressions == NULL);

                if (current_node->data.call_statement.argument_expressions) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "Argumentos:\n");
                    ast_print_child_nodes(current_node->data.call_statement.argument_expressions, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_IF:
            fprintf(output_stream, "ComandoSe\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Condicao:\n");
                ast_print_child_nodes(current_node->data.if_statement.condition_expression, output_stream, child_prefix, false);

                print_hierarchical_indentation(output_stream, child_prefix, current_node->data.if_statement.else_branch_statement == NULL);
                fprintf(output_stream, "Entao:\n");
                ast_print_child_nodes(current_node->data.if_statement.then_branch_statement, output_stream, child_prefix, current_node->data.if_statement.else_branch_statement == NULL);

                if (current_node->data.if_statement.else_branch_statement) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "Senao:\n");
                    ast_print_child_nodes(current_node->data.if_statement.else_branch_statement, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_SWITCH:
            fprintf(output_stream, "ComandoEscolha\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "ExpressaoAlvo:\n");
                ast_print_child_nodes(current_node->data.switch_statement.condition_expression, output_stream, child_prefix, false);

                if (current_node->data.switch_statement.case_branches) {
                    ast_print_child_nodes(current_node->data.switch_statement.case_branches, output_stream, child_prefix, current_node->data.switch_statement.default_branch == NULL);
                }
                if (current_node->data.switch_statement.default_branch) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "CasoPadrao:\n");
                    ast_print_child_nodes(current_node->data.switch_statement.default_branch, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_CASE_BRANCH:
            fprintf(output_stream, "Caso:\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Valor:\n");
                ast_print_child_nodes(current_node->data.case_branch.match_expression, output_stream, child_prefix, false);

                if (current_node->data.case_branch.statements) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "Comandos:\n");
                    ast_print_child_nodes(current_node->data.case_branch.statements, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_WHILE:
            fprintf(output_stream, "ComandoEnquanto\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Condicao:\n");
                ast_print_child_nodes(current_node->data.while_statement.condition_expression, output_stream, child_prefix, false);

                print_hierarchical_indentation(output_stream, child_prefix, true);
                fprintf(output_stream, "Corpo:\n");
                ast_print_child_nodes(current_node->data.while_statement.body_statement, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_FOR:
            fprintf(output_stream, "ComandoPara (%s)\n", current_node->data.for_statement.control_variable_name);
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Inicio:\n");
                ast_print_child_nodes(current_node->data.for_statement.start_expression, output_stream, child_prefix, false);

                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Limite:\n");
                ast_print_child_nodes(current_node->data.for_statement.end_expression, output_stream, child_prefix, false);

                if (current_node->data.for_statement.step_expression) {
                    print_hierarchical_indentation(output_stream, child_prefix, false);
                    fprintf(output_stream, "Passo:\n");
                    ast_print_child_nodes(current_node->data.for_statement.step_expression, output_stream, child_prefix, false);
                }

                print_hierarchical_indentation(output_stream, child_prefix, true);
                fprintf(output_stream, "Corpo:\n");
                ast_print_child_nodes(current_node->data.for_statement.body_statement, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_REPEAT:
            fprintf(output_stream, "ComandoRepita\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, false);
                fprintf(output_stream, "Corpo:\n");
                ast_print_child_nodes(current_node->data.repeat_statement.body_block, output_stream, child_prefix, false);

                print_hierarchical_indentation(output_stream, child_prefix, true);
                fprintf(output_stream, "CondicaoEnquanto:\n");
                ast_print_child_nodes(current_node->data.repeat_statement.condition_expression, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_STMT_BREAK:
            fprintf(output_stream, "ComandoAborte\n");
            break;

        case AST_NODE_STMT_RETURN:
            fprintf(output_stream, "ComandoRetorne\n");
            if (current_node->data.return_statement.return_value_expression) {
                ast_print_child_nodes(current_node->data.return_statement.return_value_expression, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_STMT_READ:
            fprintf(output_stream, "ComandoLeia\n");
            if (current_node->data.read_statement.target_lvalues) {
                ast_print_child_nodes(current_node->data.read_statement.target_lvalues, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_STMT_WRITE:
            fprintf(output_stream, "%s\n", current_node->data.write_statement.is_newline ? "ComandoEscrevaLn" : "ComandoEscreva");
            if (current_node->data.write_statement.argument_expressions) {
                ast_print_child_nodes(current_node->data.write_statement.argument_expressions, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_STMT_FREE:
            fprintf(output_stream, "ComandoLibere\n");
            if (current_node->data.free_statement.target_lvalue) {
                ast_print_child_nodes(current_node->data.free_statement.target_lvalue, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_STMT_ASM:
            if (current_node->data.asm_statement.target_builtin_name) {
                fprintf(output_stream, "ComandoAsm: %s\n", current_node->data.asm_statement.target_builtin_name);
            } else {
                fprintf(output_stream, "ComandoAsm\n");
            }
            break;

        case AST_NODE_EXPR_BINARY:
            fprintf(output_stream, "OperadorBinario: %s\n", ijkl_token_type_name(current_node->data.binary_expression.operator_type));
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                ast_print_node(current_node->data.binary_expression.left_operand, output_stream, child_prefix, false);
                ast_print_node(current_node->data.binary_expression.right_operand, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_EXPR_UNARY:
            fprintf(output_stream, "OperadorUnario: %s\n", ijkl_token_type_name(current_node->data.unary_expression.operator_type));
            if (current_node->data.unary_expression.operand) {
                ast_print_child_nodes(current_node->data.unary_expression.operand, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_EXPR_DEREFERENCE:
            fprintf(output_stream, "Desreferenciacao (@)\n");
            if (current_node->data.dereference_expression.operand) {
                ast_print_child_nodes(current_node->data.dereference_expression.operand, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_EXPR_MEMBER_ACCESS:
            fprintf(output_stream, "AcessoMembro: .%s\n", current_node->data.member_access_expression.member_name);
            if (current_node->data.member_access_expression.target_expression) {
                ast_print_child_nodes(current_node->data.member_access_expression.target_expression, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_EXPR_INDEX_ACCESS:
            fprintf(output_stream, "IndexacaoArranjo [ ]\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                ast_print_node(current_node->data.index_access_expression.target_expression, output_stream, child_prefix, false);
                ast_print_node(current_node->data.index_access_expression.index_expression, output_stream, child_prefix, true);
                free(child_prefix);
            }
            break;

        case AST_NODE_EXPR_CALL:
            fprintf(output_stream, "ChamadaFuncao\n");
            {
                char* child_prefix = create_child_indentation_prefix(indentation_prefix, is_last_sibling);
                print_hierarchical_indentation(output_stream, child_prefix, current_node->data.call_expression.argument_expressions == NULL);
                fprintf(output_stream, "Alvo:\n");
                ast_print_child_nodes(current_node->data.call_expression.callable_expression, output_stream, child_prefix, current_node->data.call_expression.argument_expressions == NULL);

                if (current_node->data.call_expression.argument_expressions) {
                    print_hierarchical_indentation(output_stream, child_prefix, true);
                    fprintf(output_stream, "Argumentos:\n");
                    ast_print_child_nodes(current_node->data.call_expression.argument_expressions, output_stream, child_prefix, true);
                }
                free(child_prefix);
            }
            break;

        case AST_NODE_EXPR_ALLOC:
            fprintf(output_stream, "AlocacaoDinamica: reserve(bytes)\n");
            if (current_node->data.allocation_expression.byte_count_expression) {
                ast_print_child_nodes(current_node->data.allocation_expression.byte_count_expression, output_stream, indentation_prefix, is_last_sibling);
            }
            break;

        case AST_NODE_EXPR_LITERAL_INT:
            fprintf(output_stream, "LiteralInteiro: %lld\n", (long long)current_node->data.literal_integer.integer_value);
            break;

        case AST_NODE_EXPR_LITERAL_REAL:
            fprintf(output_stream, "LiteralReal: %g\n", current_node->data.literal_real.real_value);
            break;

        case AST_NODE_EXPR_LITERAL_FLOAT:
            fprintf(output_stream, "LiteralFlutuante: %gf\n", current_node->data.literal_float.float_value);
            break;

        case AST_NODE_EXPR_LITERAL_CHAR:
            fprintf(output_stream, "LiteralCaracter: '%c'\n", current_node->data.literal_character.char_value);
            break;

        case AST_NODE_EXPR_LITERAL_STRING:
            fprintf(output_stream, "LiteralTexto: \"%s\"\n", current_node->data.literal_string.string_value ? current_node->data.literal_string.string_value : "");
            break;

        case AST_NODE_EXPR_LITERAL_BOOL:
            fprintf(output_stream, "LiteralLogico: %s\n", current_node->data.literal_boolean.boolean_value ? "verdadeiro" : "falso");
            break;

        case AST_NODE_EXPR_LITERAL_NULL:
            fprintf(output_stream, "LiteralNulo\n");
            break;

        case AST_NODE_EXPR_IDENTIFIER:
            fprintf(output_stream, "Identificador: %s\n", current_node->data.identifier_expression.identifier_name);
            break;

        default:
            fprintf(output_stream, "NoAST_Desconhecido\n");
            break;
    }
}

void ijkl_ast_print(const IjklAstNode* root_node, FILE* output_stream) {
    if (!root_node) return;
    FILE* effective_stream = output_stream ? output_stream : stdout;
    ast_print_node(root_node, effective_stream, "", true);
}
