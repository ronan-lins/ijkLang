#include "semantic.h"
#include "vocabulary.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * ==============================================================================
 * MÓDULO: ANALISADOR SEMÂNTICO E VERIFICADOR DE TIPOS DA IJKLANG
 * ==============================================================================
 * Implementa a verificação de regras semânticas e inferência estática de tipos
 * em conformidade com o padrão ISO C11.
 *
 * Principais responsabilidades:
 * - Validação estrita de compatibilidade de tipos em expressões e atribuições.
 * - Resolução de identificadores nos escopos léxicos estáticos.
 * - Proibição de atribuições em expressões condicionais.
 * - Verificação de argumentos em chamadas de funções e procedimentos.
 * - Validação de comandos de controle de fluxo ('aborte' / 'break').
 * - Acesso e desreferenciação de memória (&, @, $).
 * ==============================================================================
 */

/* Utilitário para emissão formatada de mensagens de erro semântico */
static void report_semantic_error(IjklSemanticAnalyzer* analyzer,
                                  size_t line_number,
                                  size_t column_number,
                                  const char* format_string, ...) {
    analyzer->total_errors_count++;
    va_list arguments;
    va_start(arguments, format_string);
    fprintf(stderr, "%zu:%zu: Semantic error: ", line_number, column_number);
    vfprintf(stderr, format_string, arguments);
    fprintf(stderr, "\n");
    va_end(arguments);
}

/* Utilitário para emissão formatada de alertas semânticos não-bloqueantes */
static void report_semantic_warning(IjklSemanticAnalyzer* analyzer,
                                    size_t line_number,
                                    size_t column_number,
                                    const char* format_string, ...) {
    analyzer->total_warnings_count++;
    va_list arguments;
    va_start(arguments, format_string);
    fprintf(stderr, "%zu:%zu: Semantic warning: ", line_number, column_number);
    vfprintf(stderr, format_string, arguments);
    fprintf(stderr, "\n");
    va_end(arguments);
}

/* Criação de um especificador de tipo primitivo na memória arena */
static IjklTypeSpec* create_primitive_type(IjklSemanticAnalyzer* analyzer,
                                          IjklPrimitiveType primitive_type) {
    IjklTypeSpec* type_spec = ijkl_ast_type_spec_create(analyzer->memory_arena,
                                                        TYPE_KIND_PRIMITIVE,
                                                        0, 0);
    type_spec->details.primitive_type = primitive_type;
    return type_spec;
}

/* Criação de um especificador de tipo ponteiro na memória arena */
static IjklTypeSpec* create_pointer_type(IjklSemanticAnalyzer* analyzer,
                                        IjklTypeSpec* base_type) {
    IjklTypeSpec* type_spec = ijkl_ast_type_spec_create(analyzer->memory_arena,
                                                        TYPE_KIND_POINTER,
                                                        0, 0);
    type_spec->details.pointer_info.base_type = base_type;
    return type_spec;
}

/* Criação do analisador semântico */
IjklSemanticAnalyzer* ijkl_semantic_analyzer_create(IjklMemoryArena* memory_arena,
                                                   IjklSymbolTable* symbol_table) {
    if (!memory_arena || !symbol_table) return NULL;

    IjklSemanticAnalyzer* analyzer = (IjklSemanticAnalyzer*)ijkl_memory_arena_allocate(
        memory_arena, sizeof(IjklSemanticAnalyzer));

    analyzer->memory_arena = memory_arena;
    analyzer->symbol_table = symbol_table;
    analyzer->vocabulary = NULL;
    analyzer->total_errors_count = 0;
    analyzer->total_warnings_count = 0;
    analyzer->loop_nesting_depth = 0;
    analyzer->switch_nesting_depth = 0;
    analyzer->current_routine_node = NULL;

    return analyzer;
}

void ijkl_semantic_analyzer_set_vocabulary(IjklSemanticAnalyzer* analyzer,
                                           const struct IjklVocabulary* vocabulary) {
    if (analyzer) {
        analyzer->vocabulary = vocabulary;
    }
}

/* Verifica se um tipo é estritamente numérico (inteiro, real, flutuante ou caractere) */
static bool is_numeric_type(const IjklTypeSpec* type_specification) {
    if (!type_specification) return false;
    if (type_specification->kind != TYPE_KIND_PRIMITIVE) return false;
    IjklPrimitiveType primitive = type_specification->details.primitive_type;
    return (primitive == PRIMITIVE_INT || primitive == PRIMITIVE_REAL || primitive == PRIMITIVE_FLOAT || primitive == PRIMITIVE_CHAR);
}

/* Verifica se um tipo é booleano */
static bool is_boolean_type(const IjklTypeSpec* type_specification) {
    if (!type_specification) return false;
    return (type_specification->kind == TYPE_KIND_PRIMITIVE &&
            type_specification->details.primitive_type == PRIMITIVE_BOOL);
}

/* Verifica se um tipo é do tipo primitivo texto */
static bool is_string_type(const IjklTypeSpec* type_specification) {
    if (!type_specification) return false;
    return (type_specification->kind == TYPE_KIND_PRIMITIVE &&
            type_specification->details.primitive_type == PRIMITIVE_STRING);
}

/* Verifica igualdade estrutural entre dois tipos */
static bool are_types_identical(const IjklTypeSpec* first_type, const IjklTypeSpec* second_type) {
    return ijkl_ast_are_types_identical(first_type, second_type);
}

/* Verifica compatibilidade semântica com promoção didática de tipos */
bool ijkl_semantic_are_types_compatible(const IjklTypeSpec* target_type,
                                       const IjklTypeSpec* source_type) {
    if (!target_type || !source_type) return true;

    /* Igualdade estrutural direta */
    if (are_types_identical(target_type, source_type)) {
        return true;
    }

    IjklPrimitiveType target_boxed_prim = PRIMITIVE_INT;
    bool target_is_boxed = ijkl_type_is_boxed_primitive(target_type, &target_boxed_prim);

    IjklPrimitiveType source_boxed_prim = PRIMITIVE_INT;
    bool source_is_boxed = ijkl_type_is_boxed_primitive(source_type, &source_boxed_prim);

    /* Dois tipos encapsulados equivalentes (ex.: Integer e Inteiro) */
    if (target_is_boxed && source_is_boxed) {
        return target_boxed_prim == source_boxed_prim;
    }

    /* Auto-boxing: Primitivo para Boxed Type */
    if (target_is_boxed && source_type->kind == TYPE_KIND_PRIMITIVE) {
        if (source_type->details.primitive_type == target_boxed_prim) {
            return true;
        }
        /* Promoção numérica didática: int -> Boxed Real */
        if (target_boxed_prim == PRIMITIVE_REAL && source_type->details.primitive_type == PRIMITIVE_INT) {
            return true;
        }
    }

    /* Tipo Boxed para ponteiro genérico ou null */
    if (target_is_boxed && source_type->kind == TYPE_KIND_POINTER && !source_type->details.pointer_info.base_type) {
        return true; /* Integer y = null; */
    }

    /* Auto-boxing na passagem para ponteiro genérico (ex.: Collections.add(lista, 42)) */
    if (target_type->kind == TYPE_KIND_POINTER && !target_type->details.pointer_info.base_type) {
        if (source_is_boxed) {
            return true;
        }
        if (source_type->kind == TYPE_KIND_PRIMITIVE) {
            return true;
        }
    }

    /* Desencapsulamento implícito (Unboxing): ESTRITAMENTE PROIBIDO */
    if (target_type->kind == TYPE_KIND_PRIMITIVE && source_is_boxed) {
        return false;
    }

    /* Compatibilidade de tipos por referência (user-defined pointer struct) com null / ponteiro genérico */
    if (target_type->kind == TYPE_KIND_USER_DEFINED && source_type->kind == TYPE_KIND_POINTER && !source_type->details.pointer_info.base_type) {
        return true;
    }
    if (source_type->kind == TYPE_KIND_USER_DEFINED && target_type->kind == TYPE_KIND_POINTER && !target_type->details.pointer_info.base_type) {
        return true;
    }

    /* Promoção implícita válida: inteiro para real ou flutuante */
    if (target_type->kind == TYPE_KIND_PRIMITIVE && source_type->kind == TYPE_KIND_PRIMITIVE) {
        if ((target_type->details.primitive_type == PRIMITIVE_REAL ||
             target_type->details.primitive_type == PRIMITIVE_FLOAT) &&
            source_type->details.primitive_type == PRIMITIVE_INT) {
            return true;
        }

        /* Promoção e coerção didática entre ponto flutuante e real */
        if ((target_type->details.primitive_type == PRIMITIVE_REAL &&
             source_type->details.primitive_type == PRIMITIVE_FLOAT) ||
            (target_type->details.primitive_type == PRIMITIVE_FLOAT &&
             source_type->details.primitive_type == PRIMITIVE_REAL)) {
            return true;
        }

        /* Compatibilidade didática: literal de texto atribuível a variável caractere */
        if (target_type->details.primitive_type == PRIMITIVE_CHAR &&
            source_type->details.primitive_type == PRIMITIVE_STRING) {
            return true;
        }

        /* Promoção implícita: caractere para inteiro */
        if (target_type->details.primitive_type == PRIMITIVE_INT &&
            source_type->details.primitive_type == PRIMITIVE_CHAR) {
            return true;
        }
    }

    /* Ponteiros: ponteiro genérico/nulo é compatível com qualquer ponteiro */
    if (target_type->kind == TYPE_KIND_POINTER && source_type->kind == TYPE_KIND_POINTER) {
        if (!source_type->details.pointer_info.base_type ||
            !target_type->details.pointer_info.base_type) {
            return true;
        }
        return are_types_identical(target_type->details.pointer_info.base_type,
                                   source_type->details.pointer_info.base_type);
    }

    /* Compatibilidade entre inteiros e ponteiros (endereçamento de memória) */
    if ((target_type->kind == TYPE_KIND_POINTER && source_type->kind == TYPE_KIND_PRIMITIVE && source_type->details.primitive_type == PRIMITIVE_INT) ||
        (target_type->kind == TYPE_KIND_PRIMITIVE && target_type->details.primitive_type == PRIMITIVE_INT && source_type->kind == TYPE_KIND_POINTER)) {
        return true;
    }

    /* Compatibilidade entre ponteiro genérico e texto (endereçamento de string na memória linear) */
    if ((target_type->kind == TYPE_KIND_POINTER && !target_type->details.pointer_info.base_type &&
         source_type->kind == TYPE_KIND_PRIMITIVE && source_type->details.primitive_type == PRIMITIVE_STRING) ||
        (target_type->kind == TYPE_KIND_PRIMITIVE && target_type->details.primitive_type == PRIMITIVE_STRING &&
         source_type->kind == TYPE_KIND_POINTER && !source_type->details.pointer_info.base_type)) {
        return true;
    }

    return false;
}

