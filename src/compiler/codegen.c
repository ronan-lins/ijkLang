#include "codegen.h"
#include <string.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * ==============================================================================
 * MÓDULO: GERADOR DE CÓDIGO INTERMEDIÁRIO (.CI) DA IJKLANG
 * ==============================================================================
 * Implementa a transformação direta da Árvore Sintática Abstrata (AST) em
 * instruções lineares textuais em conformidade com Codigo_Intermediario_CI.md.
 *
 * Princípios aplicados:
 * - Registradores r0 a r4 com convenção determinística.
 * - Prólogo com 'alloc_frame <tamanho_em_bytes>' e epílogo com 'free_frame'.
 * - Endereçamento relativo a ra para variáveis locais e parâmetros.
 * - Desvios e rótulos estruturados para laços e condicionais.
 * - Isolamento de E/S por instruções 'in' e 'out' nas portas da VM.
 * ==============================================================================
 */

static void generate_statement(IjklCodeGenerator* generator, const IjklAstNode* statement_node);
static void generate_expression(IjklCodeGenerator* generator,
                                const IjklAstNode* expression_node,
                                const char* target_register);
static void generate_lvalue_address(IjklCodeGenerator* generator,
                                    const IjklAstNode* lvalue_node,
                                    const char* target_register);


/* Verifica se um nó da AST representa uma expressão do tipo texto */
static bool is_string_node(IjklCodeGenerator* generator, const IjklAstNode* node) {
    if (!node) return false;
    if (node->inferred_type) {
        if (node->inferred_type->kind == TYPE_KIND_PRIMITIVE &&
            node->inferred_type->details.primitive_type == PRIMITIVE_STRING) {
            return true;
        }
    }
    if (node->kind == AST_NODE_EXPR_LITERAL_STRING) return true;
    if (node->kind == AST_NODE_EXPR_IDENTIFIER && generator && generator->symbol_table) {
        const char* name = node->data.identifier_expression.identifier_name;
        IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, name);
        if (symbol && symbol->type_specification &&
            symbol->type_specification->kind == TYPE_KIND_PRIMITIVE &&
            symbol->type_specification->details.primitive_type == PRIMITIVE_STRING) {
            return true;
        }
    }
    return false;
}

/* Identifica o tipo primitivo de uma expressão da AST */
static IjklPrimitiveType get_node_primitive_type(IjklCodeGenerator* generator, const IjklAstNode* node) {
    if (!node) return PRIMITIVE_INT;
    if (node->inferred_type && node->inferred_type->kind == TYPE_KIND_PRIMITIVE) {
        return node->inferred_type->details.primitive_type;
    }
    if (node->kind == AST_NODE_EXPR_LITERAL_STRING) return PRIMITIVE_STRING;
    if (node->kind == AST_NODE_EXPR_LITERAL_INT) return PRIMITIVE_INT;
    if (node->kind == AST_NODE_EXPR_LITERAL_REAL) return PRIMITIVE_REAL;
    if (node->kind == AST_NODE_EXPR_LITERAL_FLOAT) return PRIMITIVE_FLOAT;
    if (node->kind == AST_NODE_EXPR_LITERAL_BOOL) return PRIMITIVE_BOOL;
    if (node->kind == AST_NODE_EXPR_LITERAL_CHAR) return PRIMITIVE_CHAR;
    if (node->kind == AST_NODE_EXPR_IDENTIFIER && generator && generator->symbol_table) {
        const char* name = node->data.identifier_expression.identifier_name;
        IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, name);
        if (symbol && symbol->type_specification && symbol->type_specification->kind == TYPE_KIND_PRIMITIVE) {
            return symbol->type_specification->details.primitive_type;
        }
    }
    return PRIMITIVE_INT;
}

/* Determina se a atribuição ou passagem de argumento requer auto-boxing */
static bool should_autobox_expression(const IjklTypeSpec* target_type, const IjklTypeSpec* source_type) {
    if (!target_type || !source_type) return false;

    IjklPrimitiveType target_boxed_prim = PRIMITIVE_INT;
    bool target_is_boxed = ijkl_type_is_boxed_primitive(target_type, &target_boxed_prim);
    bool target_is_generic_ptr = (target_type->kind == TYPE_KIND_POINTER && !target_type->details.pointer_info.base_type);

    if (target_is_boxed) {
        if (source_type->kind == TYPE_KIND_PRIMITIVE) {
            return true;
        }
    } else if (target_is_generic_ptr) {
        if (source_type->kind == TYPE_KIND_PRIMITIVE && source_type->details.primitive_type != PRIMITIVE_STRING) {
            return true;
        }
    }
    return false;
}

/* Emite as instruções de alocação e encapsulamento de valor primitivo na heap */
static void emit_autobox_code(IjklCodeGenerator* generator, const char* value_register) {
    fprintf(generator->output_stream, "    r1 := alloc(8);\n");
    fprintf(generator->output_stream, "    [r1, 0] := %s;\n", value_register);
    fprintf(generator->output_stream, "    %s := r1;\n", value_register);
    generator->total_emitted_instructions_count += 3;
}

/* Emite uma cadeia de caracteres escapando adequadamente aspas e caracteres de controle */
static void emit_escaped_string_literal(FILE* output_stream, const char* string_value) {
    if (!string_value) return;
    for (const char* ptr = string_value; *ptr != '\0'; ptr++) {
        switch (*ptr) {
            case '"':  fputs("\\\"", output_stream); break;
            case '\\': fputs("\\\\", output_stream); break;
            case '\n': fputs("\\n", output_stream); break;
            case '\r': fputs("\\r", output_stream); break;
            case '\t': fputs("\\t", output_stream); break;
            default:   fputc(*ptr, output_stream); break;
        }
    }
}

/* Criação do gerador de código intermediário */
IjklCodeGenerator* ijkl_code_generator_create(IjklMemoryArena* memory_arena,
                                             IjklSymbolTable* symbol_table,
                                             FILE* output_stream) {
    if (!memory_arena || !symbol_table || !output_stream) return NULL;

    IjklCodeGenerator* generator = (IjklCodeGenerator*)ijkl_memory_arena_allocate(
        memory_arena, sizeof(IjklCodeGenerator));

    generator->memory_arena = memory_arena;
    generator->symbol_table = symbol_table;
    generator->output_stream = output_stream;
    generator->label_counter = 1;
    generator->temporary_register_index = 0;
    generator->total_emitted_instructions_count = 0;
    generator->current_spill_offset = 0;
    generator->current_spill_depth = 0;
    generator->use_native_builtins = false;
    generator->current_routine_name = NULL;
    generator->current_module_path = NULL;
    generator->current_scope = symbol_table->root_global_scope;
    generator->break_target_label_depth = 0;

    return generator;
}

/* Configura se as rotinas built-in serão omitidas do .ci para resolução nativa pela VM */
void ijkl_code_generator_set_native_builtins(IjklCodeGenerator* code_generator,
                                             bool use_native_builtins) {
    if (code_generator) {
        code_generator->use_native_builtins = use_native_builtins;
    }
}

/* Gera um novo identificador único de rótulo para desvios e saltos */
static const char* generate_unique_label(IjklCodeGenerator* generator, const char* prefix) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s_%zu", prefix, generator->label_counter++);
    size_t length = strlen(buffer);
    char* label_copy = (char*)ijkl_memory_arena_allocate(generator->memory_arena, length + 1);
    memcpy(label_copy, buffer, length + 1);
    return label_copy;
}