/* Helper para recompor identificadores qualificados pontuados (ex.: 'GeometriaApp.Calculos.areaRetangulo') */
static void build_qualified_identifier_name(const IjklAstNode* node, char* buffer, size_t buffer_size) {
    if (!node || !buffer || buffer_size == 0) return;
    if (node->kind == AST_NODE_EXPR_IDENTIFIER) {
        snprintf(buffer, buffer_size, "%s", node->data.identifier_expression.identifier_name);
    } else if (node->kind == AST_NODE_EXPR_MEMBER_ACCESS) {
        char prefix[256] = "";
        build_qualified_identifier_name(node->data.member_access_expression.target_expression, prefix, sizeof(prefix));
        if (prefix[0] != '\0') {
            snprintf(buffer, buffer_size, "%s.%s", prefix, node->data.member_access_expression.member_name);
        } else {
            snprintf(buffer, buffer_size, "%s", node->data.member_access_expression.member_name);
        }
    }
}

/* Verifica se uma expressão da AST representa um l-value gravável */
static bool is_valid_lvalue_expression(const IjklAstNode* expression_node) {
    if (!expression_node) return false;

    switch (expression_node->kind) {
        case AST_NODE_EXPR_IDENTIFIER:
        case AST_NODE_EXPR_MEMBER_ACCESS:
        case AST_NODE_EXPR_INDEX_ACCESS:
        case AST_NODE_EXPR_DEREFERENCE:
            return true;
        default:
            return false;
    }
}

/*
 * EFEITO COLATERAL:
 * Resolve estaticamente a sobrecarga de rotina invocada em conformidade com os tipos
 * dos argumentos passados, registrando o símbolo resolvido diretamente no nó da AST.
 */
static IjklSymbol* resolve_routine_invocation(IjklSemanticAnalyzer* analyzer,
                                              const IjklAstNode* call_node,
                                              const IjklAstNode* callable_expression,
                                              const IjklAstNode* argument_expressions) {
    char callee_qualified_name[256] = "";
    build_qualified_identifier_name(callable_expression, callee_qualified_name, sizeof(callee_qualified_name));

    if (callee_qualified_name[0] == '\0') {
        return NULL;
    }

    IjklSymbol* symbol = ijkl_symbol_table_lookup_qualified(analyzer->symbol_table, callee_qualified_name);
    if (!symbol) {
        symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, callee_qualified_name);
    }

    if (!symbol) {
        report_semantic_error(analyzer,
                              call_node->line_number,
                              call_node->column_number,
                              "Invocation of undeclared routine '%s'.", callee_qualified_name);
        return NULL;
    }

    if (symbol->symbol_kind != SYMBOL_KIND_FUNCTION &&
        symbol->symbol_kind != SYMBOL_KIND_PROCEDURE) {
        report_semantic_error(analyzer,
                              call_node->line_number,
                              call_node->column_number,
                              "Symbol '%s' is not a callable routine.", callee_qualified_name);
        return NULL;
    }

    /* Avalia e infere os tipos dos argumentos passados */
    size_t argument_count = 0;
    const IjklAstNode* current_arg = argument_expressions;
    while (current_arg) {
        argument_count++;
        current_arg = current_arg->next_sibling;
    }

    IjklTypeSpec** argument_types = NULL;
    if (argument_count > 0) {
        argument_types = (IjklTypeSpec**)ijkl_memory_arena_allocate(
            analyzer->memory_arena, sizeof(IjklTypeSpec*) * argument_count);
        size_t index = 0;
        current_arg = argument_expressions;
        while (current_arg) {
            argument_types[index++] = ijkl_semantic_analyze_expression(analyzer, current_arg);
            current_arg = current_arg->next_sibling;
        }
    }

    bool is_ambiguous = false;
    IjklSymbol* resolved_routine = ijkl_symbol_resolve_overload(symbol, argument_types, argument_count, &is_ambiguous);

    if (is_ambiguous) {
        report_semantic_error(analyzer,
                              call_node->line_number,
                              call_node->column_number,
                              "Call to overloaded routine '%s' is ambiguous with provided argument types.",
                              callee_qualified_name);
        return NULL;
    }

    if (!resolved_routine) {
        report_semantic_error(analyzer,
                              call_node->line_number,
                              call_node->column_number,
                              "No matching overload found for routine '%s' with %zu argument(s).",
                              callee_qualified_name,
                              argument_count);
        return NULL;
    }

    /* Validação estrita de argumentos para parâmetros por referência (ref) */
    if (resolved_routine && resolved_routine->declaration_node &&
        resolved_routine->declaration_node->kind == AST_NODE_DECL_ROUTINE) {
        const IjklAstNode* formal_parameter = resolved_routine->declaration_node->data.routine_declaration.parameters;
        const IjklAstNode* actual_argument = argument_expressions;
        size_t argument_index = 1;

        while (formal_parameter && actual_argument) {
            if (formal_parameter->kind == AST_NODE_PARAMETER && formal_parameter->data.parameter.is_by_reference) {
                if (!is_valid_lvalue_expression(actual_argument)) {
                    report_semantic_error(analyzer,
                                          actual_argument->line_number,
                                          actual_argument->column_number,
                                          "Argument %zu passed to 'ref' parameter '%s' of routine '%s' must be a valid l-value (literals and expressions are not allowed).",
                                          argument_index,
                                          formal_parameter->data.parameter.parameter_name,
                                          callee_qualified_name);
                } else if (actual_argument->kind == AST_NODE_EXPR_IDENTIFIER) {
                    const char* identifier_name = actual_argument->data.identifier_expression.identifier_name;
                    IjklSymbol* arg_symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, identifier_name);
                    if (arg_symbol && arg_symbol->symbol_kind == SYMBOL_KIND_CONSTANT) {
                        report_semantic_error(analyzer,
                                              actual_argument->line_number,
                                              actual_argument->column_number,
                                              "Cannot pass constant '%s' to 'ref' parameter '%s' of routine '%s'.",
                                              identifier_name,
                                              formal_parameter->data.parameter.parameter_name,
                                              callee_qualified_name);
                    }
                }
            }
            formal_parameter = formal_parameter->next_sibling;
            actual_argument = actual_argument->next_sibling;
            argument_index++;
        }
    }

    ((IjklAstNode*)call_node)->resolved_symbol = resolved_routine;
    return resolved_routine;
}

/*
 * EFEITO COLATERAL:
 * Analisa e infere o tipo estático resultante de uma expressão da AST.
 */
static IjklTypeSpec* analyze_expression_internal(IjklSemanticAnalyzer* analyzer,
                                                const IjklAstNode* expression_node) {
    if (!expression_node) return NULL;

    switch (expression_node->kind) {
        case AST_NODE_EXPR_LITERAL_INT:
            return create_primitive_type(analyzer, PRIMITIVE_INT);

        case AST_NODE_EXPR_LITERAL_REAL:
            return create_primitive_type(analyzer, PRIMITIVE_REAL);

        case AST_NODE_EXPR_LITERAL_FLOAT:
            return create_primitive_type(analyzer, PRIMITIVE_FLOAT);

        case AST_NODE_EXPR_LITERAL_CHAR:
            return create_primitive_type(analyzer, PRIMITIVE_CHAR);

        case AST_NODE_EXPR_LITERAL_STRING:
            return create_primitive_type(analyzer, PRIMITIVE_STRING);

        case AST_NODE_EXPR_LITERAL_BOOL:
            return create_primitive_type(analyzer, PRIMITIVE_BOOL);

        case AST_NODE_EXPR_LITERAL_NULL:
            return create_pointer_type(analyzer, NULL);

        case AST_NODE_EXPR_IDENTIFIER: {
            const char* name = expression_node->data.identifier_expression.identifier_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, name);
            if (!symbol) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Undeclared identifier '%s'.", name);
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }
            return symbol->type_specification;
        }

        case AST_NODE_EXPR_DEREFERENCE: {
            IjklTypeSpec* operand_type = ijkl_semantic_analyze_expression(
                analyzer, expression_node->data.dereference_expression.operand);
            IjklTypeSpec* resolved_operand_type = operand_type;
            if (resolved_operand_type && resolved_operand_type->kind == TYPE_KIND_USER_DEFINED &&
                resolved_operand_type->details.user_type_name) {
                IjklSymbol* type_symbol = ijkl_symbol_table_lookup_lexical(
                    analyzer->symbol_table, resolved_operand_type->details.user_type_name);
                if (type_symbol && type_symbol->type_specification) {
                    resolved_operand_type = type_symbol->type_specification;
                }
            }

            if (resolved_operand_type && resolved_operand_type->kind != TYPE_KIND_POINTER) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Dereference operator '@' requires a pointer operand.");
                return operand_type;
            }
            return resolved_operand_type ? resolved_operand_type->details.pointer_info.base_type : NULL;
        }

        case AST_NODE_EXPR_INDEX_ACCESS: {
            IjklTypeSpec* target_type = ijkl_semantic_analyze_expression(
                analyzer, expression_node->data.index_access_expression.target_expression);
            IjklTypeSpec* index_type = ijkl_semantic_analyze_expression(
                analyzer, expression_node->data.index_access_expression.index_expression);

            if (index_type && (index_type->kind != TYPE_KIND_PRIMITIVE ||
                               index_type->details.primitive_type != PRIMITIVE_INT)) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Array index expression must evaluate to integer type.");
            }

            if (target_type) {
                if (target_type->kind == TYPE_KIND_ARRAY) {
                    return target_type->details.array_info.element_type;
                } else if (target_type->kind == TYPE_KIND_POINTER) {
                    return target_type->details.pointer_info.base_type ? target_type->details.pointer_info.base_type : create_pointer_type(analyzer, NULL);
                } else {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Index access '[]' is only permitted on arrays or pointers.");
                }
            }
            return create_primitive_type(analyzer, PRIMITIVE_INT);
        }

        case AST_NODE_EXPR_MEMBER_ACCESS: {
            IjklTypeSpec* target_type = ijkl_semantic_analyze_expression(
                analyzer, expression_node->data.member_access_expression.target_expression);
            const char* member_name = expression_node->data.member_access_expression.member_name;

            if (target_type) {
                /* Acesso a membro de tipo encapsulado (.value ou .valor) */
                IjklPrimitiveType boxed_prim;
                if (ijkl_type_is_boxed_primitive(target_type, &boxed_prim)) {
                    if (strcmp(member_name, "value") == 0 || strcmp(member_name, "valor") == 0) {
                        return create_primitive_type(analyzer, boxed_prim);
                    }
                }

                /* Se for ponteiro para struct, auto-desreferencia para acesso a membro */
                if (target_type->kind == TYPE_KIND_POINTER && target_type->details.pointer_info.base_type) {
                    target_type = target_type->details.pointer_info.base_type;
                }

                /* Se for tipo definido pelo usuário, resolve o símbolo na tabela */
                if (target_type->kind == TYPE_KIND_USER_DEFINED && target_type->details.user_type_name) {
                    IjklSymbol* type_sym = ijkl_symbol_table_lookup_lexical(
                        analyzer->symbol_table, target_type->details.user_type_name);
                    if (type_sym && type_sym->type_specification) {
                        target_type = type_sym->type_specification;
                        if (target_type->kind == TYPE_KIND_POINTER && target_type->details.pointer_info.base_type) {
                            target_type = target_type->details.pointer_info.base_type;
                        }
                    }
                }

                if (target_type->kind == TYPE_KIND_STRUCT) {
                    const IjklAstNode* field = target_type->details.struct_info.field_declarations;
                    while (field) {
                        if (field->kind == AST_NODE_PARAMETER &&
                            strcmp(field->data.parameter.parameter_name, member_name) == 0) {
                            return field->data.parameter.type_specification;
                        }
                        if (field->kind == AST_NODE_DECL_VAR &&
                            strcmp(field->data.variable_declaration.variable_name, member_name) == 0) {
                            return field->data.variable_declaration.type_specification;
                        }
                        field = field->next_sibling;
                    }
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Field '%s' does not exist in struct type.", member_name);
                }
            }
            return create_primitive_type(analyzer, PRIMITIVE_INT);
        }

        case AST_NODE_EXPR_UNARY: {
            IjklTokenType operator_type = expression_node->data.unary_expression.operator_type;
            const IjklAstNode* operand = expression_node->data.unary_expression.operand;

            if (operator_type == TOKEN_AMPERSAND) {
                if (!is_valid_lvalue_expression(operand)) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Address-of operator '&' requires an l-value operand.");
                }
                IjklTypeSpec* operand_type = ijkl_semantic_analyze_expression(analyzer, operand);
                return create_pointer_type(analyzer, operand_type);
            }

            if (operator_type == TOKEN_DOLLAR) {
                /* Custo de memória em bytes sempre resulta em inteiro */
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }

            IjklTypeSpec* operand_type = ijkl_semantic_analyze_expression(analyzer, operand);

            if (operator_type == TOKEN_KW_NOT || operator_type == TOKEN_EXCLAMATION) {
                if (operand_type && !is_boolean_type(operand_type)) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Logical NOT operator requires a boolean operand.");
                }
                return create_primitive_type(analyzer, PRIMITIVE_BOOL);
            }

            if (operator_type == TOKEN_MINUS) {
                if (operand_type && !is_numeric_type(operand_type)) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Unary negation '-' requires a numeric operand.");
                }
                return operand_type;
            }

            return operand_type;
        }

        case AST_NODE_EXPR_BINARY: {
            IjklTokenType op = expression_node->data.binary_expression.operator_type;
            const IjklAstNode* left = expression_node->data.binary_expression.left_operand;
            const IjklAstNode* right = expression_node->data.binary_expression.right_operand;

            IjklTypeSpec* left_type = ijkl_semantic_analyze_expression(analyzer, left);
            IjklTypeSpec* right_type = ijkl_semantic_analyze_expression(analyzer, right);

            /* Operações Relacionais de Ordem (resultam em Booleano) */
            if (op == TOKEN_LESS || op == TOKEN_GREATER ||
                op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                bool both_numeric = (left_type && right_type && is_numeric_type(left_type) && is_numeric_type(right_type));
                bool both_string = (left_type && right_type && is_string_type(left_type) && is_string_type(right_type));
                if (!both_numeric && !both_string) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Relational comparison requires compatible numeric or text operands.");
                }
                return create_primitive_type(analyzer, PRIMITIVE_BOOL);
            }

            /* Operações Relacionais de Igualdade (resultam em Booleano) */
            if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL || op == TOKEN_NOT_EQUAL_PASCAL || op == TOKEN_EQUAL_STRICT) {
                if (op == TOKEN_EQUAL_STRICT && left_type && right_type &&
                    left_type->kind == TYPE_KIND_PRIMITIVE &&
                    left_type->details.primitive_type == PRIMITIVE_FLOAT) {
                    report_semantic_warning(analyzer,
                                            expression_node->line_number,
                                            expression_node->column_number,
                                            "Strict equality ('==') on binary floating-point numbers may cause precision anomalies.");
                }
                if (left_type && right_type &&
                    !ijkl_semantic_are_types_compatible(left_type, right_type) &&
                    !ijkl_semantic_are_types_compatible(right_type, left_type)) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Incompatible types in equality comparison.");
                }
                return create_primitive_type(analyzer, PRIMITIVE_BOOL);
            }

            /* Operações Lógicas (exigem operandos booleanos) */
            if (op == TOKEN_KW_AND || op == TOKEN_KW_OR ||
                op == TOKEN_LOGICAL_AND || op == TOKEN_LOGICAL_OR) {
                if (left_type && !is_boolean_type(left_type)) {
                    report_semantic_error(analyzer,
                                          left ? left->line_number : expression_node->line_number,
                                          left ? left->column_number : expression_node->column_number,
                                          "Logical operator requires boolean operands.");
                }
                if (right_type && !is_boolean_type(right_type)) {
                    report_semantic_error(analyzer,
                                          right ? right->line_number : expression_node->line_number,
                                          right ? right->column_number : expression_node->column_number,
                                          "Logical operator requires boolean operands.");
                }
                return create_primitive_type(analyzer, PRIMITIVE_BOOL);
            }

            /* Concatenação de texto e Adição com operador '+' */
            if (op == TOKEN_PLUS) {
                if ((left_type && is_string_type(left_type)) ||
                    (right_type && is_string_type(right_type))) {
                    return create_primitive_type(analyzer, PRIMITIVE_STRING);
                }

                if (left_type && right_type && (!is_numeric_type(left_type) || !is_numeric_type(right_type))) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Operator '+' requires numeric operands or at least one text operand.");
                    return create_primitive_type(analyzer, PRIMITIVE_INT);
                }

                if (left_type && right_type) {
                    if (left_type->details.primitive_type == PRIMITIVE_FLOAT ||
                        right_type->details.primitive_type == PRIMITIVE_FLOAT) {
                        return create_primitive_type(analyzer, PRIMITIVE_FLOAT);
                    }
                    if (left_type->details.primitive_type == PRIMITIVE_REAL ||
                        right_type->details.primitive_type == PRIMITIVE_REAL) {
                        return create_primitive_type(analyzer, PRIMITIVE_REAL);
                    }
                }
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }

            /* Operador de divisão ('/') resulta em tipo real ou ponto flutuante */
            if (op == TOKEN_SLASH) {
                if (left_type && right_type && (!is_numeric_type(left_type) || !is_numeric_type(right_type))) {
                    report_semantic_error(analyzer,
                                          expression_node->line_number,
                                          expression_node->column_number,
                                          "Division operator '/' requires numeric operands.");
                    return create_primitive_type(analyzer, PRIMITIVE_REAL);
                }

                if (left_type && right_type) {
                    if (left_type->details.primitive_type == PRIMITIVE_FLOAT ||
                        right_type->details.primitive_type == PRIMITIVE_FLOAT) {
                        return create_primitive_type(analyzer, PRIMITIVE_FLOAT);
                    }
                }
                return create_primitive_type(analyzer, PRIMITIVE_REAL);
            }

            /* Demais operadores aritméticos (-, *, %, ^, etc.) */
            if (left_type && right_type && (!is_numeric_type(left_type) || !is_numeric_type(right_type))) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Arithmetic operator requires numeric operands.");
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }

            if (left_type && right_type) {
                if (left_type->details.primitive_type == PRIMITIVE_FLOAT ||
                    right_type->details.primitive_type == PRIMITIVE_FLOAT) {
                    return create_primitive_type(analyzer, PRIMITIVE_FLOAT);
                }
                if (left_type->details.primitive_type == PRIMITIVE_REAL ||
                    right_type->details.primitive_type == PRIMITIVE_REAL) {
                    return create_primitive_type(analyzer, PRIMITIVE_REAL);
                }
            }

            return left_type ? left_type : create_primitive_type(analyzer, PRIMITIVE_INT);
        }

        case AST_NODE_EXPR_CALL: {
            IjklSymbol* resolved = resolve_routine_invocation(analyzer,
                                                             expression_node,
                                                             expression_node->data.call_expression.callable_expression,
                                                             expression_node->data.call_expression.argument_expressions);
            if (!resolved) {
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }
            if (resolved->symbol_kind != SYMBOL_KIND_FUNCTION) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Procedure '%s' cannot be invoked in an expression (no return value).",
                                      resolved->identifier_name);
                return create_primitive_type(analyzer, PRIMITIVE_INT);
            }
            return resolved->type_specification ? resolved->type_specification : create_primitive_type(analyzer, PRIMITIVE_INT);
        }

        case AST_NODE_EXPR_ALLOC: {
            IjklTypeSpec* size_type = ijkl_semantic_analyze_expression(
                analyzer, expression_node->data.allocation_expression.byte_count_expression);
            if (size_type && (size_type->kind != TYPE_KIND_PRIMITIVE ||
                              size_type->details.primitive_type != PRIMITIVE_INT)) {
                report_semantic_error(analyzer,
                                      expression_node->line_number,
                                      expression_node->column_number,
                                      "Allocation byte count must evaluate to integer type.");
            }
            return create_pointer_type(analyzer, NULL);
        }

        default:
            return create_primitive_type(analyzer, PRIMITIVE_INT);
    }
}