/* Retorna o mnemônico textual de um operador binário para o código intermediário */
static const char* binary_operator_to_string(IjklTokenType operator_type) {
    switch (operator_type) {
        case TOKEN_PLUS:               return "+";
        case TOKEN_MINUS:              return "-";
        case TOKEN_STAR:               return "*";
        case TOKEN_SLASH:              return "/";
        case TOKEN_KW_DIV:             return "div";
        case TOKEN_KW_MOD:             return "mod";
        case TOKEN_CARET:              return "^";
        case TOKEN_EQUAL:              return "=";
        case TOKEN_EQUAL_STRICT:       return "=";
        case TOKEN_NOT_EQUAL:
        case TOKEN_NOT_EQUAL_PASCAL:   return "<>";
        case TOKEN_LESS:               return "<";
        case TOKEN_GREATER:            return ">";
        case TOKEN_LESS_EQUAL:         return "<=";
        case TOKEN_GREATER_EQUAL:      return ">=";
        case TOKEN_KW_AND:
        case TOKEN_LOGICAL_AND:        return "and";
        case TOKEN_KW_OR:
        case TOKEN_LOGICAL_OR:         return "or";
        default:                       return "+";
    }
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

/*
 * EFEITO COLATERAL:
 * Calcula o endereço efetivo de um l-value e carrega o ponteiro no target_register.
 */
static void generate_lvalue_address(IjklCodeGenerator* generator,
                                   const IjklAstNode* lvalue_node,
                                   const char* target_register) {
    if (!lvalue_node || !target_register) return;

    switch (lvalue_node->kind) {
        case AST_NODE_EXPR_IDENTIFIER: {
            const char* var_name = lvalue_node->data.identifier_expression.identifier_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, var_name);
            size_t offset = symbol ? symbol->memory_offset_in_bytes : 0;

            if (symbol && symbol->enclosing_scope &&
                (symbol->enclosing_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                 symbol->enclosing_scope->scope_kind == SCOPE_KIND_MODULE)) {
                fprintf(generator->output_stream, "    %s := 0x1000 + %zu;\n", target_register, offset);
            } else if (symbol && symbol->is_by_reference) {
                fprintf(generator->output_stream, "    %s := [ra, %zu];\n", target_register, offset);
            } else {
                fprintf(generator->output_stream, "    %s := ra + %zu;\n", target_register, offset);
            }
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_DEREFERENCE: {
            generate_expression(generator, lvalue_node->data.dereference_expression.operand, target_register);
            break;
        }

        case AST_NODE_EXPR_INDEX_ACCESS: {
            const IjklAstNode* base = lvalue_node->data.index_access_expression.target_expression;
            const IjklAstNode* index = lvalue_node->data.index_access_expression.index_expression;

            if (base->kind == AST_NODE_EXPR_IDENTIFIER) {
                const char* base_name = base->data.identifier_expression.identifier_name;
                IjklSymbol* base_sym = ijkl_symbol_table_lookup_lexical(generator->symbol_table, base_name);
                if (base_sym && base_sym->type_specification && base_sym->type_specification->kind == TYPE_KIND_ARRAY) {
                    generate_lvalue_address(generator, base, target_register);
                } else {
                    generate_expression(generator, base, target_register);
                }
            } else {
                generate_expression(generator, base, target_register);
            }

            /*
             * Proteção de Registrador contra Sobrescrita em Expressão de Índice:
             * A avaliação do nó 'index' pode requerer operações aritméticas intermediárias
             * ou chamadas que utilizam registradores voláteis (como 'r0' e 'r1'). Portanto,
             * salvamos o ponteiro base avaliado no slot de spill da pilha antes de avaliar o índice.
             */
            size_t base_spill_slot = generator->current_spill_offset + generator->current_spill_depth * 8;
            fprintf(generator->output_stream, "    [ra, %zu] := %s;\n", base_spill_slot, target_register);
            generator->total_emitted_instructions_count++;
            generator->current_spill_depth++;

            generate_expression(generator, index, "r3");

            generator->current_spill_depth--;
            fprintf(generator->output_stream, "    %s := [ra, %zu];\n", target_register, base_spill_slot);
            generator->total_emitted_instructions_count++;

            fprintf(generator->output_stream, "    r3 := r3 * 8;\n");
            fprintf(generator->output_stream, "    %s := %s + r3;\n", target_register, target_register);
            generator->total_emitted_instructions_count += 2;
            break;
        }

        case AST_NODE_EXPR_MEMBER_ACCESS: {
            const IjklAstNode* target = lvalue_node->data.member_access_expression.target_expression;
            const char* member_name = lvalue_node->data.member_access_expression.member_name;

            bool target_is_pointer = false;
            IjklTypeSpec* target_struct_type = NULL;

            if (target->kind == AST_NODE_EXPR_IDENTIFIER) {
                const char* target_name = target->data.identifier_expression.identifier_name;
                IjklSymbol* target_symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, target_name);
                if (target_symbol && target_symbol->type_specification) {
                    if (target_symbol->type_specification->kind == TYPE_KIND_POINTER) {
                        target_is_pointer = true;
                        target_struct_type = target_symbol->type_specification->details.pointer_info.base_type;
                    } else if (target_symbol->type_specification->kind == TYPE_KIND_STRUCT) {
                        target_struct_type = target_symbol->type_specification;
                    } else if (target_symbol->type_specification->kind == TYPE_KIND_USER_DEFINED &&
                               target_symbol->type_specification->details.user_type_name) {
                        IjklSymbol* type_sym = ijkl_symbol_table_lookup_lexical(generator->symbol_table,
                                                                               target_symbol->type_specification->details.user_type_name);
                        if (type_sym && type_sym->type_specification) {
                            if (type_sym->type_specification->kind == TYPE_KIND_POINTER) {
                                target_is_pointer = true;
                                target_struct_type = type_sym->type_specification->details.pointer_info.base_type;
                            } else {
                                target_struct_type = type_sym->type_specification;
                            }
                        }
                    }
                }
            } else if (target->kind == AST_NODE_EXPR_DEREFERENCE) {
                target_is_pointer = true;
                const IjklAstNode* inner_operand = target->data.dereference_expression.operand;
                if (inner_operand && inner_operand->kind == AST_NODE_EXPR_IDENTIFIER) {
                    IjklSymbol* sym = ijkl_symbol_table_lookup_lexical(generator->symbol_table, inner_operand->data.identifier_expression.identifier_name);
                    if (sym && sym->type_specification && sym->type_specification->kind == TYPE_KIND_POINTER) {
                        target_struct_type = sym->type_specification->details.pointer_info.base_type;
                    }
                }
            } else if (target->inferred_type && (target->inferred_type->kind == TYPE_KIND_POINTER ||
                                                 target->inferred_type->kind == TYPE_KIND_USER_DEFINED)) {
                target_is_pointer = true;
            }

            if (target->inferred_type && ijkl_type_is_boxed_primitive(target->inferred_type, NULL)) {
                target_is_pointer = true;
            }

            if (target_is_pointer) {
                if (target->kind == AST_NODE_EXPR_DEREFERENCE) {
                    generate_expression(generator, target->data.dereference_expression.operand, target_register);
                } else {
                    generate_expression(generator, target, target_register);
                }
            } else {
                generate_lvalue_address(generator, target, target_register);
            }

            size_t member_offset = 0;
            if (target_struct_type) {
                if (target_struct_type->kind == TYPE_KIND_USER_DEFINED && target_struct_type->details.user_type_name) {
                    IjklSymbol* sym = ijkl_symbol_table_lookup_lexical(generator->symbol_table,
                                                                       target_struct_type->details.user_type_name);
                    if (sym && sym->type_specification) {
                        target_struct_type = sym->type_specification;
                    }
                }

                if (target_struct_type->kind == TYPE_KIND_STRUCT) {
                    const IjklAstNode* field = target_struct_type->details.struct_info.field_declarations;
                    size_t curr_offset = 0;
                    while (field) {
                        const char* field_name = NULL;
                        if (field->kind == AST_NODE_PARAMETER) {
                            field_name = field->data.parameter.parameter_name;
                        } else if (field->kind == AST_NODE_DECL_VAR) {
                            field_name = field->data.variable_declaration.variable_name;
                        }

                        if (field_name && ijkl_string_case_insensitive_equals(field_name, member_name)) {
                            member_offset = curr_offset;
                            break;
                        }
                        curr_offset += 8;
                        field = field->next_sibling;
                    }
                }
            }

            if (target->inferred_type && ijkl_type_is_boxed_primitive(target->inferred_type, NULL)) {
                member_offset = 0;
            }

            if (member_offset > 0) {
                fprintf(generator->output_stream, "    %s := %s + %zu;\n", target_register, target_register, member_offset);
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        default:
            break;
    }
}

/*
 * Constrói o rótulo de destino de chamada (.ci), considerando qualificação modular
 * e a sobrecarga resolvida durante a análise semântica.
 */
static void resolve_callee_label_name(IjklCodeGenerator* generator,
                                      const IjklAstNode* call_node,
                                      const char* callee_name,
                                      char* output_buffer,
                                      size_t buffer_size) {
    if (!callee_name || buffer_size == 0) return;

    if (strncmp(callee_name, "__builtin_", 10) == 0) {
        snprintf(output_buffer, buffer_size, "%s", callee_name);
        return;
    }

    IjklSymbol* resolved_symbol = call_node ? call_node->resolved_symbol : NULL;
    if (!resolved_symbol) {
        if (strchr(callee_name, '.') == NULL) {
            resolved_symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, callee_name);
        } else {
            resolved_symbol = ijkl_symbol_table_lookup_qualified(generator->symbol_table, callee_name);
        }
    }

    if (resolved_symbol && resolved_symbol->identifier_name &&
        strncmp(resolved_symbol->identifier_name, "__builtin_", 10) == 0) {
        snprintf(output_buffer, buffer_size, "%s", resolved_symbol->identifier_name);
        return;
    }

    const char* mangled_routine_name = (resolved_symbol && resolved_symbol->mangled_identifier_name) ?
        resolved_symbol->mangled_identifier_name : callee_name;

    if (resolved_symbol && resolved_symbol->enclosing_scope &&
        strncmp(resolved_symbol->enclosing_scope->scope_name, "Module: ", 8) == 0) {
        const char* canonical_module_name = resolved_symbol->enclosing_scope->scope_name + 8;
        snprintf(output_buffer, buffer_size, "%s.%s", canonical_module_name, mangled_routine_name);
        return;
    }

    const char* dot_separator = strrchr(callee_name, '.');
    if (dot_separator != NULL) {
        size_t prefix_length = (size_t)(dot_separator - callee_name);
        char module_prefix[256];
        if (prefix_length < sizeof(module_prefix)) {
            strncpy(module_prefix, callee_name, prefix_length);
            module_prefix[prefix_length] = '\0';
            snprintf(output_buffer, buffer_size, "%s.%s", module_prefix, mangled_routine_name);
        } else {
            snprintf(output_buffer, buffer_size, "%s", mangled_routine_name);
        }
    } else {
        snprintf(output_buffer, buffer_size, "%s", mangled_routine_name);
    }
}

/*
 * EFEITO COLATERAL:
 * Avalia uma expressão da AST e emite as instruções que depositam o resultado no target_register.
 */
static void generate_expression(IjklCodeGenerator* generator,
                                const IjklAstNode* expression_node,
                                const char* target_register) {
    if (!expression_node) return;

    switch (expression_node->kind) {
        case AST_NODE_EXPR_LITERAL_INT:
            fprintf(generator->output_stream, "    %s := %lld;\n",
                    target_register,
                    (long long)expression_node->data.literal_integer.integer_value);
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_LITERAL_REAL:
            fprintf(generator->output_stream, "    %s := %f;\n",
                    target_register,
                    expression_node->data.literal_real.real_value);
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_LITERAL_FLOAT:
            fprintf(generator->output_stream, "    %s := %ff;\n",
                    target_register,
                    expression_node->data.literal_float.float_value);
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_LITERAL_CHAR: {
            char character_value = expression_node->data.literal_character.char_value;
            switch (character_value) {
                case '\n': fprintf(generator->output_stream, "    %s := '\\n';\n", target_register); break;
                case '\r': fprintf(generator->output_stream, "    %s := '\\r';\n", target_register); break;
                case '\t': fprintf(generator->output_stream, "    %s := '\\t';\n", target_register); break;
                case '\\': fprintf(generator->output_stream, "    %s := '\\\\';\n", target_register); break;
                case '\'': fprintf(generator->output_stream, "    %s := '\\'';\n", target_register); break;
                case '\0': fprintf(generator->output_stream, "    %s := '\\0';\n", target_register); break;
                default:   fprintf(generator->output_stream, "    %s := '%c';\n", target_register, character_value); break;
            }
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_LITERAL_STRING:
            fprintf(generator->output_stream, "    %s := \"", target_register);
            emit_escaped_string_literal(generator->output_stream,
                                        expression_node->data.literal_string.string_value ?
                                        expression_node->data.literal_string.string_value : "");
            fprintf(generator->output_stream, "\";\n");
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_LITERAL_BOOL:
            fprintf(generator->output_stream, "    %s := %s;\n",
                    target_register,
                    expression_node->data.literal_boolean.boolean_value ? "true" : "false");
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_LITERAL_NULL:
            fprintf(generator->output_stream, "    %s := null;\n", target_register);
            generator->total_emitted_instructions_count++;
            break;

        case AST_NODE_EXPR_IDENTIFIER: {
            const char* name = expression_node->data.identifier_expression.identifier_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, name);
            if (symbol && symbol->symbol_kind == SYMBOL_KIND_CONSTANT &&
                symbol->declaration_node &&
                symbol->declaration_node->kind == AST_NODE_DECL_CONST &&
                symbol->declaration_node->data.constant_declaration.initial_value_expression) {
                generate_expression(generator, symbol->declaration_node->data.constant_declaration.initial_value_expression, target_register);
                return;
            }
            if (symbol) {
                if (symbol->enclosing_scope &&
                    (symbol->enclosing_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                     symbol->enclosing_scope->scope_kind == SCOPE_KIND_MODULE)) {
                    fprintf(generator->output_stream, "    %s := [0x1000, %zu];\n",
                            target_register, symbol->memory_offset_in_bytes);
                } else if (symbol->is_by_reference) {
                    fprintf(generator->output_stream, "    %s := [ra, %zu];\n",
                            target_register, symbol->memory_offset_in_bytes);
                    fprintf(generator->output_stream, "    %s := [%s, 0];\n",
                            target_register, target_register);
                    generator->total_emitted_instructions_count++;
                } else {
                    fprintf(generator->output_stream, "    %s := [ra, %zu];\n",
                            target_register, symbol->memory_offset_in_bytes);
                }
            } else {
                fprintf(generator->output_stream, "    %s := 0;\n", target_register);
            }
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_BINARY: {
            IjklTokenType op = expression_node->data.binary_expression.operator_type;
            const IjklAstNode* left = expression_node->data.binary_expression.left_operand;
            const IjklAstNode* right = expression_node->data.binary_expression.right_operand;

            if (op == TOKEN_PLUS && (is_string_node(generator, left) || is_string_node(generator, right))) {
                /*
                 * Coerção Implícita e Concatenação de Texto:
                 * Se qualquer operando do operador '+' for do tipo texto, o outro operando
                 * é convertido automaticamente para texto chamando a rotina built-in apropriada,
                 * e a concatenação é efetuada através de '__builtin_str_concat'.
                 */
                /* 1. Avalia operando esquerdo em r0 e converte para string se necessário */
                generate_expression(generator, left, "r0");
                if (!is_string_node(generator, left)) {
                    IjklPrimitiveType left_prim = get_node_primitive_type(generator, left);
                    fprintf(generator->output_stream, "    push r0;\n");
                    generator->total_emitted_instructions_count++;
                    if (left_prim == PRIMITIVE_REAL || left_prim == PRIMITIVE_FLOAT) {
                        fprintf(generator->output_stream, "    call #__builtin_real_to_str;\n");
                    } else if (left_prim == PRIMITIVE_BOOL) {
                        fprintf(generator->output_stream, "    call #__builtin_bool_to_str;\n");
                    } else if (left_prim == PRIMITIVE_CHAR) {
                        fprintf(generator->output_stream, "    call #__builtin_char_to_str;\n");
                    } else {
                        fprintf(generator->output_stream, "    call #__builtin_int_to_str;\n");
                    }
                    generator->total_emitted_instructions_count++;
                }

                /* Salva o ponteiro da string esquerda no slot de spill do quadro ativo */
                size_t left_spill_slot = generator->current_spill_offset + generator->current_spill_depth * 8;
                fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", left_spill_slot);
                generator->total_emitted_instructions_count++;
                generator->current_spill_depth++;

                /* 2. Avalia operando direito em r1 e converte para string se necessário */
                generate_expression(generator, right, "r1");
                if (!is_string_node(generator, right)) {
                    IjklPrimitiveType right_prim = get_node_primitive_type(generator, right);
                    fprintf(generator->output_stream, "    push r1;\n");
                    generator->total_emitted_instructions_count++;
                    if (right_prim == PRIMITIVE_REAL || right_prim == PRIMITIVE_FLOAT) {
                        fprintf(generator->output_stream, "    call #__builtin_real_to_str;\n");
                    } else if (right_prim == PRIMITIVE_BOOL) {
                        fprintf(generator->output_stream, "    call #__builtin_bool_to_str;\n");
                    } else if (right_prim == PRIMITIVE_CHAR) {
                        fprintf(generator->output_stream, "    call #__builtin_char_to_str;\n");
                    } else {
                        fprintf(generator->output_stream, "    call #__builtin_int_to_str;\n");
                    }
                    generator->total_emitted_instructions_count++;
                    fprintf(generator->output_stream, "    r1 := r0;\n");
                    generator->total_emitted_instructions_count++;
                }

                /* 3. Restaura string esquerda do spill para r0 */
                generator->current_spill_depth--;
                fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", left_spill_slot);
                generator->total_emitted_instructions_count++;

                /* 4. Empilha argumentos e invoca '__builtin_str_concat' */
                fprintf(generator->output_stream, "    push r0;\n");
                fprintf(generator->output_stream, "    push r1;\n");
                fprintf(generator->output_stream, "    call #__builtin_str_concat;\n");
                generator->total_emitted_instructions_count += 3;

                if (strcmp(target_register, "r0") != 0) {
                    fprintf(generator->output_stream, "    %s := r0;\n", target_register);
                    generator->total_emitted_instructions_count++;
                }
                break;
            }

            generate_expression(generator, left, "r0");
            size_t left_spill_slot = generator->current_spill_offset + generator->current_spill_depth * 8;
            fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", left_spill_slot);
            generator->total_emitted_instructions_count++;
            generator->current_spill_depth++;

            generate_expression(generator, right, "r1");

            generator->current_spill_depth--;
            fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", left_spill_slot);
            generator->total_emitted_instructions_count++;

            const char* op_str = binary_operator_to_string(op);
            fprintf(generator->output_stream, "    %s := r0 %s r1;\n", target_register, op_str);
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_UNARY: {
            IjklTokenType op = expression_node->data.unary_expression.operator_type;
            const IjklAstNode* operand = expression_node->data.unary_expression.operand;

            if (op == TOKEN_AMPERSAND) {
                /* Operador de endereço: calcula o LEA da variável */
                generate_lvalue_address(generator, operand, target_register);
                return;
            }

            if (op == TOKEN_DOLLAR) {
                /* Custo de memória em bytes resolvido em tempo de compilação */
                size_t size_in_bytes = 8;
                if (operand->kind == AST_NODE_EXPR_IDENTIFIER) {
                    const char* name = operand->data.identifier_expression.identifier_name;
                    IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, name);
                    if (symbol) {
                        if (symbol->symbol_kind == SYMBOL_KIND_TYPE && symbol->type_specification &&
                            symbol->type_specification->kind == TYPE_KIND_POINTER &&
                            symbol->type_specification->details.pointer_info.base_type) {
                            size_in_bytes = ijkl_symbol_table_calculate_type_size(symbol->type_specification->details.pointer_info.base_type, generator->symbol_table);
                        } else if (symbol->memory_size_in_bytes > 0) {
                            size_in_bytes = symbol->memory_size_in_bytes;
                        }
                    }
                } else if (operand->kind == AST_NODE_EXPR_MEMBER_ACCESS) {
                    char qualified_name[256] = "";
                    build_qualified_identifier_name(operand, qualified_name, sizeof(qualified_name));
                    IjklSymbol* symbol = ijkl_symbol_table_lookup_qualified(generator->symbol_table, qualified_name);
                    if (symbol) {
                        if (symbol->symbol_kind == SYMBOL_KIND_TYPE && symbol->type_specification &&
                            symbol->type_specification->kind == TYPE_KIND_POINTER &&
                            symbol->type_specification->details.pointer_info.base_type) {
                            size_in_bytes = ijkl_symbol_table_calculate_type_size(symbol->type_specification->details.pointer_info.base_type, generator->symbol_table);
                        } else if (symbol->memory_size_in_bytes > 0) {
                            size_in_bytes = symbol->memory_size_in_bytes;
                        }
                    }
                }
                fprintf(generator->output_stream, "    %s := %zu;\n", target_register, size_in_bytes);
                generator->total_emitted_instructions_count++;
                return;
            }

            generate_expression(generator, operand, target_register);
            if (op == TOKEN_MINUS) {
                fprintf(generator->output_stream, "    %s := -%s;\n", target_register, target_register);
                generator->total_emitted_instructions_count++;
            } else if (op == TOKEN_KW_NOT || op == TOKEN_EXCLAMATION) {
                fprintf(generator->output_stream, "    %s := not %s;\n", target_register, target_register);
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_EXPR_DEREFERENCE: {
            const IjklAstNode* operand = expression_node->data.dereference_expression.operand;
            generate_expression(generator, operand, target_register);
            fprintf(generator->output_stream, "    %s := [%s, 0];\n", target_register, target_register);
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_MEMBER_ACCESS:
        case AST_NODE_EXPR_INDEX_ACCESS: {
            generate_lvalue_address(generator, expression_node, target_register);
            fprintf(generator->output_stream, "    %s := [%s, 0];\n", target_register, target_register);
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_EXPR_CALL: {
            char callee_qualified_name[256] = "";
            build_qualified_identifier_name(expression_node->data.call_expression.callable_expression,
                                            callee_qualified_name, sizeof(callee_qualified_name));
            const char* callee_name = callee_qualified_name[0] != '\0' ? callee_qualified_name : "rotina";

            char resolved_callee_name[512];
            resolve_callee_label_name(generator, expression_node, callee_name, resolved_callee_name, sizeof(resolved_callee_name));

            /* Empilha os argumentos da chamada */
            const IjklSymbol* routine_symbol = expression_node->resolved_symbol;
            const IjklAstNode* formal_param = (routine_symbol && routine_symbol->declaration_node &&
                                               routine_symbol->declaration_node->kind == AST_NODE_DECL_ROUTINE)
                                              ? routine_symbol->declaration_node->data.routine_declaration.parameters
                                              : NULL;

            const IjklAstNode* arg = expression_node->data.call_expression.argument_expressions;
            while (arg) {
                bool is_ref_param = (formal_param &&
                                     formal_param->kind == AST_NODE_PARAMETER &&
                                     formal_param->data.parameter.is_by_reference);
                if (is_ref_param) {
                    generate_lvalue_address(generator, arg, "r0");
                } else {
                    generate_expression(generator, arg, "r0");
                    if (formal_param && formal_param->kind == AST_NODE_PARAMETER) {
                        IjklTypeSpec* param_type = formal_param->data.parameter.type_specification;
                        IjklTypeSpec* arg_type = arg->inferred_type;
                        if (should_autobox_expression(param_type, arg_type)) {
                            emit_autobox_code(generator, "r0");
                        }
                    }
                }
                fprintf(generator->output_stream, "    push r0;\n");
                generator->total_emitted_instructions_count++;
                arg = arg->next_sibling;
                if (formal_param) formal_param = formal_param->next_sibling;
            }

            fprintf(generator->output_stream, "    call #%s;\n", resolved_callee_name);
            generator->total_emitted_instructions_count++;

            if (strcmp(target_register, "r0") != 0) {
                fprintf(generator->output_stream, "    %s := r0;\n", target_register);
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_EXPR_ALLOC: {
            generate_expression(generator, expression_node->data.allocation_expression.byte_count_expression, "r0");
            fprintf(generator->output_stream, "    %s := alloc(r0);\n", target_register);
            generator->total_emitted_instructions_count++;
            break;
        }

        default:
            break;
    }
}

/*
 * EFEITO COLATERAL:
 * Percorre recursivamente um nó de comando e emite suas instruções lineares correspondentes.
 */
static void generate_statement(IjklCodeGenerator* generator, const IjklAstNode* statement_node) {
    if (!statement_node) return;

    switch (statement_node->kind) {
        case AST_NODE_STMT_BLOCK: {
            char block_scope_name[64];
            snprintf(block_scope_name, sizeof(block_scope_name), "Block_L%zu_C%zu", statement_node->line_number, statement_node->column_number);
            IjklScope* previous_scope = generator->symbol_table->current_active_scope;
            IjklScope* child_scope = previous_scope ? previous_scope->first_child_scope : NULL;
            while (child_scope) {
                if (ijkl_string_case_insensitive_equals(child_scope->scope_name, block_scope_name)) {
                    generator->symbol_table->current_active_scope = child_scope;
                    break;
                }
                child_scope = child_scope->next_sibling_scope;
            }

            const IjklAstNode* child = statement_node->data.block_statement.statements;
            while (child) {
                generate_statement(generator, child);
                child = child->next_sibling;
            }

            generator->symbol_table->current_active_scope = previous_scope;
            break;
        }

        case AST_NODE_DECL_VAR: {
            if (statement_node->data.variable_declaration.initial_value_expression) {
                const char* var_name = statement_node->data.variable_declaration.variable_name;
                IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, var_name);
                size_t offset = symbol ? symbol->memory_offset_in_bytes : 0;

                generate_expression(generator,
                                    statement_node->data.variable_declaration.initial_value_expression,
                                    "r0");

                IjklTypeSpec* var_type = statement_node->data.variable_declaration.type_specification;
                if (!var_type && symbol) var_type = symbol->type_specification;
                IjklTypeSpec* init_type = statement_node->data.variable_declaration.initial_value_expression->inferred_type;
                if (should_autobox_expression(var_type, init_type)) {
                    emit_autobox_code(generator, "r0");
                }

                if (symbol && symbol->enclosing_scope &&
                    (symbol->enclosing_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                     symbol->enclosing_scope->scope_kind == SCOPE_KIND_MODULE)) {
                    fprintf(generator->output_stream, "    [0x1000, %zu] := r0;\n", offset);
                } else {
                    fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", offset);
                }
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_STMT_ASSIGN: {
            const IjklAstNode* target = statement_node->data.assignment_statement.target_lvalue;
            const IjklAstNode* value = statement_node->data.assignment_statement.value_expression;

            generate_expression(generator, value, "r0");

            IjklTypeSpec* target_type = target->inferred_type;
            if (!target_type && target->kind == AST_NODE_EXPR_IDENTIFIER) {
                IjklSymbol* s = ijkl_symbol_table_lookup_lexical(generator->symbol_table, target->data.identifier_expression.identifier_name);
                if (s) target_type = s->type_specification;
            }
            IjklTypeSpec* value_type = value->inferred_type;
            if (should_autobox_expression(target_type, value_type)) {
                emit_autobox_code(generator, "r0");
            }

            if (target->kind == AST_NODE_EXPR_IDENTIFIER) {
                const char* var_name = target->data.identifier_expression.identifier_name;
                IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, var_name);
                size_t offset = symbol ? symbol->memory_offset_in_bytes : 0;

                if (symbol && symbol->enclosing_scope &&
                    (symbol->enclosing_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                     symbol->enclosing_scope->scope_kind == SCOPE_KIND_MODULE)) {
                    fprintf(generator->output_stream, "    [0x1000, %zu] := r0;\n", offset);
                    generator->total_emitted_instructions_count++;
                } else if (symbol && symbol->is_by_reference) {
                    fprintf(generator->output_stream, "    r1 := [ra, %zu];\n", offset);
                    fprintf(generator->output_stream, "    [r1, 0] := r0;\n");
                    generator->total_emitted_instructions_count += 2;
                } else {
                    fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", offset);
                    generator->total_emitted_instructions_count++;
                }
            } else {
                /*
                 * Salva o valor da expressão à direita (r0) em slot de spill temporário
                 * para evitar que a computação do endereço de lvalue à esquerda (r1)
                 * corrompa o valor a ser atribuído.
                 */
                size_t val_spill_slot = generator->current_spill_offset + generator->current_spill_depth * 8;
                fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", val_spill_slot);
                generator->total_emitted_instructions_count++;
                generator->current_spill_depth++;

                generate_lvalue_address(generator, target, "r1");

                generator->current_spill_depth--;
                fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", val_spill_slot);
                generator->total_emitted_instructions_count++;

                fprintf(generator->output_stream, "    [r1, 0] := r0;\n");
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_STMT_IF: {
            const char* label_then = generate_unique_label(generator, "se_verdadeiro");
            const char* label_end = generate_unique_label(generator, "se_fim");

            generate_expression(generator, statement_node->data.if_statement.condition_expression, "r0");
            fprintf(generator->output_stream, "    jump_if r0, #%s;\n", label_then);
            generator->total_emitted_instructions_count++;

            if (statement_node->data.if_statement.else_branch_statement) {
                generate_statement(generator, statement_node->data.if_statement.else_branch_statement);
            }
            fprintf(generator->output_stream, "    jump #%s;\n", label_end);
            generator->total_emitted_instructions_count++;

            fprintf(generator->output_stream, "#%s:\n", label_then);
            generate_statement(generator, statement_node->data.if_statement.then_branch_statement);
            fprintf(generator->output_stream, "#%s:\n", label_end);
            break;
        }

        case AST_NODE_STMT_WHILE: {
            const char* label_start = generate_unique_label(generator, "enquanto_inicio");
            const char* label_end = generate_unique_label(generator, "enquanto_fim");

            if (generator->break_target_label_depth < IJKL_CODEGEN_MAX_BREAK_TARGETS) {
                generator->break_target_label_stack[generator->break_target_label_depth++] = label_end;
            }

            fprintf(generator->output_stream, "#%s:\n", label_start);
            generate_expression(generator, statement_node->data.while_statement.condition_expression, "r0");
            fprintf(generator->output_stream, "    r0 := not r0;\n");
            fprintf(generator->output_stream, "    jump_if r0, #%s;\n", label_end);
            generator->total_emitted_instructions_count += 2;

            generate_statement(generator, statement_node->data.while_statement.body_statement);

            fprintf(generator->output_stream, "    jump #%s;\n", label_start);
            fprintf(generator->output_stream, "#%s:\n", label_end);
            generator->total_emitted_instructions_count++;

            if (generator->break_target_label_depth > 0) {
                generator->break_target_label_depth--;
            }
            break;
        }

        case AST_NODE_STMT_FOR: {
            char for_scope_name[64];
            snprintf(for_scope_name, sizeof(for_scope_name), "ForLoop_L%zu", statement_node->line_number);
            IjklScope* previous_scope = generator->symbol_table->current_active_scope;
            IjklScope* child_scope = previous_scope ? previous_scope->first_child_scope : NULL;
            while (child_scope) {
                if (ijkl_string_case_insensitive_equals(child_scope->scope_name, for_scope_name)) {
                    generator->symbol_table->current_active_scope = child_scope;
                    break;
                }
                child_scope = child_scope->next_sibling_scope;
            }

            const char* var_name = statement_node->data.for_statement.control_variable_name;
            IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, var_name);
            size_t offset = symbol ? symbol->memory_offset_in_bytes : 0;

            const char* label_start = generate_unique_label(generator, "para_inicio");
            const char* label_end = generate_unique_label(generator, "para_fim");

            if (generator->break_target_label_depth < IJKL_CODEGEN_MAX_BREAK_TARGETS) {
                generator->break_target_label_stack[generator->break_target_label_depth++] = label_end;
            }

            /* Inicialização do contador */
            generate_expression(generator, statement_node->data.for_statement.start_expression, "r0");
            fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", offset);
            generator->total_emitted_instructions_count++;

            fprintf(generator->output_stream, "#%s:\n", label_start);

            /* Verificação do limite */
            generate_expression(generator, statement_node->data.for_statement.end_expression, "r1");
            fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", offset);
            fprintf(generator->output_stream, "    r0 := r0 <= r1;\n");
            fprintf(generator->output_stream, "    r0 := not r0;\n");
            fprintf(generator->output_stream, "    jump_if r0, #%s;\n", label_end);
            generator->total_emitted_instructions_count += 4;

            /* Corpo do laço */
            generate_statement(generator, statement_node->data.for_statement.body_statement);

            /* Incremento do passo */
            if (statement_node->data.for_statement.step_expression) {
                generate_expression(generator, statement_node->data.for_statement.step_expression, "r1");
            } else {
                fprintf(generator->output_stream, "    r1 := 1;\n");
                generator->total_emitted_instructions_count++;
            }
            fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", offset);
            fprintf(generator->output_stream, "    r0 := r0 + r1;\n");
            fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", offset);
            fprintf(generator->output_stream, "    jump #%s;\n", label_start);
            fprintf(generator->output_stream, "#%s:\n", label_end);
            generator->total_emitted_instructions_count += 4;

            if (generator->break_target_label_depth > 0) {
                generator->break_target_label_depth--;
            }

            generator->symbol_table->current_active_scope = previous_scope;
            break;
        }

        case AST_NODE_STMT_REPEAT: {
            const char* label_start = generate_unique_label(generator, "repita_inicio");
            const char* label_end = generate_unique_label(generator, "repita_fim");

            if (generator->break_target_label_depth < IJKL_CODEGEN_MAX_BREAK_TARGETS) {
                generator->break_target_label_stack[generator->break_target_label_depth++] = label_end;
            }

            fprintf(generator->output_stream, "#%s:\n", label_start);
            generate_statement(generator, statement_node->data.repeat_statement.body_block);

            generate_expression(generator, statement_node->data.repeat_statement.condition_expression, "r0");
            fprintf(generator->output_stream, "    r0 := not r0;\n");
            fprintf(generator->output_stream, "    jump_if r0, #%s;\n", label_start);
            fprintf(generator->output_stream, "#%s:\n", label_end);
            generator->total_emitted_instructions_count += 2;

            if (generator->break_target_label_depth > 0) {
                generator->break_target_label_depth--;
            }
            break;
        }

        case AST_NODE_STMT_SWITCH: {
            const char* label_end = generate_unique_label(generator, "escolha_fim");

            if (generator->break_target_label_depth < IJKL_CODEGEN_MAX_BREAK_TARGETS) {
                generator->break_target_label_stack[generator->break_target_label_depth++] = label_end;
            }

            generate_expression(generator, statement_node->data.switch_statement.condition_expression, "r0");
            size_t switch_spill_slot = generator->current_spill_offset + generator->current_spill_depth * 8;
            fprintf(generator->output_stream, "    [ra, %zu] := r0;\n", switch_spill_slot);
            generator->total_emitted_instructions_count++;
            generator->current_spill_depth++;

            const IjklAstNode* branch = statement_node->data.switch_statement.case_branches;
            while (branch) {
                if (branch->kind == AST_NODE_CASE_BRANCH) {
                    const char* label_case = generate_unique_label(generator, "caso");
                    generate_expression(generator, branch->data.case_branch.match_expression, "r1");
                    fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", switch_spill_slot);
                    fprintf(generator->output_stream, "    r2 := r0 = r1;\n");
                    fprintf(generator->output_stream, "    jump_if r2, #%s;\n", label_case);
                    generator->total_emitted_instructions_count += 3;

                    const char* label_skip = generate_unique_label(generator, "caso_salto");
                    fprintf(generator->output_stream, "    jump #%s;\n", label_skip);
                    generator->total_emitted_instructions_count++;

                    fprintf(generator->output_stream, "#%s:\n", label_case);
                    const IjklAstNode* stmt = branch->data.case_branch.statements;
                    while (stmt) {
                        generate_statement(generator, stmt);
                        stmt = stmt->next_sibling;
                    }
                    fprintf(generator->output_stream, "    jump #%s;\n", label_end);
                    generator->total_emitted_instructions_count++;

                    fprintf(generator->output_stream, "#%s:\n", label_skip);
                }
                branch = branch->next_sibling;
            }

            if (statement_node->data.switch_statement.default_branch) {
                const IjklAstNode* def_stmt = statement_node->data.switch_statement.default_branch;
                while (def_stmt) {
                    generate_statement(generator, def_stmt);
                    def_stmt = def_stmt->next_sibling;
                }
            }
            generator->current_spill_depth--;
            fprintf(generator->output_stream, "#%s:\n", label_end);

            if (generator->break_target_label_depth > 0) {
                generator->break_target_label_depth--;
            }
            break;
        }

        case AST_NODE_STMT_BREAK: {
            if (generator->break_target_label_depth > 0) {
                const char* target_label = generator->break_target_label_stack[generator->break_target_label_depth - 1];
                fprintf(generator->output_stream, "    jump #%s;\n", target_label);
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_STMT_RETURN: {
            if (statement_node->data.return_statement.return_value_expression) {
                generate_expression(generator,
                                    statement_node->data.return_statement.return_value_expression,
                                    "r0");
                fprintf(generator->output_stream, "    free_frame;\n");
                fprintf(generator->output_stream, "    return r0;\n");
            } else {
                fprintf(generator->output_stream, "    free_frame;\n");
                fprintf(generator->output_stream, "    return;\n");
            }
            generator->total_emitted_instructions_count += 2;
            break;
        }

        case AST_NODE_STMT_WRITE: {
            const IjklAstNode* arg = statement_node->data.write_statement.argument_expressions;
            while (arg) {
                generate_expression(generator, arg, "r0");
                fprintf(generator->output_stream, "    out 0x0001, r0;\n");
                generator->total_emitted_instructions_count++;
                arg = arg->next_sibling;
            }
            if (statement_node->data.write_statement.is_newline) {
                fprintf(generator->output_stream, "    out 0x0000, '\\n';\n");
                generator->total_emitted_instructions_count++;
            }
            break;
        }

        case AST_NODE_STMT_READ: {
            const IjklAstNode* target = statement_node->data.read_statement.target_lvalues;
            while (target) {
                if (target->kind == AST_NODE_EXPR_IDENTIFIER) {
                    const char* var_name = target->data.identifier_expression.identifier_name;
                    IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, var_name);
                    size_t offset = symbol ? symbol->memory_offset_in_bytes : 0;
                    if (symbol && symbol->enclosing_scope &&
                        (symbol->enclosing_scope->scope_kind == SCOPE_KIND_GLOBAL ||
                         symbol->enclosing_scope->scope_kind == SCOPE_KIND_MODULE)) {
                        fprintf(generator->output_stream, "    in 0x0003, [0x1000, %zu];\n", offset);
                        generator->total_emitted_instructions_count++;
                    } else if (symbol && symbol->is_by_reference) {
                        fprintf(generator->output_stream, "    r1 := [ra, %zu];\n", offset);
                        fprintf(generator->output_stream, "    in 0x0003, [r1, 0];\n");
                        generator->total_emitted_instructions_count += 2;
                    } else {
                        fprintf(generator->output_stream, "    in 0x0003, [ra, %zu];\n", offset);
                        generator->total_emitted_instructions_count++;
                    }
                } else {
                    generate_lvalue_address(generator, target, "r1");
                    fprintf(generator->output_stream, "    in 0x0003, [r1, 0];\n");
                    generator->total_emitted_instructions_count++;
                }
                target = target->next_sibling;
            }
            break;
        }

        case AST_NODE_STMT_FREE: {
            generate_expression(generator, statement_node->data.free_statement.target_lvalue, "r0");
            fprintf(generator->output_stream, "    free(r0);\n");
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_STMT_CALL: {
            char callee_qualified_name[256] = "";
            build_qualified_identifier_name(statement_node->data.call_statement.callable_expression,
                                            callee_qualified_name, sizeof(callee_qualified_name));
            const char* callee_name = callee_qualified_name[0] != '\0' ? callee_qualified_name : "rotina";

            char resolved_callee_name[512];
            resolve_callee_label_name(generator, statement_node, callee_name, resolved_callee_name, sizeof(resolved_callee_name));

            const IjklSymbol* routine_symbol = statement_node->resolved_symbol;
            const IjklAstNode* formal_param = (routine_symbol && routine_symbol->declaration_node &&
                                               routine_symbol->declaration_node->kind == AST_NODE_DECL_ROUTINE)
                                              ? routine_symbol->declaration_node->data.routine_declaration.parameters
                                              : NULL;

            const IjklAstNode* arg = statement_node->data.call_statement.argument_expressions;
            while (arg) {
                bool is_ref_param = (formal_param &&
                                     formal_param->kind == AST_NODE_PARAMETER &&
                                     formal_param->data.parameter.is_by_reference);
                if (is_ref_param) {
                    generate_lvalue_address(generator, arg, "r0");
                } else {
                    generate_expression(generator, arg, "r0");
                    if (formal_param && formal_param->kind == AST_NODE_PARAMETER) {
                        IjklTypeSpec* param_type = formal_param->data.parameter.type_specification;
                        IjklTypeSpec* arg_type = arg->inferred_type;
                        if (should_autobox_expression(param_type, arg_type)) {
                            emit_autobox_code(generator, "r0");
                        }
                    }
                }
                fprintf(generator->output_stream, "    push r0;\n");
                generator->total_emitted_instructions_count++;
                arg = arg->next_sibling;
                if (formal_param) formal_param = formal_param->next_sibling;
            }

            fprintf(generator->output_stream, "    call #%s;\n", resolved_callee_name);
            generator->total_emitted_instructions_count++;
            break;
        }

        case AST_NODE_STMT_ASM: {
            const char* ci_code = statement_node->data.asm_statement.ci_code_text;
            if (!ci_code) break;

            const char* line_cursor = ci_code;
            while (*line_cursor != '\0') {
                while (*line_cursor == ' ' || *line_cursor == '\t' || *line_cursor == '\r') {
                    line_cursor++;
                }

                if (*line_cursor == '\n') {
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
                    if (*line_start == '#') {
                        fprintf(generator->output_stream, "%.*s\n", (int)line_length, line_start);
                    } else if (line_length >= 2 && line_start[0] == '/' && line_start[1] == '/') {
                        fprintf(generator->output_stream, "    %.*s\n", (int)line_length, line_start);
                    } else {
                        fprintf(generator->output_stream, "    %.*s\n", (int)line_length, line_start);
                        generator->total_emitted_instructions_count++;
                    }
                }

                line_cursor = (*line_end == '\n') ? (line_end + 1) : line_end;
            }
            break;
        }

        default:
            break;
    }
}

/*
 * EFEITO COLATERAL:
 * Emite a declaração de uma rotina com cálculo dinâmico do quadro de ativação.
 */
static const char* find_lifecycle_routine_name(const IjklAstNode* unit_node, bool is_initialize) {
    if (!unit_node) return NULL;
    const IjklAstNode* decl = NULL;
    if (unit_node->kind == AST_NODE_PROGRAM) {
        decl = unit_node->data.program.declarations;
    } else if (unit_node->kind == AST_NODE_MODULE) {
        decl = unit_node->data.module_declaration.declarations;
    }
    while (decl) {
        if (decl->kind == AST_NODE_DECL_ROUTINE && !decl->data.routine_declaration.is_public) {
            const char* name = decl->data.routine_declaration.routine_name;
            if (is_initialize) {
                if (ijkl_string_case_insensitive_equals(name, "inicializar") ||
                    ijkl_string_case_insensitive_equals(name, "initialize") ||
                    ijkl_string_case_insensitive_equals(name, "initialiser")) {
                    return name;
                }
            } else {
                if (ijkl_string_case_insensitive_equals(name, "finalizar") ||
                    ijkl_string_case_insensitive_equals(name, "finalize") ||
                    ijkl_string_case_insensitive_equals(name, "finaliser")) {
                    return name;
                }
            }
        }
        decl = decl->next_sibling;
    }
    return NULL;
}

static const IjklAstNode* find_entry_routine_node(const IjklAstNode* program_node) {
    if (!program_node || program_node->kind != AST_NODE_PROGRAM) return NULL;
    const IjklAstNode* decl = program_node->data.program.declarations;
    while (decl) {
        if (decl->kind == AST_NODE_DECL_ROUTINE && !decl->data.routine_declaration.is_public) {
            const char* name = decl->data.routine_declaration.routine_name;
            if (ijkl_string_case_insensitive_equals(name, "principal") ||
                ijkl_string_case_insensitive_equals(name, "main")) {
                return decl;
            }
        }
        decl = decl->next_sibling;
    }
    return NULL;
}

/*
 * EFEITO COLATERAL:
 * Emite a declaração de uma rotina com cálculo dinâmico do quadro de ativação.
 */
static const char* get_builtin_alias_name(const char* module_path, const char* mangled_name) {
    if (!module_path || !mangled_name) return NULL;

    bool is_text = (strcmp(module_path, "ijkL.Text") == 0 ||
                    strcmp(module_path, "ijkL.Texto") == 0 ||
                    strcmp(module_path, "ijkL.Texte") == 0);
    if (is_text) {
        if (strcmp(mangled_name, "length$string") == 0 || strcmp(mangled_name, "length") == 0) return "__builtin_str_len";
        if (strcmp(mangled_name, "compare$string$string") == 0 || strcmp(mangled_name, "compare") == 0) return "__builtin_str_cmp";
        if (strcmp(mangled_name, "concat$string$string") == 0 || strcmp(mangled_name, "concat") == 0) return "__builtin_str_concat";
        if (strcmp(mangled_name, "substring$string$int$int") == 0 || strcmp(mangled_name, "substring") == 0) return "__builtin_str_sub";
        if (strcmp(mangled_name, "create$int") == 0 || strcmp(mangled_name, "create") == 0) return "__builtin_str_create";
        if (strcmp(mangled_name, "get$string$int") == 0 || strcmp(mangled_name, "get") == 0 ||
            strcmp(mangled_name, "getChar$string$int") == 0 || strcmp(mangled_name, "getChar") == 0) return "__builtin_str_get_char";
        if (strcmp(mangled_name, "change$string$int$char") == 0 || strcmp(mangled_name, "change") == 0 ||
            strcmp(mangled_name, "changeChar$string$int$char") == 0 || strcmp(mangled_name, "changeChar") == 0) return "__builtin_str_set_char";
    }

    bool is_convert = (strcmp(module_path, "ijkL.Convert") == 0 ||
                       strcmp(module_path, "ijkL.Conversao") == 0 ||
                       strcmp(module_path, "ijkL.Conversion") == 0);
    if (is_convert) {
        if (strcmp(mangled_name, "toInt$string") == 0) return "__builtin_str_to_int";
        if (strcmp(mangled_name, "toReal$string") == 0) return "__builtin_str_to_real";
        if (strcmp(mangled_name, "toBool$string") == 0) return "__builtin_str_to_bool";
        if (strcmp(mangled_name, "toString$int") == 0) return "__builtin_int_to_str";
        if (strcmp(mangled_name, "toString$bool") == 0) return "__builtin_bool_to_str";
        if (strcmp(mangled_name, "toString$char") == 0) return "__builtin_char_to_str";
    }

    bool is_memory = (strcmp(module_path, "ijkL.Memory") == 0 ||
                      strcmp(module_path, "ijkL.Memoria") == 0 ||
                      strcmp(module_path, "ijkL.Memoire") == 0);
    if (is_memory) {
        if (strcmp(mangled_name, "copy$pointer$pointer$int") == 0 || strcmp(mangled_name, "copy") == 0 ||
            strcmp(mangled_name, "copiar$pointer$pointer$int") == 0 || strcmp(mangled_name, "copiar") == 0) return "__builtin_mem_copy";
        if (strcmp(mangled_name, "fill$pointer$int$int") == 0 || strcmp(mangled_name, "fill") == 0 ||
            strcmp(mangled_name, "preencher$pointer$int$int") == 0 || strcmp(mangled_name, "preencher") == 0) return "__builtin_mem_fill";
        if (strcmp(mangled_name, "compare$pointer$pointer$int") == 0 || strcmp(mangled_name, "compare") == 0 ||
            strcmp(mangled_name, "comparar$pointer$pointer$int") == 0 || strcmp(mangled_name, "comparar") == 0) return "__builtin_mem_cmp";
    }

    return NULL;
}

static const char* find_routine_target_builtin(const IjklAstNode* routine_node) {
    if (!routine_node) return NULL;
    const IjklAstNode* body = routine_node->data.routine_declaration.routine_body;
    while (body) {
        if (body->kind == AST_NODE_STMT_ASM && body->data.asm_statement.target_builtin_name) {
            return body->data.asm_statement.target_builtin_name;
        }
        body = body->next_sibling;
    }
    return NULL;
}

static void generate_routine_declaration(IjklCodeGenerator* generator,
                                        const IjklAstNode* routine_node) {
    if (!routine_node) return;

    const char* routine_name = routine_node->data.routine_declaration.routine_name;
    IjklSymbol* symbol = ijkl_symbol_table_lookup_lexical(generator->symbol_table, routine_name);
    while (symbol) {
        if (symbol->declaration_node == routine_node) {
            break;
        }
        symbol = symbol->next_overload;
    }

    size_t frame_size = 0;
    IjklScope* previous_scope = generator->symbol_table->current_active_scope;
    if (symbol && symbol->internal_scope) {
        frame_size = symbol->internal_scope->current_memory_offset;
        generator->symbol_table->current_active_scope = symbol->internal_scope;
    }

    size_t local_bytes = frame_size;
    generator->current_spill_offset = local_bytes;
    generator->current_spill_depth = 0;
    frame_size = local_bytes + 64;

    bool is_lifecycle = (strcmp(routine_name, "inicializar") == 0 ||
                         strcmp(routine_name, "initialize") == 0 ||
                         strcmp(routine_name, "initialiser") == 0 ||
                         strcmp(routine_name, "finalizar") == 0 ||
                         strcmp(routine_name, "finalize") == 0 ||
                         strcmp(routine_name, "finaliser") == 0);

    const char* mangled_name = (symbol && symbol->mangled_identifier_name) ?
        symbol->mangled_identifier_name : routine_name;

    const char* builtin_alias = find_routine_target_builtin(routine_node);
    if (!builtin_alias) {
        builtin_alias = get_builtin_alias_name(generator->current_module_path, mangled_name);
    }

    if (generator->current_module_path && generator->current_module_path[0] != '\0') {
        if (is_lifecycle) {
            fprintf(generator->output_stream, "\n#%s.%s:\n", generator->current_module_path, routine_name);
        } else {
            fprintf(generator->output_stream, "\n#%s.%s:\n", generator->current_module_path, mangled_name);
            fprintf(generator->output_stream, "#%s:\n", mangled_name);
            if (symbol && !symbol->is_overloaded && strcmp(mangled_name, routine_name) != 0) {
                fprintf(generator->output_stream, "#%s.%s:\n", generator->current_module_path, routine_name);
                fprintf(generator->output_stream, "#%s:\n", routine_name);
            }
            if (builtin_alias && !generator->use_native_builtins) {
                fprintf(generator->output_stream, "#%s:\n", builtin_alias);
            }
        }
    } else if (generator->current_program_name && generator->current_program_name[0] != '\0') {
        bool is_entry_point = (!routine_node->data.routine_declaration.is_public &&
                               (strcmp(routine_name, "main") == 0 ||
                                strcmp(routine_name, "principal") == 0));
        if (is_entry_point) {
            bool is_main_word = (strcmp(routine_name, "main") == 0);
            const char* target_label = is_main_word ? "main_impl" : routine_name;
            fprintf(generator->output_stream, "\n#%s.%s:\n", generator->current_program_name, target_label);
            fprintf(generator->output_stream, "#%s:\n", target_label);
        } else if (is_lifecycle) {
            fprintf(generator->output_stream, "\n#%s.%s:\n", generator->current_program_name, routine_name);
        } else {
            fprintf(generator->output_stream, "\n#%s.%s:\n", generator->current_program_name, mangled_name);
            fprintf(generator->output_stream, "#%s:\n", mangled_name);
            if (symbol && !symbol->is_overloaded && strcmp(mangled_name, routine_name) != 0) {
                fprintf(generator->output_stream, "#%s.%s:\n", generator->current_program_name, routine_name);
                fprintf(generator->output_stream, "#%s:\n", routine_name);
            }
        }
    } else {
        fprintf(generator->output_stream, "\n#%s:\n", mangled_name);
        if (symbol && !symbol->is_overloaded && strcmp(mangled_name, routine_name) != 0) {
            fprintf(generator->output_stream, "#%s:\n", routine_name);
        }
    }
    if (generator->use_native_builtins && builtin_alias != NULL) {
        fprintf(generator->output_stream, "    alloc_frame %zu;\n", frame_size);
        size_t parameter_index = 0;
        const IjklAstNode* param_cursor = routine_node->data.routine_declaration.parameters;
        while (param_cursor) {
            fprintf(generator->output_stream, "    r0 := [ra, %zu];\n", parameter_index * 8);
            fprintf(generator->output_stream, "    push r0;\n");
            parameter_index++;
            param_cursor = param_cursor->next_sibling;
        }
        fprintf(generator->output_stream, "    call #%s;\n", builtin_alias);
        fprintf(generator->output_stream, "    free_frame;\n");
        fprintf(generator->output_stream, "    return r0;\n");
        generator->total_emitted_instructions_count += (parameter_index * 2) + 4;

        generator->symbol_table->current_active_scope = previous_scope;
        return;
    }
    fprintf(generator->output_stream, "    alloc_frame %zu;\n", frame_size);
    generator->total_emitted_instructions_count++;

    const char* previous_routine = generator->current_routine_name;
    generator->current_routine_name = routine_name;

    const IjklAstNode* body = routine_node->data.routine_declaration.routine_body;
    while (body) {
        generate_statement(generator, body);
        body = body->next_sibling;
    }

    /* Epílogo de segurança caso a rotina encerre sem return explícito */
    fprintf(generator->output_stream, "    free_frame;\n");
    fprintf(generator->output_stream, "    return;\n");
    generator->total_emitted_instructions_count += 2;

    generator->current_routine_name = previous_routine;
    generator->symbol_table->current_active_scope = previous_scope;
}

/*
 * EFEITO COLATERAL:
 * Percorre recursivamente a AST e emite todo o código intermediário linear (*.ci).
 */
bool ijkl_code_generator_generate(IjklCodeGenerator* code_generator,
                                  const IjklAstNode* compilation_unit_root) {
    if (!code_generator || !compilation_unit_root) return false;

    fprintf(code_generator->output_stream, "// ==============================================================================\n");
    fprintf(code_generator->output_stream, "// ijkLang (ijkL) - Linear Intermediate Code (.ci)\n");
    fprintf(code_generator->output_stream, "// Generated automatically by ijkLang Compiler\n");
    fprintf(code_generator->output_stream, "// ==============================================================================\n\n");

    /* Emite salto inicial para o ponto de entrada principal do programa caso exista */
    const IjklAstNode* unit_scan = compilation_unit_root;
    while (unit_scan) {
        if (unit_scan->kind == AST_NODE_PROGRAM) {
            fprintf(code_generator->output_stream, "    jump #%s.main;\n\n", unit_scan->data.program.program_name);
            code_generator->total_emitted_instructions_count++;
            break;
        }
        unit_scan = unit_scan->next_sibling;
    }


    const IjklAstNode* current_unit = compilation_unit_root;
    while (current_unit) {
        code_generator->symbol_table->active_compilation_unit = current_unit;
        if (current_unit->kind == AST_NODE_PROGRAM) {
            const char* program_name = current_unit->data.program.program_name;
            IjklSymbol* program_symbol = ijkl_symbol_table_lookup_lexical(code_generator->symbol_table, program_name);
            IjklScope* previous_scope = code_generator->symbol_table->current_active_scope;
            if (program_symbol && program_symbol->internal_scope) {
                code_generator->symbol_table->current_active_scope = program_symbol->internal_scope;
            }

            code_generator->current_program_name = program_name;

            /* 1. Emite declarações de funções e procedimentos */
            const IjklAstNode* declaration = current_unit->data.program.declarations;
            while (declaration) {
                if (declaration->kind == AST_NODE_DECL_ROUTINE) {
                    generate_routine_declaration(code_generator, declaration);
                } else if (declaration->kind == AST_NODE_STMT_ASM) {
                    generate_statement(code_generator, declaration);
                }
                declaration = declaration->next_sibling;
            }

            code_generator->current_program_name = NULL;
            code_generator->symbol_table->current_active_scope = previous_scope;
        } else if (current_unit->kind == AST_NODE_MODULE) {
            const char* module_path = current_unit->data.module_declaration.qualified_module_path;
            IjklSymbol* module_symbol = ijkl_symbol_table_lookup_qualified(code_generator->symbol_table, module_path);
            IjklScope* previous_scope = code_generator->symbol_table->current_active_scope;
            if (module_symbol && module_symbol->internal_scope) {
                code_generator->symbol_table->current_active_scope = module_symbol->internal_scope;
            }

            fprintf(code_generator->output_stream, "\n// --- Module: %s ---\n", module_path);
            code_generator->current_module_path = module_path;

            const IjklAstNode* declaration = current_unit->data.module_declaration.declarations;
            while (declaration) {
                if (declaration->kind == AST_NODE_DECL_ROUTINE) {
                    generate_routine_declaration(code_generator, declaration);
                } else if (declaration->kind == AST_NODE_STMT_ASM) {
                    generate_statement(code_generator, declaration);
                }
                declaration = declaration->next_sibling;
            }

            code_generator->current_module_path = NULL;
            code_generator->symbol_table->current_active_scope = previous_scope;
        }

        current_unit = current_unit->next_sibling;
    }

    /* 2. Emite o ponto de entrada bootstrap do programa com gerenciamento de ciclo de vida */
    const IjklAstNode* program_unit = NULL;
    const IjklAstNode* prog_search = compilation_unit_root;
    while (prog_search) {
        if (prog_search->kind == AST_NODE_PROGRAM) {
            program_unit = prog_search;
            break;
        }
        prog_search = prog_search->next_sibling;
    }

    if (program_unit) {
        const char* program_name = program_unit->data.program.program_name;
        IjklSymbol* program_symbol = ijkl_symbol_table_lookup_lexical(code_generator->symbol_table, program_name);
        IjklScope* previous_scope = code_generator->symbol_table->current_active_scope;
        if (program_symbol && program_symbol->internal_scope) {
            code_generator->symbol_table->current_active_scope = program_symbol->internal_scope;
        }
        code_generator->current_program_name = program_name;

        fprintf(code_generator->output_stream, "\n// ==============================================================================\n");
        fprintf(code_generator->output_stream, "// Bootstrap & Lifecycle Management Sequence\n");
        fprintf(code_generator->output_stream, "// ==============================================================================\n");
        fprintf(code_generator->output_stream, "#%s.main:\n", program_name);
        fprintf(code_generator->output_stream, "#main:\n");
        code_generator->current_spill_offset = 16;
        code_generator->current_spill_depth = 0;
        fprintf(code_generator->output_stream, "    alloc_frame 80;\n");
        code_generator->total_emitted_instructions_count++;

        /* A. Inicialização de variáveis estáticas de todos os módulos */
        const IjklAstNode* mod_scan = compilation_unit_root;
        while (mod_scan) {
            if (mod_scan->kind == AST_NODE_MODULE) {
                const char* mod_path = mod_scan->data.module_declaration.qualified_module_path;
                IjklSymbol* mod_sym = ijkl_symbol_table_lookup_qualified(code_generator->symbol_table, mod_path);
                IjklScope* prev_mod_scope = code_generator->symbol_table->current_active_scope;
                if (mod_sym && mod_sym->internal_scope) {
                    code_generator->symbol_table->current_active_scope = mod_sym->internal_scope;
                }
                const IjklAstNode* decl = mod_scan->data.module_declaration.declarations;
                while (decl) {
                    if (decl->kind == AST_NODE_DECL_VAR && decl->data.variable_declaration.initial_value_expression) {
                        generate_statement(code_generator, decl);
                    }
                    decl = decl->next_sibling;
                }
                code_generator->symbol_table->current_active_scope = prev_mod_scope;
            }
            mod_scan = mod_scan->next_sibling;
        }

        /* B. Inicialização de variáveis estáticas do programa */
        IjklSymbol* prog_sym = ijkl_symbol_table_lookup_lexical(code_generator->symbol_table, program_name);
        IjklScope* prev_prog_scope = code_generator->symbol_table->current_active_scope;
        if (prog_sym && prog_sym->internal_scope) {
            code_generator->symbol_table->current_active_scope = prog_sym->internal_scope;
        }
        const IjklAstNode* prog_decl = program_unit->data.program.declarations;
        while (prog_decl) {
            if (prog_decl->kind == AST_NODE_DECL_VAR && prog_decl->data.variable_declaration.initial_value_expression) {
                generate_statement(code_generator, prog_decl);
            }
            prog_decl = prog_decl->next_sibling;
        }
        code_generator->symbol_table->current_active_scope = prev_prog_scope;

        /* C. Executa rotinas de ciclo de vida 'inicializar' dos módulos importados */
        mod_scan = compilation_unit_root;
        while (mod_scan) {
            if (mod_scan->kind == AST_NODE_MODULE) {
                const char* init_name = find_lifecycle_routine_name(mod_scan, true);
                if (init_name) {
                    const char* mod_path = mod_scan->data.module_declaration.qualified_module_path;
                    fprintf(code_generator->output_stream, "    call #%s.%s;\n", mod_path, init_name);
                    code_generator->total_emitted_instructions_count++;
                }
            }
            mod_scan = mod_scan->next_sibling;
        }

        /* D. Executa rotina de ciclo de vida 'inicializar' do programa (se definida) */
        const char* prog_init_name = find_lifecycle_routine_name(program_unit, true);
        if (prog_init_name) {
            fprintf(code_generator->output_stream, "    call #%s.%s;\n", program_name, prog_init_name);
            code_generator->total_emitted_instructions_count++;
        }

        /* E. Invoca o ponto de entrada principal 'principal' / 'main' */
        const IjklAstNode* entry_routine = find_entry_routine_node(program_unit);
        if (entry_routine) {
            const char* entry_name = entry_routine->data.routine_declaration.routine_name;
            bool is_main_word = ijkl_string_case_insensitive_equals(entry_name, "main");
            const char* target_call_label = is_main_word ? "main_impl" : entry_name;

            size_t param_count = 0;
            const IjklAstNode* p = entry_routine->data.routine_declaration.parameters;
            while (p) {
                param_count++;
                p = p->next_sibling;
            }

            if (param_count == 2) {
                fprintf(code_generator->output_stream, "    r0 := [ra, 0];\n");
                fprintf(code_generator->output_stream, "    push r0;\n");
                fprintf(code_generator->output_stream, "    r0 := [ra, 8];\n");
                fprintf(code_generator->output_stream, "    push r0;\n");
                code_generator->total_emitted_instructions_count += 4;
            }

            fprintf(code_generator->output_stream, "    call #%s.%s;\n", program_name, target_call_label);
            code_generator->total_emitted_instructions_count++;
        }

        /* Salva código de retorno da função principal */
        fprintf(code_generator->output_stream, "    r1 := r0;\n");
        code_generator->total_emitted_instructions_count++;

        /* F. Executa rotina de ciclo de vida 'finalizar' do programa (se definida) */
        const char* prog_fin_name = find_lifecycle_routine_name(program_unit, false);
        if (prog_fin_name) {
            fprintf(code_generator->output_stream, "    call #%s.%s;\n", program_name, prog_fin_name);
            code_generator->total_emitted_instructions_count++;
        }

        /* G. Executa rotinas de ciclo de vida 'finalizar' dos módulos importados (em ordem inversa) */
        size_t total_mods = 0;
        mod_scan = compilation_unit_root;
        while (mod_scan) {
            if (mod_scan->kind == AST_NODE_MODULE) total_mods++;
            mod_scan = mod_scan->next_sibling;
        }

        if (total_mods > 0) {
            const IjklAstNode** mod_array = (const IjklAstNode**)ijkl_memory_arena_allocate(
                code_generator->memory_arena, total_mods * sizeof(const IjklAstNode*));
            size_t idx = 0;
            mod_scan = compilation_unit_root;
            while (mod_scan) {
                if (mod_scan->kind == AST_NODE_MODULE) {
                    mod_array[idx++] = mod_scan;
                }
                mod_scan = mod_scan->next_sibling;
            }

            for (size_t i = total_mods; i > 0; --i) {
                const IjklAstNode* m = mod_array[i - 1];
                const char* fin_name = find_lifecycle_routine_name(m, false);
                if (fin_name) {
                    const char* mod_path = m->data.module_declaration.qualified_module_path;
                    fprintf(code_generator->output_stream, "    call #%s.%s;\n", mod_path, fin_name);
                    code_generator->total_emitted_instructions_count++;
                }
            }
        }

        /* Restaura o código de saída para r0 e finaliza o programa */
        fprintf(code_generator->output_stream, "    r0 := r1;\n");
        fprintf(code_generator->output_stream, "    free_frame;\n");
        fprintf(code_generator->output_stream, "    halt;\n");
        code_generator->total_emitted_instructions_count += 3;

        code_generator->current_program_name = NULL;
        code_generator->symbol_table->current_active_scope = previous_scope;
    }

    code_generator->symbol_table->active_compilation_unit = NULL;
    return true;
}

/* Emissão segura de arquivo *.ci em disco */
bool ijkl_code_generator_generate_to_file(IjklMemoryArena* memory_arena,
                                         IjklSymbolTable* symbol_table,
                                         const IjklAstNode* compilation_unit_root,
                                         const char* output_file_path,
                                         bool use_native_builtins) {
    if (!memory_arena || !symbol_table || !compilation_unit_root || !output_file_path) {
        return false;
    }

    FILE* file_handle = NULL;
    const int max_open_attempts = 5;

    for (int attempt_index = 0; attempt_index < max_open_attempts; attempt_index++) {
#if defined(_WIN32) || defined(_WIN64)
        /* Remove eventual atributo somente-leitura em arquivos pre-existentes */
        SetFileAttributesA(output_file_path, FILE_ATTRIBUTE_NORMAL);
#endif
        file_handle = fopen(output_file_path, "w");
        if (file_handle) {
            break;
        }

        /* Se falhar (por exemplo, lock transitorio de antivirus ou exclusao pendente), aguarda brevemente */
        if (attempt_index + 1 < max_open_attempts) {
#if defined(_WIN32) || defined(_WIN64)
            Sleep(50);
#else
            usleep(50000);
#endif
        }
    }

    if (!file_handle) {
#if defined(_WIN32) || defined(_WIN64)
        DWORD last_windows_error = GetLastError();
        fprintf(stderr, "[Error] Could not open output file '%s' for writing: %s (errno %d, Win32 error %lu).\n",
                output_file_path, strerror(errno), errno, (unsigned long)last_windows_error);
#else
        fprintf(stderr, "[Error] Could not open output file '%s' for writing: %s (errno %d).\n",
                output_file_path, strerror(errno), errno);
#endif
        return false;
    }

    IjklCodeGenerator* generator = ijkl_code_generator_create(memory_arena, symbol_table, file_handle);
    generator->use_native_builtins = use_native_builtins;
    bool generation_status = ijkl_code_generator_generate(generator, compilation_unit_root);

    fclose(file_handle);
    return generation_status;
}