/*
 * EFEITO COLATERAL:
 * Analisa e infere o tipo estático resultante de uma expressão da AST,
 * decorando o nó da AST com seu tipo inferido para o gerador de código.
 */
IjklTypeSpec* ijkl_semantic_analyze_expression(IjklSemanticAnalyzer* analyzer,
                                              const IjklAstNode* expression_node) {
    if (!expression_node) return NULL;
    IjklTypeSpec* inferred_type = analyze_expression_internal(analyzer, expression_node);
    ((IjklAstNode*)expression_node)->inferred_type = inferred_type;
    return inferred_type;
}

/*
 * Validação semântica e estrutural de blocos de montagem embutida (inline assembly / CI).
 * Verifica terminação correta de instruções com ';' e rótulos iniciados por '#'.
 */
static void validate_asm_block(IjklSemanticAnalyzer* analyzer, const IjklAstNode* asm_node) {
    if (!analyzer || !asm_node) return;

    const char* target_builtin = asm_node->data.asm_statement.target_builtin_name;
    if (target_builtin) {
        if (!ijkl_symbol_table_is_known_builtin(analyzer->symbol_table, target_builtin)) {
            report_semantic_error(analyzer,
                                  asm_node->line_number,
                                  asm_node->column_number,
                                  "Identifier '%s' in asm block does not correspond to any known native builtin routine.",
                                  target_builtin);
        }
    }

    const char* ci_code = asm_node->data.asm_statement.ci_code_text;
    if (!ci_code) return;

    size_t current_line_number = asm_node->line_number;
    const char* line_cursor = ci_code;

    while (*line_cursor != '\0') {
        while (*line_cursor == ' ' || *line_cursor == '\t' || *line_cursor == '\r') {
            line_cursor++;
        }

        if (*line_cursor == '\n') {
            current_line_number++;
            line_cursor++;
            continue;
        }

        if (*line_cursor == '\0') {
            break;
        }

        const char* line_start = line_cursor;
        const char* line_end = line_cursor;
        while (*line_end != '\0' && *line_end != '\n') {
            line_end++;
        }

        const char* line_trimmed_end = line_end;
        while (line_trimmed_end > line_start &&
               (*(line_trimmed_end - 1) == ' ' || *(line_trimmed_end - 1) == '\t' || *(line_trimmed_end - 1) == '\r')) {
            line_trimmed_end--;
        }

        size_t line_length = (size_t)(line_trimmed_end - line_start);

        if (line_length > 0) {
            bool is_comment = (line_length >= 2 && line_start[0] == '/' && line_start[1] == '/');
            if (!is_comment) {
                if (*line_start != '#') {
                    if (*(line_trimmed_end - 1) != ';') {
                        report_semantic_error(analyzer,
                                              current_line_number,
                                              1,
                                              "Inline assembly statement must terminate with ';' (found '%.*s').",
                                              (int)(line_length > 32 ? 32 : line_length),
                                              line_start);
                    }
                }
            }
        }

        if (*line_end == '\n') {
            current_line_number++;
            line_cursor = line_end + 1;
        } else {
            line_cursor = line_end;
        }
    }
}

/*
 * EFEITO COLATERAL:
 * Percorre recursivamente instruções e comandos da AST, validando tipos e regras de fluxo.
 */
static void analyze_statement(IjklSemanticAnalyzer* analyzer, const IjklAstNode* statement_node) {
    if (!statement_node) return;

    switch (statement_node->kind) {
        case AST_NODE_STMT_BLOCK: {
            char block_scope_name[64];
            snprintf(block_scope_name, sizeof(block_scope_name), "Block_L%zu_C%zu", statement_node->line_number, statement_node->column_number);
            IjklScope* previous_scope = analyzer->symbol_table->current_active_scope;
            IjklScope* child_scope = previous_scope ? previous_scope->first_child_scope : NULL;
            while (child_scope) {
                if (strcmp(child_scope->scope_name, block_scope_name) == 0) {
                    analyzer->symbol_table->current_active_scope = child_scope;
                    break;
                }
                child_scope = child_scope->next_sibling_scope;
            }

            const IjklAstNode* child_statement = statement_node->data.block_statement.statements;
            while (child_statement) {
                analyze_statement(analyzer, child_statement);
                child_statement = child_statement->next_sibling;
            }

            analyzer->symbol_table->current_active_scope = previous_scope;
            break;
        }

        case AST_NODE_STMT_ASSIGN: {
            const IjklAstNode* target = statement_node->data.assignment_statement.target_lvalue;
            const IjklAstNode* value = statement_node->data.assignment_statement.value_expression;

            if (!is_valid_lvalue_expression(target)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "Assignment target must be a valid l-value.");
            }

            /* Impede atribuição a constantes declaradas */
            if (target && target->kind == AST_NODE_EXPR_IDENTIFIER) {
                const char* target_name = target->data.identifier_expression.identifier_name;
                IjklSymbol* target_symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, target_name);
                if (target_symbol && target_symbol->symbol_kind == SYMBOL_KIND_CONSTANT) {
                    report_semantic_error(analyzer,
                                          statement_node->line_number,
                                          statement_node->column_number,
                                          "Cannot assign a new value to constant '%s'.", target_name);
                }
            }

            IjklTypeSpec* target_type = ijkl_semantic_analyze_expression(analyzer, target);
            IjklTypeSpec* value_type = ijkl_semantic_analyze_expression(analyzer, value);

            if (target_type && value_type && !ijkl_semantic_are_types_compatible(target_type, value_type)) {
                IjklPrimitiveType boxed_prim;
                if (target_type->kind == TYPE_KIND_PRIMITIVE && ijkl_type_is_boxed_primitive(value_type, &boxed_prim)) {
                    report_semantic_error(analyzer,
                                          statement_node->line_number,
                                          statement_node->column_number,
                                          "Cannot implicitly unbox '%s' to primitive type '%s'. Explicitly access '.value' (or '.valor') to retrieve the primitive value (e.g., 'z = y.value;').",
                                          ijkl_ast_type_spec_to_string(value_type, analyzer->memory_arena),
                                          ijkl_ast_type_spec_to_string(target_type, analyzer->memory_arena));
                } else {
                    report_semantic_error(analyzer,
                                          statement_node->line_number,
                                          statement_node->column_number,
                                          "Incompatible types in assignment statement.");
                }
            }
            break;
        }

        case AST_NODE_STMT_IF: {
            const IjklAstNode* condition = statement_node->data.if_statement.condition_expression;
            /* Regra 8: Proibição estrita de atribuição em condições */
            if (condition && condition->kind == AST_NODE_STMT_ASSIGN) {
                report_semantic_error(analyzer,
                                      condition->line_number,
                                      condition->column_number,
                                      "Assignment is strictly prohibited within conditional expressions.");
            }

            IjklTypeSpec* condition_type = ijkl_semantic_analyze_expression(analyzer, condition);
            if (condition_type && !is_boolean_type(condition_type) && !is_numeric_type(condition_type)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "If condition must evaluate to a boolean or integer expression.");
            }

            analyze_statement(analyzer, statement_node->data.if_statement.then_branch_statement);
            if (statement_node->data.if_statement.else_branch_statement) {
                analyze_statement(analyzer, statement_node->data.if_statement.else_branch_statement);
            }
            break;
        }

        case AST_NODE_STMT_WHILE: {
            const IjklAstNode* condition = statement_node->data.while_statement.condition_expression;
            if (condition && condition->kind == AST_NODE_STMT_ASSIGN) {
                report_semantic_error(analyzer,
                                      condition->line_number,
                                      condition->column_number,
                                      "Assignment is strictly prohibited within conditional expressions.");
            }

            IjklTypeSpec* condition_type = ijkl_semantic_analyze_expression(analyzer, condition);
            if (condition_type && !is_boolean_type(condition_type) && !is_numeric_type(condition_type)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "While loop condition must evaluate to a boolean or integer expression.");
            }

            analyzer->loop_nesting_depth++;
            analyze_statement(analyzer, statement_node->data.while_statement.body_statement);
            analyzer->loop_nesting_depth--;
            break;
        }

        case AST_NODE_STMT_FOR: {
            char for_scope_name[64];
            snprintf(for_scope_name, sizeof(for_scope_name), "ForLoop_L%zu", statement_node->line_number);
            IjklScope* previous_scope = analyzer->symbol_table->current_active_scope;
            IjklScope* child_scope = previous_scope ? previous_scope->first_child_scope : NULL;
            while (child_scope) {
                if (strcmp(child_scope->scope_name, for_scope_name) == 0) {
                    analyzer->symbol_table->current_active_scope = child_scope;
                    break;
                }
                child_scope = child_scope->next_sibling_scope;
            }

            const char* control_var = statement_node->data.for_statement.control_variable_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, control_var);
            if (!symbol) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "Undeclared control variable '%s' in for loop.", control_var);
            }

            IjklTypeSpec* start_type = ijkl_semantic_analyze_expression(
                analyzer, statement_node->data.for_statement.start_expression);
            IjklTypeSpec* end_type = ijkl_semantic_analyze_expression(
                analyzer, statement_node->data.for_statement.end_expression);

            if (start_type && (start_type->kind != TYPE_KIND_PRIMITIVE ||
                               start_type->details.primitive_type != PRIMITIVE_INT)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "For loop initial value must be an integer.");
            }

            if (end_type && (end_type->kind != TYPE_KIND_PRIMITIVE ||
                             end_type->details.primitive_type != PRIMITIVE_INT)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "For loop limit value must be an integer.");
            }

            if (statement_node->data.for_statement.step_expression) {
                IjklTypeSpec* step_type = ijkl_semantic_analyze_expression(
                    analyzer, statement_node->data.for_statement.step_expression);
                if (step_type && (step_type->kind != TYPE_KIND_PRIMITIVE ||
                                  step_type->details.primitive_type != PRIMITIVE_INT)) {
                    report_semantic_error(analyzer,
                                          statement_node->line_number,
                                          statement_node->column_number,
                                          "For loop step value must be an integer.");
                }
            }

            analyzer->loop_nesting_depth++;
            analyze_statement(analyzer, statement_node->data.for_statement.body_statement);
            analyzer->loop_nesting_depth--;

            analyzer->symbol_table->current_active_scope = previous_scope;
            break;
        }

        case AST_NODE_STMT_REPEAT: {
            analyzer->loop_nesting_depth++;
            analyze_statement(analyzer, statement_node->data.repeat_statement.body_block);
            analyzer->loop_nesting_depth--;

            const IjklAstNode* condition = statement_node->data.repeat_statement.condition_expression;
            if (condition && condition->kind == AST_NODE_STMT_ASSIGN) {
                report_semantic_error(analyzer,
                                      condition->line_number,
                                      condition->column_number,
                                      "Assignment is strictly prohibited within conditional expressions.");
            }
            IjklTypeSpec* condition_type = ijkl_semantic_analyze_expression(analyzer, condition);
            if (condition_type && !is_boolean_type(condition_type) && !is_numeric_type(condition_type)) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "Repeat-until condition must evaluate to a boolean expression.");
            }
            break;
        }

        case AST_NODE_STMT_SWITCH: {
            IjklTypeSpec* cond_type = ijkl_semantic_analyze_expression(
                analyzer, statement_node->data.switch_statement.condition_expression);

            analyzer->switch_nesting_depth++;
            const IjklAstNode* branch = statement_node->data.switch_statement.case_branches;
            while (branch) {
                if (branch->kind == AST_NODE_CASE_BRANCH) {
                    IjklTypeSpec* match_type = ijkl_semantic_analyze_expression(
                        analyzer, branch->data.case_branch.match_expression);
                    if (cond_type && match_type && !ijkl_semantic_are_types_compatible(cond_type, match_type)) {
                        report_semantic_error(analyzer,
                                              branch->line_number,
                                              branch->column_number,
                                              "Case value type is incompatible with switch expression type.");
                    }
                    const IjklAstNode* case_stmt = branch->data.case_branch.statements;
                    while (case_stmt) {
                        analyze_statement(analyzer, case_stmt);
                        case_stmt = case_stmt->next_sibling;
                    }
                }
                branch = branch->next_sibling;
            }

            if (statement_node->data.switch_statement.default_branch) {
                const IjklAstNode* def_stmt = statement_node->data.switch_statement.default_branch;
                while (def_stmt) {
                    analyze_statement(analyzer, def_stmt);
                    def_stmt = def_stmt->next_sibling;
                }
            }
            analyzer->switch_nesting_depth--;
            break;
        }

        case AST_NODE_STMT_BREAK: {
            if (analyzer->loop_nesting_depth == 0 && analyzer->switch_nesting_depth == 0) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "Break statement ('aborte') must be located inside a loop or switch.");
            }
            break;
        }

        case AST_NODE_STMT_RETURN: {
            if (analyzer->current_routine_node) {
                bool is_function = analyzer->current_routine_node->data.routine_declaration.is_function;
                IjklTypeSpec* expected_return_type = analyzer->current_routine_node->data.routine_declaration.return_type_spec;
                const IjklAstNode* returned_val = statement_node->data.return_statement.return_value_expression;

                if (is_function) {
                    if (!returned_val) {
                        report_semantic_error(analyzer,
                                              statement_node->line_number,
                                              statement_node->column_number,
                                              "Function must return an expression of declared return type.");
                    } else {
                        IjklTypeSpec* val_type = ijkl_semantic_analyze_expression(analyzer, returned_val);
                        if (expected_return_type && val_type &&
                            !ijkl_semantic_are_types_compatible(expected_return_type, val_type)) {
                            report_semantic_error(analyzer,
                                                  statement_node->line_number,
                                                  statement_node->column_number,
                                                  "Type of return expression does not match function signature.");
                        }
                    }
                } else {
                    if (returned_val) {
                        report_semantic_error(analyzer,
                                              statement_node->line_number,
                                              statement_node->column_number,
                                              "Procedure cannot return a value expression.");
                    }
                }
            }
            break;
        }

        case AST_NODE_STMT_WRITE: {
            const IjklAstNode* arg = statement_node->data.write_statement.argument_expressions;
            while (arg) {
                ijkl_semantic_analyze_expression(analyzer, arg);
                arg = arg->next_sibling;
            }
            break;
        }

        case AST_NODE_STMT_READ: {
            const IjklAstNode* target = statement_node->data.read_statement.target_lvalues;
            while (target) {
                if (!is_valid_lvalue_expression(target)) {
                    report_semantic_error(analyzer,
                                          target->line_number,
                                          target->column_number,
                                          "Read destination must be a valid l-value variable.");
                }
                ijkl_semantic_analyze_expression(analyzer, target);
                target = target->next_sibling;
            }
            break;
        }

        case AST_NODE_STMT_FREE: {
            const IjklAstNode* target = statement_node->data.free_statement.target_lvalue;
            IjklTypeSpec* target_type = ijkl_semantic_analyze_expression(analyzer, target);
            if (target_type && target_type->kind == TYPE_KIND_USER_DEFINED &&
                target_type->details.user_type_name) {
                IjklSymbol* type_symbol = ijkl_symbol_table_lookup_lexical(
                    analyzer->symbol_table, target_type->details.user_type_name);
                if (type_symbol && type_symbol->type_specification) {
                    target_type = type_symbol->type_specification;
                }
            }
            if (target_type && target_type->kind != TYPE_KIND_POINTER) {
                report_semantic_error(analyzer,
                                      statement_node->line_number,
                                      statement_node->column_number,
                                      "Free statement ('libere') requires a pointer operand.");
            }
            break;
        }

        case AST_NODE_STMT_CALL: {
            resolve_routine_invocation(analyzer,
                                       statement_node,
                                       statement_node->data.call_statement.callable_expression,
                                       statement_node->data.call_statement.argument_expressions);
            break;
        }

        case AST_NODE_DECL_VAR: {
            if (statement_node->data.variable_declaration.initial_value_expression) {
                IjklTypeSpec* var_type = statement_node->data.variable_declaration.type_specification;
                IjklTypeSpec* init_type = ijkl_semantic_analyze_expression(
                    analyzer, statement_node->data.variable_declaration.initial_value_expression);
                if (var_type && init_type && !ijkl_semantic_are_types_compatible(var_type, init_type)) {
                    IjklPrimitiveType boxed_prim;
                    if (var_type->kind == TYPE_KIND_PRIMITIVE && ijkl_type_is_boxed_primitive(init_type, &boxed_prim)) {
                        report_semantic_error(analyzer,
                                              statement_node->line_number,
                                              statement_node->column_number,
                                              "Cannot implicitly unbox '%s' to primitive type '%s' in variable initialization. Explicitly access '.value' (or '.valor') to retrieve the primitive value (e.g., 'z = y.value;').",
                                              ijkl_ast_type_spec_to_string(init_type, analyzer->memory_arena),
                                              ijkl_ast_type_spec_to_string(var_type, analyzer->memory_arena));
                    } else {
                        report_semantic_error(analyzer,
                                              statement_node->line_number,
                                              statement_node->column_number,
                                              "Incompatible initializer type for variable '%s'.",
                                              statement_node->data.variable_declaration.variable_name);
                    }
                }
            }
            break;
        }

        case AST_NODE_STMT_ASM: {
            validate_asm_block(analyzer, statement_node);
            break;
        }

        default:
            break;
    }
}

/*
 * Verifica se um comando ou bloco encerra obrigatoriamente com retorno em todos os caminhos.
 */
static bool statement_always_returns(const IjklAstNode* statement_node) {
    if (!statement_node) return false;

    switch (statement_node->kind) {
        case AST_NODE_STMT_RETURN:
            return true;

        case AST_NODE_STMT_ASM: {
            const char* code_text = statement_node->data.asm_statement.ci_code_text;
            if (code_text && (strstr(code_text, "return") != NULL || strstr(code_text, "ret") != NULL)) {
                return true;
            }
            return false;
        }

        case AST_NODE_STMT_BLOCK: {
            const IjklAstNode* child_statement = statement_node->data.block_statement.statements;
            while (child_statement) {
                if (statement_always_returns(child_statement)) {
                    return true;
                }
                child_statement = child_statement->next_sibling;
            }
            return false;
        }

        case AST_NODE_STMT_IF: {
            if (!statement_node->data.if_statement.else_branch_statement) {
                return false;
            }
            return statement_always_returns(statement_node->data.if_statement.then_branch_statement) &&
                   statement_always_returns(statement_node->data.if_statement.else_branch_statement);
        }

        default:
            return false;
    }
}

/*
 * EFEITO COLATERAL:
 * Analisa uma rotina (função ou procedimento) ajustando o escopo ativo e o contexto.
 */
static void analyze_routine_declaration(IjklSemanticAnalyzer* analyzer,
                                       const IjklAstNode* routine_node) {
    if (!routine_node) return;

    const char* routine_name = routine_node->data.routine_declaration.routine_name;
    IjklSymbol* routine_symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, routine_name);
    while (routine_symbol) {
        if (routine_symbol->declaration_node == routine_node) {
            break;
        }
        routine_symbol = routine_symbol->next_overload;
    }

    IjklScope* previous_scope = analyzer->symbol_table->current_active_scope;
    if (routine_symbol && routine_symbol->internal_scope) {
        analyzer->symbol_table->current_active_scope = routine_symbol->internal_scope;
    }

    const IjklAstNode* previous_routine = analyzer->current_routine_node;
    analyzer->current_routine_node = routine_node;

    const IjklAstNode* body_statement = routine_node->data.routine_declaration.routine_body;
    while (body_statement) {
        analyze_statement(analyzer, body_statement);
        body_statement = body_statement->next_sibling;
    }

    /* Validação estrita de retorno: funções devem retornar valor em todos os caminhos (Requisito 11) */
    if (routine_node->data.routine_declaration.is_function) {
        bool has_guaranteed_return = false;
        const IjklAstNode* scan_statement = routine_node->data.routine_declaration.routine_body;
        while (scan_statement) {
            if (statement_always_returns(scan_statement)) {
                has_guaranteed_return = true;
                break;
            }
            scan_statement = scan_statement->next_sibling;
        }

        if (!has_guaranteed_return) {
            report_semantic_error(analyzer,
                                  routine_node->line_number,
                                  routine_node->column_number,
                                  "Function '%s' must return a value on all control paths (explicit 'retorne' / 'return' required).",
                                  routine_name);
        }
    }

    analyzer->current_routine_node = previous_routine;
    analyzer->symbol_table->current_active_scope = previous_scope;
}

static bool is_constant_expression(const IjklSemanticAnalyzer* analyzer, const IjklAstNode* expression_node) {
    if (!expression_node) return false;
    switch (expression_node->kind) {
        case AST_NODE_EXPR_LITERAL_INT:
        case AST_NODE_EXPR_LITERAL_REAL:
        case AST_NODE_EXPR_LITERAL_FLOAT:
        case AST_NODE_EXPR_LITERAL_STRING:
        case AST_NODE_EXPR_LITERAL_CHAR:
        case AST_NODE_EXPR_LITERAL_BOOL:
        case AST_NODE_EXPR_LITERAL_NULL:
            return true;
        case AST_NODE_EXPR_UNARY:
            return is_constant_expression(analyzer, expression_node->data.unary_expression.operand);
        case AST_NODE_EXPR_BINARY:
            return is_constant_expression(analyzer, expression_node->data.binary_expression.left_operand) &&
                   is_constant_expression(analyzer, expression_node->data.binary_expression.right_operand);
        case AST_NODE_EXPR_IDENTIFIER: {
            const char* name = expression_node->data.identifier_expression.identifier_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, name);
            return (symbol && symbol->symbol_kind == SYMBOL_KIND_CONSTANT);
        }
        default:
            return false;
    }
}

static bool is_initialize_routine(const IjklSemanticAnalyzer* analyzer, const char* routine_name) {
    if (!routine_name) return false;
    const char* configured_name = ijkl_vocabulary_get_initialize_name(analyzer->vocabulary);
    return (configured_name && strcmp(routine_name, configured_name) == 0) ||
           strcmp(routine_name, "inicializar") == 0 ||
           strcmp(routine_name, "initialize") == 0 ||
           strcmp(routine_name, "initialiser") == 0;
}

static bool is_finalize_routine(const IjklSemanticAnalyzer* analyzer, const char* routine_name) {
    if (!routine_name) return false;
    const char* configured_name = ijkl_vocabulary_get_finalize_name(analyzer->vocabulary);
    return (configured_name && strcmp(routine_name, configured_name) == 0) ||
           strcmp(routine_name, "finalizar") == 0 ||
           strcmp(routine_name, "finalize") == 0 ||
           strcmp(routine_name, "finaliser") == 0;
}

/*
 * EFEITO COLATERAL:
 * Varre recursivamente todas as declarações e o ponto de entrada da compilação.
 */
bool ijkl_semantic_analyzer_analyze(IjklSemanticAnalyzer* analyzer,
                                    const IjklAstNode* compilation_unit_root) {
    if (!analyzer || !compilation_unit_root) return false;

    const char* configured_entry_name = analyzer->vocabulary ?
        ijkl_vocabulary_get_entry_point_name(analyzer->vocabulary) : "principal";
    size_t total_entry_points_found = 0;

    const IjklAstNode* current_unit = compilation_unit_root;
    while (current_unit) {
        ijkl_symbol_table_set_active_compilation_unit(analyzer->symbol_table, current_unit);

        if (current_unit->kind == AST_NODE_PROGRAM) {
            const char* program_name = current_unit->data.program.program_name;
            IjklSymbol* program_symbol = ijkl_symbol_table_lookup_lexical(analyzer->symbol_table, program_name);
            IjklScope* previous_scope = analyzer->symbol_table->current_active_scope;
            if (program_symbol && program_symbol->internal_scope) {
                analyzer->symbol_table->current_active_scope = program_symbol->internal_scope;
            }

            /* Validação de importações: unidades 'program' não podem ser importadas */
            const IjklAstNode* program_import_node = current_unit->data.program.imports;
            while (program_import_node) {
                if (program_import_node->kind == AST_NODE_IMPORT) {
                    const char* imported_path = program_import_node->data.import_declaration.module_path;
                    IjklSymbol* imported_symbol = ijkl_symbol_table_lookup_qualified(analyzer->symbol_table, imported_path);
                    if (imported_symbol && imported_symbol->declaration_node && imported_symbol->declaration_node->kind == AST_NODE_PROGRAM) {
                        report_semantic_error(analyzer,
                                              program_import_node->line_number,
                                              program_import_node->column_number,
                                              "Cannot import program unit '%s'. Only 'unit' modules can be imported.",
                                              imported_path);
                    }
                }
                program_import_node = program_import_node->next_sibling;
            }

            size_t initialize_count = 0;
            size_t finalize_count = 0;

            /* Analisa declarações */
            const IjklAstNode* declaration = current_unit->data.program.declarations;
            while (declaration) {
                if (declaration->kind == AST_NODE_DECL_ROUTINE) {
                    const char* routine_name = declaration->data.routine_declaration.routine_name;
                    bool is_entry_point = ((configured_entry_name && strcmp(routine_name, configured_entry_name) == 0) ||
                                           strcmp(routine_name, "main") == 0 ||
                                           strcmp(routine_name, "principal") == 0);
                    if (is_entry_point) {
                        /* Validação de visibilidade do ponto de entrada: deve ser obrigatoriamente privado */
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Entry function '%s' must be declared as 'privado' ('private'). It cannot be declared as 'publico' ('public').",
                                                  routine_name);
                        } else {
                            total_entry_points_found++;
                        }

                        /* Validação de assinatura: Requisito 11 */
                        /* Deve retornar inteiro */
                        if (!declaration->data.routine_declaration.is_function ||
                            !declaration->data.routine_declaration.return_type_spec ||
                            declaration->data.routine_declaration.return_type_spec->kind != TYPE_KIND_PRIMITIVE ||
                            declaration->data.routine_declaration.return_type_spec->details.primitive_type != PRIMITIVE_INT) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Entry function '%s' must return an integer ('inteiro' / 'int').",
                                                  routine_name);
                        }

                        /* Parâmetros: 0 parâmetros OU (inteiro quantidade, arranjo[] de texto args) */
                        size_t parameter_count = 0;
                        const IjklAstNode* param = declaration->data.routine_declaration.parameters;
                        while (param) {
                            parameter_count++;
                            param = param->next_sibling;
                        }

                        if (parameter_count == 0) {
                            /* Assinatura válida: principal(): inteiro */
                        } else if (parameter_count == 2) {
                            const IjklAstNode* p1 = declaration->data.routine_declaration.parameters;
                            const IjklAstNode* p2 = p1->next_sibling;

                            if (!p1->data.parameter.type_specification ||
                                p1->data.parameter.type_specification->kind != TYPE_KIND_PRIMITIVE ||
                                p1->data.parameter.type_specification->details.primitive_type != PRIMITIVE_INT) {
                                report_semantic_error(analyzer,
                                                      p1->line_number,
                                                      p1->column_number,
                                                      "First parameter of entry function '%s' must be an integer ('inteiro' / 'int').",
                                                      routine_name);
                            }

                            if (!p2->data.parameter.type_specification ||
                                p2->data.parameter.type_specification->kind != TYPE_KIND_ARRAY ||
                                !p2->data.parameter.type_specification->details.array_info.element_type ||
                                p2->data.parameter.type_specification->details.array_info.element_type->kind != TYPE_KIND_PRIMITIVE ||
                                p2->data.parameter.type_specification->details.array_info.element_type->details.primitive_type != PRIMITIVE_STRING) {
                                report_semantic_error(analyzer,
                                                      p2->line_number,
                                                      p2->column_number,
                                                      "Second parameter of entry function '%s' must be an array of text ('arranjo[] de texto' / 'string[]').",
                                                      routine_name);
                            }
                        } else {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Entry function '%s' must have 0 parameters or exactly 2 parameters: '(inteiro quantidade, arranjo[] de texto args)'.",
                                                  routine_name);
                        }
                    } else if (is_initialize_routine(analyzer, routine_name)) {
                        initialize_count++;
                        if (initialize_count > 1) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Multiple initialize procedures defined in program '%s'. At most one initialize procedure is allowed.",
                                                  program_name);
                        }
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be declared as 'privado' ('private'). It cannot be declared as 'publico' ('public').",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.is_function) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be a procedure ('procedimento'), not a function with return value.",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.parameters != NULL) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' cannot take any parameters. It must have signature '%s()'.",
                                                  routine_name, routine_name);
                        }
                    } else if (is_finalize_routine(analyzer, routine_name)) {
                        finalize_count++;
                        if (finalize_count > 1) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Multiple finalize procedures defined in program '%s'. At most one finalize procedure is allowed.",
                                                  program_name);
                        }
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be declared as 'privado' ('private'). It cannot be declared as 'publico' ('public').",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.is_function) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be a procedure ('procedimento'), not a function with return value.",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.parameters != NULL) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' cannot take any parameters. It must have signature '%s()'.",
                                                  routine_name, routine_name);
                        }
                    } else {
                        /* Em unidades do tipo 'program', nenhuma outra rotina pode ser 'publico' */
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Routine '%s' in a 'program' unit cannot be declared as 'publico' ('public'). Units of type 'program' cannot be imported and cannot export public routines; all routines in a program must be 'privado' ('private').",
                                                  routine_name);
                        }
                    }

                    analyze_routine_declaration(analyzer, declaration);
                } else if (declaration->kind == AST_NODE_DECL_VAR) {
                    if (declaration->data.variable_declaration.initial_value_expression &&
                        !is_constant_expression(analyzer, declaration->data.variable_declaration.initial_value_expression)) {
                        report_semantic_error(analyzer,
                                              declaration->line_number,
                                              declaration->column_number,
                                              "Initializer for namespace variable '%s' must be a constant expression (e.g. literal value or constant). Non-trivial expressions must be initialized inside 'inicializar' ('initialize').",
                                              declaration->data.variable_declaration.variable_name);
                    }
                    analyze_statement(analyzer, declaration);
                } else {
                    analyze_statement(analyzer, declaration);
                }
                declaration = declaration->next_sibling;
            }

            analyzer->symbol_table->current_active_scope = previous_scope;
        } else if (current_unit->kind == AST_NODE_MODULE) {
            const char* module_path = current_unit->data.module_declaration.qualified_module_path;
            IjklSymbol* module_symbol = ijkl_symbol_table_lookup_qualified(analyzer->symbol_table, module_path);
            IjklScope* previous_scope = analyzer->symbol_table->current_active_scope;
            if (module_symbol && module_symbol->internal_scope) {
                analyzer->symbol_table->current_active_scope = module_symbol->internal_scope;
            }

            /* Validação de importações: unidades 'program' não podem ser importadas */
            const IjklAstNode* module_import_node = current_unit->data.module_declaration.imports;
            while (module_import_node) {
                if (module_import_node->kind == AST_NODE_IMPORT) {
                    const char* imported_path = module_import_node->data.import_declaration.module_path;
                    IjklSymbol* imported_symbol = ijkl_symbol_table_lookup_qualified(analyzer->symbol_table, imported_path);
                    if (imported_symbol && imported_symbol->declaration_node && imported_symbol->declaration_node->kind == AST_NODE_PROGRAM) {
                        report_semantic_error(analyzer,
                                              module_import_node->line_number,
                                              module_import_node->column_number,
                                              "Cannot import program unit '%s'. Only 'unit' modules can be imported.",
                                              imported_path);
                    }
                }
                module_import_node = module_import_node->next_sibling;
            }

            size_t initialize_count = 0;
            size_t finalize_count = 0;

            const IjklAstNode* declaration = current_unit->data.module_declaration.declarations;
            while (declaration) {
                if (declaration->kind == AST_NODE_DECL_ROUTINE) {
                    const char* routine_name = declaration->data.routine_declaration.routine_name;
                    bool is_entry_point = ((configured_entry_name && strcmp(routine_name, configured_entry_name) == 0) ||
                                           strcmp(routine_name, "main") == 0 ||
                                           strcmp(routine_name, "principal") == 0);
                    if (is_entry_point) {
                        report_semantic_error(analyzer,
                                              declaration->line_number,
                                              declaration->column_number,
                                              "Entry function '%s' cannot be declared inside a module unit ('%s'). It must be declared in a 'program' unit.",
                                              routine_name, module_path);
                    } else if (is_initialize_routine(analyzer, routine_name)) {
                        initialize_count++;
                        if (initialize_count > 1) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Multiple initialize procedures defined in module '%s'. At most one initialize procedure is allowed.",
                                                  module_path);
                        }
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be declared as 'privado' ('private'). It cannot be declared as 'publico' ('public').",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.is_function) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be a procedure ('procedimento'), not a function with return value.",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.parameters != NULL) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' cannot take any parameters. It must have signature '%s()'.",
                                                  routine_name, routine_name);
                        }
                    } else if (is_finalize_routine(analyzer, routine_name)) {
                        finalize_count++;
                        if (finalize_count > 1) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Multiple finalize procedures defined in module '%s'. At most one finalize procedure is allowed.",
                                                  module_path);
                        }
                        if (declaration->data.routine_declaration.is_public) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be declared as 'privado' ('private'). It cannot be declared as 'publico' ('public').",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.is_function) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' must be a procedure ('procedimento'), not a function with return value.",
                                                  routine_name);
                        }
                        if (declaration->data.routine_declaration.parameters != NULL) {
                            report_semantic_error(analyzer,
                                                  declaration->line_number,
                                                  declaration->column_number,
                                                  "Lifecycle routine '%s' cannot take any parameters. It must have signature '%s()'.",
                                                  routine_name, routine_name);
                        }
                    }
                    analyze_routine_declaration(analyzer, declaration);
                } else if (declaration->kind == AST_NODE_DECL_VAR) {
                    if (declaration->data.variable_declaration.initial_value_expression &&
                        !is_constant_expression(analyzer, declaration->data.variable_declaration.initial_value_expression)) {
                        report_semantic_error(analyzer,
                                              declaration->line_number,
                                              declaration->column_number,
                                              "Initializer for namespace variable '%s' must be a constant expression (e.g. literal value or constant). Non-trivial expressions must be initialized inside 'inicializar' ('initialize').",
                                              declaration->data.variable_declaration.variable_name);
                    }
                    analyze_statement(analyzer, declaration);
                } else {
                    analyze_statement(analyzer, declaration);
                }
                declaration = declaration->next_sibling;
            }

            analyzer->symbol_table->current_active_scope = previous_scope;
        }

        current_unit = current_unit->next_sibling;
    }

    bool has_program_unit = false;
    const IjklAstNode* scan = compilation_unit_root;
    while (scan) {
        if (scan->kind == AST_NODE_PROGRAM) {
            has_program_unit = true;
            break;
        }
        scan = scan->next_sibling;
    }

    /* Validação de Ponto de Entrada Único: Requisito 12 */
    if (has_program_unit && total_entry_points_found == 0) {
        report_semantic_error(analyzer, 1, 1,
                              "No entry point function found in compilation set. Exactly one entry point function ('%s' or 'main') is required.",
                              configured_entry_name);
    } else if (total_entry_points_found > 1) {
        report_semantic_error(analyzer, 1, 1,
                              "Multiple entry point functions detected (%zu found). Exactly one entry point is required.",
                              total_entry_points_found);
    }

    ijkl_symbol_table_set_active_compilation_unit(analyzer->symbol_table, NULL);
    return analyzer->total_errors_count == 0;
}

size_t ijkl_semantic_analyzer_get_error_count(const IjklSemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->total_errors_count : 0;
}

size_t ijkl_semantic_analyzer_get_warning_count(const IjklSemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->total_warnings_count : 0;
}
