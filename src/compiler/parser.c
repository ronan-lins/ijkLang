#include "parser.h"

/*
 * ==============================================================================
 * MÓDULO: ANALISADOR SINTÁTICO (PARSER) DA IJKLANG
 * ==============================================================================
 * Implementa o analisador sintático descendente recursivo em ISO C11.
 * Constrói a Árvore Sintática Abstrata (AST) a partir do fluxo de tokens,
 * utilizando um buffer de lookahead dinâmico para desambiguação preditiva.
 *
 * Siglas técnicas empregadas:
 * - AST: Abstract Syntax Tree (Árvore Sintática Abstrata).
 * ==============================================================================
 */

#define IJKL_PARSER_LOOKAHEAD_CAPACITY 64

struct IjklParser {
    IjklLexer* lexer_instance;
    IjklMemoryArena* memory_arena;
    const char* source_file_path;

    IjklToken lookahead_buffer[IJKL_PARSER_LOOKAHEAD_CAPACITY];
    size_t lookahead_count;

    size_t error_count;
    bool is_panic_mode;
};

/* --- Gerenciamento do Buffer de Lookahead e Sincronização --- */

static void parser_fill_lookahead(IjklParser* parser, size_t needed_offset) {
    while (parser->lookahead_count <= needed_offset) {
        if (parser->lookahead_count >= IJKL_PARSER_LOOKAHEAD_CAPACITY) {
            return;
        }
        IjklToken scanned_token = ijkl_lexer_next_token(parser->lexer_instance);
        parser->lookahead_buffer[parser->lookahead_count++] = scanned_token;
        if (scanned_token.type == TOKEN_EOF) {
            break;
        }
    }
}

static IjklToken parser_peek_token(IjklParser* parser, size_t offset_from_current) {
    parser_fill_lookahead(parser, offset_from_current);
    if (offset_from_current < parser->lookahead_count) {
        return parser->lookahead_buffer[offset_from_current];
    }
    IjklToken end_of_file_token;
    memset(&end_of_file_token, 0, sizeof(IjklToken));
    end_of_file_token.type = TOKEN_EOF;
    return end_of_file_token;
}

static IjklToken parser_advance_token(IjklParser* parser) {
    IjklToken current_token = parser_peek_token(parser, 0);
    if (parser->lookahead_count > 0) {
        /* Desloca a fila de lookahead consumindo o primeiro token */
        for (size_t index = 0; index + 1 < parser->lookahead_count; index++) {
            parser->lookahead_buffer[index] = parser->lookahead_buffer[index + 1];
        }
        parser->lookahead_count--;
    }
    return current_token;
}

static bool parser_check_type(IjklParser* parser, IjklTokenType expected_token_type) {
    return parser_peek_token(parser, 0).type == expected_token_type;
}

static bool parser_match_type(IjklParser* parser, IjklTokenType expected_token_type) {
    if (parser_check_type(parser, expected_token_type)) {
        parser_advance_token(parser);
        return true;
    }
    return false;
}

static bool is_token_name_or_keyword(IjklTokenType token_type) {
    return token_type == TOKEN_IDENTIFIER ||
           (token_type >= TOKEN_KW_IF && token_type <= TOKEN_KW_ASM);
}

static void parser_report_error(IjklParser* parser, const char* error_message) {
    if (parser->is_panic_mode) return;
    parser->is_panic_mode = true;
    parser->error_count++;

    IjklToken current_token = parser_peek_token(parser, 0);
    const char* source_file = parser->source_file_path ? parser->source_file_path : "<source>";

    /* Diagnóstico 1: Delimitadores ou palavras obsoletas (Requisito 1) */
    char obsolete_information[256];
    if (current_token.lexeme && ijkl_vocabulary_is_obsolete_keyword(current_token.lexeme, obsolete_information, sizeof(obsolete_information))) {
        fprintf(stderr, "%s:%zu:%zu: Syntax error: %s\n        Dica: %s\n",
                source_file,
                current_token.line_number,
                current_token.column_number,
                error_message,
                obsolete_information);
        return;
    }

    fprintf(stderr, "%s:%zu:%zu: Syntax error: %s (got '%s')\n",
            source_file,
            current_token.line_number,
            current_token.column_number,
            error_message,
            current_token.lexeme ? current_token.lexeme : "");
}

static void parser_synchronize(IjklParser* parser);

static IjklToken parser_consume_token(IjklParser* parser, IjklTokenType expected_token_type, const char* error_message) {
    if (parser_check_type(parser, expected_token_type)) {
        return parser_advance_token(parser);
    }
    parser_report_error(parser, error_message);
    if (expected_token_type == TOKEN_SEMICOLON) {
        parser_synchronize(parser);
    }
    IjklToken fallback_token = parser_peek_token(parser, 0);
    return fallback_token;
}

/* Sincronização em modo pânico: avança até ponto-e-vírgula ou início de nova estrutura de controle */
static void parser_synchronize(IjklParser* parser) {
    parser->is_panic_mode = false;
    parser_advance_token(parser);

    while (!parser_check_type(parser, TOKEN_EOF)) {
        if (parser_peek_token(parser, 0).type == TOKEN_SEMICOLON) {
            parser_advance_token(parser);
            return;
        }

        switch (parser_peek_token(parser, 0).type) {
            case TOKEN_KW_IF:
            case TOKEN_KW_WHILE:
            case TOKEN_KW_FOR:
            case TOKEN_KW_REPEAT:
            case TOKEN_KW_SWITCH:
            case TOKEN_KW_RETURN:
            case TOKEN_KW_CONST:
            case TOKEN_KW_TYPE:
            case TOKEN_KW_ASM:
            case TOKEN_RBRACE:
                return;
            default:
                break;
        }
        parser_advance_token(parser);
    }
}

/* --- Verificadores Sintáticos com Lookahead --- */

static bool is_type_start(IjklParser* parser, size_t offset) {
    IjklTokenType current_type = parser_peek_token(parser, offset).type;
    switch (current_type) {
        case TOKEN_KW_INT:
        case TOKEN_KW_REAL:
        case TOKEN_KW_FLOAT:
        case TOKEN_KW_CHAR:
        case TOKEN_KW_STRING:
        case TOKEN_KW_BOOL:
        case TOKEN_KW_ARRAY:
        case TOKEN_KW_STRUCT:
        case TOKEN_KW_POINTER:
            return true;
        default:
            return false;
    }
}

/*
 * Desambiguação de rotinas:
 * Verifica se 'identificador(params)' é uma declaração de função/procedimento
 * ou uma chamada/comando solto, inspecionando o token após o ')' correspondente.
 * Se for '{' ou ':', trata-se de declaração de rotina.
 */
static bool is_routine_declaration(IjklParser* parser) {
    size_t start_offset = 0;
    bool has_explicit_visibility = false;
    if (parser_peek_token(parser, 0).type == TOKEN_KW_PUBLIC ||
        parser_peek_token(parser, 0).type == TOKEN_KW_PRIVATE) {
        start_offset = 1;
        has_explicit_visibility = true;
    }

    IjklTokenType first_type = parser_peek_token(parser, start_offset).type;
    bool is_valid_name = false;
    if (has_explicit_visibility) {
        /* Com modificador de visibilidade explícito, qualquer identificador ou palavra reservada contextual é válida como nome de rotina */
        is_valid_name = is_token_name_or_keyword(first_type);
    } else {
        /* Sem visibilidade explícita, não pode colidir com comandos de controle de fluxo que aceitam '(condição) {' */
        if (first_type == TOKEN_IDENTIFIER ||
            first_type == TOKEN_KW_MAIN ||
            first_type == TOKEN_KW_INITIALIZE ||
            first_type == TOKEN_KW_FINALIZE ||
            first_type == TOKEN_KW_WRITE ||
            first_type == TOKEN_KW_WRITELN ||
            first_type == TOKEN_KW_READ) {
            is_valid_name = true;
        }
    }
    if (!is_valid_name) return false;
    if (parser_peek_token(parser, start_offset + 1).type != TOKEN_LPAREN) return false;

    size_t lookahead_offset = start_offset + 2;
    int parenthesis_nesting_depth = 1;
    while (parenthesis_nesting_depth > 0) {
        IjklTokenType token_type = parser_peek_token(parser, lookahead_offset).type;
        if (token_type == TOKEN_EOF || token_type == TOKEN_SEMICOLON) return false;
        if (token_type == TOKEN_LPAREN) parenthesis_nesting_depth++;
        else if (token_type == TOKEN_RPAREN) parenthesis_nesting_depth--;
        lookahead_offset++;
    }

    IjklTokenType token_following_rparen = parser_peek_token(parser, lookahead_offset).type;
    return (token_following_rparen == TOKEN_LBRACE || token_following_rparen == TOKEN_COLON);
}

/* Desambiguação de tipos definidos pelo usuário: ex.: 'Aluno estudante;' */
static bool is_user_type_variable_declaration(IjklParser* parser) {
    if (parser_peek_token(parser, 0).type != TOKEN_IDENTIFIER) return false;
    if (parser_peek_token(parser, 1).type == TOKEN_IDENTIFIER) return true;
    return false;
}

/* --- Parsing de Especificadores de Tipo --- */

static IjklTypeSpec* parse_type_specification_internal(IjklParser* parser, bool allow_generic_pointer_shorthand) {
    IjklToken current_token = parser_peek_token(parser, 0);

    if (current_token.type == TOKEN_KW_INT) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_INT;
        return created_specification;
    }
    if (current_token.type == TOKEN_KW_REAL) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_REAL;
        return created_specification;
    }
    if (current_token.type == TOKEN_KW_FLOAT) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_FLOAT;
        return created_specification;
    }
    if (current_token.type == TOKEN_KW_CHAR) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_CHAR;
        return created_specification;
    }
    if (current_token.type == TOKEN_KW_STRING) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_STRING;
        return created_specification;
    }
    if (current_token.type == TOKEN_KW_BOOL) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_PRIMITIVE, current_token.line_number, current_token.column_number);
        created_specification->details.primitive_type = PRIMITIVE_BOOL;
        return created_specification;
    }

    /* Arranjo de dimensão fixa ou aberta: arranjo[N] [de] tipo ou arranjo[] [de] tipo */
    if (current_token.type == TOKEN_KW_ARRAY) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LBRACKET, "Expected '[' after 'arranjo' / 'array'.");
        size_t fixed_element_count = 0;
        if (!parser_match_type(parser, TOKEN_RBRACKET)) {
            IjklToken size_token = parser_consume_token(parser, TOKEN_INTEGER, "Expected integer array size or ']' for open dimension array.");
            parser_consume_token(parser, TOKEN_RBRACKET, "Expected ']' after array size.");
            fixed_element_count = (size_t)size_token.value.integer_value;
        }

        /* Consome 'de' ou 'of' se presente */
        if (parser_check_type(parser, TOKEN_KW_OF) || parser_check_type(parser, TOKEN_KW_FROM)) {
            parser_advance_token(parser);
        }

        IjklTypeSpec* element_type_specification = parse_type_specification_internal(parser, false);

        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_ARRAY, current_token.line_number, current_token.column_number);
        created_specification->details.array_info.element_type = element_type_specification;
        created_specification->details.array_info.fixed_element_count = fixed_element_count;
        return created_specification;
    }

    /*
     * Desambiguação de ponteiros:
     * - Ponteiro tipado: 'ponteiro Node next;' (dois identificadores sequenciais).
     * - Ponteiro genérico: 'ponteiro p = nulo;' (apenas um identificador, que é o nome da variável).
     */
    if (current_token.type == TOKEN_KW_POINTER) {
        parser_advance_token(parser);
        IjklTypeSpec* base_type_specification = NULL;

        if (is_type_start(parser, 0)) {
            base_type_specification = parse_type_specification_internal(parser, false);
        } else if (parser_check_type(parser, TOKEN_IDENTIFIER)) {
            if (!allow_generic_pointer_shorthand || parser_peek_token(parser, 1).type == TOKEN_IDENTIFIER) {
                base_type_specification = parse_type_specification_internal(parser, false);
            }
        }

        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_POINTER, current_token.line_number, current_token.column_number);
        created_specification->details.pointer_info.base_type = base_type_specification;
        return created_specification;
    }

    /* Registro heterogêneo estruturado: registro { campos ; } */
    if (current_token.type == TOKEN_KW_STRUCT) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' to start struct body.");

        IjklAstNode* first_field_node = NULL;
        IjklAstNode* last_field_node = NULL;

        while (!parser_check_type(parser, TOKEN_RBRACE) && !parser_check_type(parser, TOKEN_EOF)) {
            IjklTypeSpec* field_type_specification = parse_type_specification_internal(parser, true);
            IjklToken field_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected field name.");

            IjklAstNode* field_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_PARAMETER, field_name_token.line_number, field_name_token.column_number);
            field_node->data.parameter.parameter_name = field_name_token.lexeme;
            field_node->data.parameter.type_specification = field_type_specification;

            if (!first_field_node) first_field_node = field_node;
            else last_field_node->next_sibling = field_node;
            last_field_node = field_node;

            /* Suporte a múltiplos campos do mesmo tipo separados por vírgula */
            while (parser_match_type(parser, TOKEN_COMMA)) {
                IjklToken extra_field_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected field name after comma.");
                IjklAstNode* extra_field_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_PARAMETER, extra_field_name_token.line_number, extra_field_name_token.column_number);
                extra_field_node->data.parameter.parameter_name = extra_field_name_token.lexeme;
                extra_field_node->data.parameter.type_specification = field_type_specification;
                last_field_node->next_sibling = extra_field_node;
                last_field_node = extra_field_node;
            }

            parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after struct field declaration.");
        }

        parser_consume_token(parser, TOKEN_RBRACE, "Expected '}' to close struct body.");

        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_STRUCT, current_token.line_number, current_token.column_number);
        created_specification->details.struct_info.field_declarations = first_field_node;
        return created_specification;
    }

    /* Tipo definido pelo usuário */
    if (current_token.type == TOKEN_IDENTIFIER) {
        parser_advance_token(parser);
        IjklTypeSpec* created_specification = ijkl_ast_type_spec_create(parser->memory_arena, TYPE_KIND_USER_DEFINED, current_token.line_number, current_token.column_number);
        created_specification->details.user_type_name = current_token.lexeme;
        return created_specification;
    }

    parser_report_error(parser, "Expected type specification.");
    return NULL;
}

static IjklTypeSpec* parse_type_specification(IjklParser* parser) {
    return parse_type_specification_internal(parser, true);
}

/* --- Parsing de Expressões por Precedência de Operadores --- */

static IjklAstNode* parse_expression(IjklParser* parser);

static IjklAstNode* parse_primary_expression(IjklParser* parser) {
    IjklToken current_token = parser_peek_token(parser, 0);

    if (current_token.type == TOKEN_INTEGER) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_INT, current_token.line_number, current_token.column_number);
        literal_node->data.literal_integer.integer_value = current_token.value.integer_value;
        return literal_node;
    }
    if (current_token.type == TOKEN_REAL) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_REAL, current_token.line_number, current_token.column_number);
        literal_node->data.literal_real.real_value = current_token.value.real_value;
        return literal_node;
    }
    if (current_token.type == TOKEN_FLOAT) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_FLOAT, current_token.line_number, current_token.column_number);
        literal_node->data.literal_float.float_value = current_token.value.float_value;
        return literal_node;
    }
    if (current_token.type == TOKEN_CHAR) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_CHAR, current_token.line_number, current_token.column_number);
        literal_node->data.literal_character.char_value = current_token.value.char_value;
        return literal_node;
    }
    if (current_token.type == TOKEN_STRING) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_STRING, current_token.line_number, current_token.column_number);
        literal_node->data.literal_string.string_value = current_token.value.string_value;
        return literal_node;
    }
    if (current_token.type == TOKEN_KW_TRUE || current_token.type == TOKEN_KW_FALSE) {
        parser_advance_token(parser);
        IjklAstNode* literal_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_BOOL, current_token.line_number, current_token.column_number);
        literal_node->data.literal_boolean.boolean_value = (current_token.type == TOKEN_KW_TRUE);
        return literal_node;
    }
    if (current_token.type == TOKEN_KW_NULL) {
        parser_advance_token(parser);
        return ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_LITERAL_NULL, current_token.line_number, current_token.column_number);
    }

    /* Alocação dinâmica: reserve(expressao_bytes) / alloc(expressao_bytes) */
    if (current_token.type == TOKEN_KW_ALLOC) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'reserve' / 'alloc'.");
        IjklAstNode* byte_count_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after alloc expression.");

        IjklAstNode* allocation_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_ALLOC, current_token.line_number, current_token.column_number);
        allocation_node->data.allocation_expression.byte_count_expression = byte_count_expression;
        return allocation_node;
    }

    if (current_token.type == TOKEN_IDENTIFIER ||
        current_token.type == TOKEN_KW_MAIN ||
        current_token.type == TOKEN_KW_INITIALIZE ||
        current_token.type == TOKEN_KW_FINALIZE ||
        (is_token_name_or_keyword(current_token.type) && parser_peek_token(parser, 1).type == TOKEN_DOT)) {
        parser_advance_token(parser);
        IjklAstNode* identifier_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_IDENTIFIER, current_token.line_number, current_token.column_number);
        identifier_node->data.identifier_expression.identifier_name = current_token.lexeme;
        return identifier_node;
    }

    if (current_token.type == TOKEN_LPAREN) {
        parser_advance_token(parser);
        IjklAstNode* enclosed_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after enclosed expression.");
        return enclosed_expression;
    }

    parser_report_error(parser, "Expected expression literal, identifier, or '(' expression.");
    return NULL;
}

/*
 * Regra de Precedência de Memória:
 * O operador unário '@' (desreferenciação) possui precedência estritamente superior
 * ao operador '.' (membro) e '[ ]' (indexação). Desta forma, '@p.campo' é parsed
 * formalmente como '(@p).campo'.
 */
static IjklAstNode* parse_postfix_and_deref_expression(IjklParser* parser) {
    IjklAstNode* left_operand = NULL;

    if (parser_check_type(parser, TOKEN_AT)) {
        IjklToken dereference_token = parser_advance_token(parser);
        IjklAstNode* operand_expression = NULL;
        if (parser_check_type(parser, TOKEN_AT)) {
            operand_expression = parse_postfix_and_deref_expression(parser);
        } else {
            operand_expression = parse_primary_expression(parser);
        }
        IjklAstNode* dereference_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_DEREFERENCE, dereference_token.line_number, dereference_token.column_number);
        dereference_node->data.dereference_expression.operand = operand_expression;
        left_operand = dereference_node;
    } else {
        left_operand = parse_primary_expression(parser);
    }

    while (true) {
        if (parser_match_type(parser, TOKEN_DOT)) {
            IjklToken member_name_token;
            if (is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
                member_name_token = parser_advance_token(parser);
            } else {
                member_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected member name after '.'.");
            }
            IjklAstNode* member_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_MEMBER_ACCESS, member_name_token.line_number, member_name_token.column_number);
            member_node->data.member_access_expression.target_expression = left_operand;
            member_node->data.member_access_expression.member_name = member_name_token.lexeme;
            left_operand = member_node;
        } else if (parser_match_type(parser, TOKEN_LBRACKET)) {
            IjklToken bracket_token = parser_peek_token(parser, 0);
            IjklAstNode* index_expression = parse_expression(parser);
            parser_consume_token(parser, TOKEN_RBRACKET, "Expected ']' after index expression.");
            IjklAstNode* index_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_INDEX_ACCESS, bracket_token.line_number, bracket_token.column_number);
            index_node->data.index_access_expression.target_expression = left_operand;
            index_node->data.index_access_expression.index_expression = index_expression;
            left_operand = index_node;
        } else if (parser_match_type(parser, TOKEN_LPAREN)) {
            IjklToken paren_token = parser_peek_token(parser, 0);
            IjklAstNode* first_argument_node = NULL;
            IjklAstNode* last_argument_node = NULL;

            if (!parser_check_type(parser, TOKEN_RPAREN)) {
                do {
                    IjklAstNode* argument_expression = parse_expression(parser);
                    if (!first_argument_node) first_argument_node = argument_expression;
                    else last_argument_node->next_sibling = argument_expression;
                    last_argument_node = argument_expression;
                } while (parser_match_type(parser, TOKEN_COMMA));
            }

            parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after argument list.");
            IjklAstNode* call_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_CALL, paren_token.line_number, paren_token.column_number);
            call_node->data.call_expression.callable_expression = left_operand;
            call_node->data.call_expression.argument_expressions = first_argument_node;
            left_operand = call_node;
        } else {
            break;
        }
    }

    return left_operand;
}

/* Nível 2: Operadores Unários (+, -, !, &, $) */
static IjklAstNode* parse_unary_expression(IjklParser* parser) {
    if (parser_check_type(parser, TOKEN_MINUS) ||
        parser_check_type(parser, TOKEN_PLUS) ||
        parser_check_type(parser, TOKEN_EXCLAMATION) ||
        parser_check_type(parser, TOKEN_KW_NOT) ||
        parser_check_type(parser, TOKEN_AMPERSAND) ||
        parser_check_type(parser, TOKEN_DOLLAR)) {

        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* operand_expression = NULL;

        /* O operador de custo '$' aceita tanto identificador de tipo quanto expressão de variável */
        if (operator_token.type == TOKEN_DOLLAR && is_type_start(parser, 0)) {
            IjklToken type_keyword_token = parser_advance_token(parser);
            operand_expression = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_IDENTIFIER, type_keyword_token.line_number, type_keyword_token.column_number);
            operand_expression->data.identifier_expression.identifier_name = type_keyword_token.lexeme;
        } else {
            operand_expression = parse_unary_expression(parser);
        }

        IjklAstNode* unary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_UNARY, operator_token.line_number, operator_token.column_number);
        unary_node->data.unary_expression.operator_type = operator_token.type;
        unary_node->data.unary_expression.operand = operand_expression;
        return unary_node;
    }

    return parse_postfix_and_deref_expression(parser);
}

/* Nível 3: Potenciação (^) */
static IjklAstNode* parse_power_expression(IjklParser* parser) {
    IjklAstNode* base_operand = parse_unary_expression(parser);

    if (parser_check_type(parser, TOKEN_CARET)) {
        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* exponent_operand = parse_power_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = base_operand;
        binary_node->data.binary_expression.right_operand = exponent_operand;
        return binary_node;
    }

    return base_operand;
}

/* Nível 4: Multiplicação, Divisão e Resto (*, /, div, mod) */
static IjklAstNode* parse_multiplicative_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_power_expression(parser);

    while (parser_check_type(parser, TOKEN_STAR) ||
           parser_check_type(parser, TOKEN_SLASH) ||
           parser_check_type(parser, TOKEN_KW_DIV) ||
           parser_check_type(parser, TOKEN_KW_MOD)) {

        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_power_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

/* Nível 5: Adição e Subtração (+, -) */
static IjklAstNode* parse_additive_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_multiplicative_expression(parser);

    while (parser_check_type(parser, TOKEN_PLUS) || parser_check_type(parser, TOKEN_MINUS)) {
        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_multiplicative_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

/* Nível 6: Relacionais (<, <=, >, >=) */
static IjklAstNode* parse_relational_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_additive_expression(parser);

    while (parser_check_type(parser, TOKEN_LESS) ||
           parser_check_type(parser, TOKEN_LESS_EQUAL) ||
           parser_check_type(parser, TOKEN_GREATER) ||
           parser_check_type(parser, TOKEN_GREATER_EQUAL)) {

        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_additive_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

/* Nível 7: Igualdade e Diferença (=, ==, !=, <>) */
static IjklAstNode* parse_equality_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_relational_expression(parser);

    while (parser_check_type(parser, TOKEN_EQUAL) ||
           parser_check_type(parser, TOKEN_EQUAL_STRICT) ||
           parser_check_type(parser, TOKEN_NOT_EQUAL) ||
           parser_check_type(parser, TOKEN_NOT_EQUAL_PASCAL)) {

        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_relational_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

/* Nível 8: Lógico E (e, &&) */
static IjklAstNode* parse_logical_and_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_equality_expression(parser);

    while (parser_check_type(parser, TOKEN_KW_AND) || parser_check_type(parser, TOKEN_LOGICAL_AND)) {
        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_equality_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

/* Nível 9: Lógico OU (ou, ||) */
static IjklAstNode* parse_logical_or_expression(IjklParser* parser) {
    IjklAstNode* left_operand = parse_logical_and_expression(parser);

    while (parser_check_type(parser, TOKEN_KW_OR) || parser_check_type(parser, TOKEN_LOGICAL_OR)) {
        IjklToken operator_token = parser_advance_token(parser);
        IjklAstNode* right_operand = parse_logical_and_expression(parser);
        IjklAstNode* binary_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_EXPR_BINARY, operator_token.line_number, operator_token.column_number);
        binary_node->data.binary_expression.operator_type = operator_token.type;
        binary_node->data.binary_expression.left_operand = left_operand;
        binary_node->data.binary_expression.right_operand = right_operand;
        left_operand = binary_node;
    }

    return left_operand;
}

static IjklAstNode* parse_expression(IjklParser* parser) {
    return parse_logical_or_expression(parser);
}

/* --- Parsing de Comandos e Blocos --- */

static IjklAstNode* parse_statement(IjklParser* parser);

static IjklAstNode* parse_block_statement(IjklParser* parser) {
    IjklToken lbrace_token = parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' to start statement block.");
    IjklAstNode* first_statement_node = NULL;
    IjklAstNode* last_statement_node = NULL;

    while (!parser_check_type(parser, TOKEN_RBRACE) && !parser_check_type(parser, TOKEN_EOF)) {
        IjklToken token_before = parser_peek_token(parser, 0);
        IjklAstNode* current_statement = parse_statement(parser);
        if (current_statement) {
            if (!first_statement_node) first_statement_node = current_statement;
            else last_statement_node->next_sibling = current_statement;
            last_statement_node = current_statement;
        }
        IjklToken token_after = parser_peek_token(parser, 0);
        if (token_before.line_number == token_after.line_number &&
            token_before.column_number == token_after.column_number &&
            !parser_check_type(parser, TOKEN_EOF)) {
            parser_synchronize(parser);
        }
    }

    parser_consume_token(parser, TOKEN_RBRACE, "Expected '}' to close statement block.");

    IjklAstNode* block_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_BLOCK, lbrace_token.line_number, lbrace_token.column_number);
    block_node->data.block_statement.statements = first_statement_node;
    return block_node;
}

static IjklAstNode* parse_obligatory_statement(IjklParser* parser) {
    if (parser_check_type(parser, TOKEN_LBRACE)) {
        return parse_block_statement(parser);
    }
    return parse_statement(parser);
}

static IjklAstNode* parse_statement(IjklParser* parser) {
    IjklToken current_token = parser_peek_token(parser, 0);

    /* Diagnóstico antecipado de palavras obsoletas no início de instruções */
    if (current_token.type == TOKEN_IDENTIFIER && current_token.lexeme) {
        char obsolete_information[256];
        if (ijkl_vocabulary_is_obsolete_keyword(current_token.lexeme, obsolete_information, sizeof(obsolete_information))) {
            parser_report_error(parser, "Obsolete keyword detected.");
            parser_synchronize(parser);
            return NULL;
        }
    }

    if (current_token.type == TOKEN_LBRACE) {
        return parse_block_statement(parser);
    }

    if (current_token.type == TOKEN_KW_IF) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'se' / 'if'.");
        IjklAstNode* condition_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after condition.");
        IjklAstNode* then_branch_statement = parse_obligatory_statement(parser);
        IjklAstNode* else_branch_statement = NULL;

        if (parser_match_type(parser, TOKEN_KW_ELSE)) {
            else_branch_statement = parse_obligatory_statement(parser);
        }

        IjklAstNode* if_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_IF, current_token.line_number, current_token.column_number);
        if_node->data.if_statement.condition_expression = condition_expression;
        if_node->data.if_statement.then_branch_statement = then_branch_statement;
        if_node->data.if_statement.else_branch_statement = else_branch_statement;
        return if_node;
    }

    if (current_token.type == TOKEN_KW_SWITCH) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'escolha' / 'switch'.");
        IjklAstNode* switch_target_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after switch target expression.");
        parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' to start switch body.");

        IjklAstNode* first_case_node = NULL;
        IjklAstNode* last_case_node = NULL;
        IjklAstNode* default_branch_node = NULL;

        while (!parser_check_type(parser, TOKEN_RBRACE) && !parser_check_type(parser, TOKEN_EOF)) {
            if (parser_match_type(parser, TOKEN_KW_CASE)) {
                IjklToken case_token = parser_peek_token(parser, 0);
                IjklAstNode* match_expression = parse_expression(parser);
                parser_consume_token(parser, TOKEN_COLON, "Expected ':' after case expression.");

                IjklAstNode* first_case_statement = NULL;
                IjklAstNode* last_case_statement = NULL;

                while (!parser_check_type(parser, TOKEN_KW_CASE) &&
                       !parser_check_type(parser, TOKEN_KW_DEFAULT) &&
                       !parser_check_type(parser, TOKEN_RBRACE) &&
                       !parser_check_type(parser, TOKEN_EOF)) {
                    IjklAstNode* case_statement = parse_statement(parser);
                    if (case_statement) {
                        if (!first_case_statement) first_case_statement = case_statement;
                        else last_case_statement->next_sibling = case_statement;
                        last_case_statement = case_statement;
                    }
                }

                IjklAstNode* case_branch_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_CASE_BRANCH, case_token.line_number, case_token.column_number);
                case_branch_node->data.case_branch.match_expression = match_expression;
                case_branch_node->data.case_branch.statements = first_case_statement;

                if (!first_case_node) first_case_node = case_branch_node;
                else last_case_node->next_sibling = case_branch_node;
                last_case_node = case_branch_node;
            } else if (parser_match_type(parser, TOKEN_KW_DEFAULT)) {
                parser_consume_token(parser, TOKEN_COLON, "Expected ':' after default / padrao.");
                IjklAstNode* first_default_statement = NULL;
                IjklAstNode* last_default_statement = NULL;

                while (!parser_check_type(parser, TOKEN_KW_CASE) &&
                       !parser_check_type(parser, TOKEN_RBRACE) &&
                       !parser_check_type(parser, TOKEN_EOF)) {
                    IjklAstNode* default_statement = parse_statement(parser);
                    if (default_statement) {
                        if (!first_default_statement) first_default_statement = default_statement;
                        else last_default_statement->next_sibling = default_statement;
                        last_default_statement = default_statement;
                    }
                }
                default_branch_node = first_default_statement;
            } else {
                parser_report_error(parser, "Expected 'caso' or 'padrao' in switch body.");
                parser_advance_token(parser);
            }
        }

        parser_consume_token(parser, TOKEN_RBRACE, "Expected '}' to close switch body.");

        IjklAstNode* switch_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_SWITCH, current_token.line_number, current_token.column_number);
        switch_node->data.switch_statement.condition_expression = switch_target_expression;
        switch_node->data.switch_statement.case_branches = first_case_node;
        switch_node->data.switch_statement.default_branch = default_branch_node;
        return switch_node;
    }

    if (current_token.type == TOKEN_KW_WHILE) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'enquanto' / 'while'.");
        IjklAstNode* condition_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after while condition.");
        IjklAstNode* body_statement = parse_obligatory_statement(parser);

        IjklAstNode* while_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_WHILE, current_token.line_number, current_token.column_number);
        while_node->data.while_statement.condition_expression = condition_expression;
        while_node->data.while_statement.body_statement = body_statement;
        return while_node;
    }

    if (current_token.type == TOKEN_KW_FOR) {
        parser_advance_token(parser);

        bool has_header_parentheses = parser_match_type(parser, TOKEN_LPAREN);

        IjklTypeSpec* control_variable_type = NULL;
        if (is_type_start(parser, 0)) {
            control_variable_type = parse_type_specification(parser);
        }

        IjklToken control_variable_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected loop variable name.");

        /* Aceita 'de' / 'from', 'of', '=' ou ':=' como operador de inicialização */
        if (!parser_match_type(parser, TOKEN_KW_FROM) &&
            !parser_match_type(parser, TOKEN_KW_OF) &&
            !parser_match_type(parser, TOKEN_EQUAL) &&
            !parser_match_type(parser, TOKEN_ASSIGN_PASCAL)) {
            parser_report_error(parser, "Expected 'de', 'from', '=', or ':=' in for loop.");
        }

        IjklAstNode* start_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_KW_TO, "Expected 'ate' / 'to' in for loop.");
        IjklAstNode* end_expression = parse_expression(parser);

        IjklAstNode* step_expression = NULL;
        if (parser_match_type(parser, TOKEN_KW_STEP)) {
            step_expression = parse_expression(parser);
        }

        if (has_header_parentheses) {
            parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after for loop header.");
        }

        IjklAstNode* body_statement = parse_obligatory_statement(parser);

        IjklAstNode* for_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_FOR, current_token.line_number, current_token.column_number);
        for_node->data.for_statement.control_variable_name = control_variable_token.lexeme;
        for_node->data.for_statement.control_variable_type = control_variable_type;
        for_node->data.for_statement.start_expression = start_expression;
        for_node->data.for_statement.end_expression = end_expression;
        for_node->data.for_statement.step_expression = step_expression;
        for_node->data.for_statement.body_statement = body_statement;
        return for_node;
    }

    if (current_token.type == TOKEN_KW_REPEAT) {
        parser_advance_token(parser);
        IjklAstNode* repeat_body_block = parse_block_statement(parser);
        parser_consume_token(parser, TOKEN_KW_WHILE, "Expected 'enquanto' / 'while' after repeat block.");
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'enquanto'.");
        IjklAstNode* condition_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after repeat condition.");
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after repeat statement.");

        IjklAstNode* repeat_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_REPEAT, current_token.line_number, current_token.column_number);
        repeat_node->data.repeat_statement.body_block = repeat_body_block;
        repeat_node->data.repeat_statement.condition_expression = condition_expression;
        return repeat_node;
    }

    if (current_token.type == TOKEN_KW_BREAK) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after 'aborte' / 'break'.");
        return ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_BREAK, current_token.line_number, current_token.column_number);
    }

    if (current_token.type == TOKEN_KW_RETURN) {
        parser_advance_token(parser);
        IjklAstNode* return_value_expression = NULL;
        if (!parser_check_type(parser, TOKEN_SEMICOLON)) {
            return_value_expression = parse_expression(parser);
        }
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after 'retorne' / 'return'.");

        IjklAstNode* return_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_RETURN, current_token.line_number, current_token.column_number);
        return_node->data.return_statement.return_value_expression = return_value_expression;
        return return_node;
    }

    if (current_token.type == TOKEN_KW_READ) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'leia' / 'read'.");

        IjklAstNode* first_target_lvalue = NULL;
        IjklAstNode* last_target_lvalue = NULL;
        do {
            IjklAstNode* target_lvalue = parse_expression(parser);
            if (!first_target_lvalue) first_target_lvalue = target_lvalue;
            else last_target_lvalue->next_sibling = target_lvalue;
            last_target_lvalue = target_lvalue;
        } while (parser_match_type(parser, TOKEN_COMMA));

        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after read arguments.");
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after read statement.");

        IjklAstNode* read_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_READ, current_token.line_number, current_token.column_number);
        read_node->data.read_statement.target_lvalues = first_target_lvalue;
        return read_node;
    }

    if (current_token.type == TOKEN_KW_WRITE || current_token.type == TOKEN_KW_WRITELN) {
        parser_advance_token(parser);
        /* Identifica se é escrita com salto de linha ('ln' = line-feed) */
        bool is_newline_required = (current_token.type == TOKEN_KW_WRITELN);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after write statement.");

        IjklAstNode* first_argument_expression = NULL;
        IjklAstNode* last_argument_expression = NULL;

        if (!parser_check_type(parser, TOKEN_RPAREN)) {
            do {
                IjklAstNode* argument_expression = parse_expression(parser);
                if (!first_argument_expression) first_argument_expression = argument_expression;
                else last_argument_expression->next_sibling = argument_expression;
                last_argument_expression = argument_expression;
            } while (parser_match_type(parser, TOKEN_COMMA));
        }

        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after write arguments.");
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after write statement.");

        IjklAstNode* write_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_WRITE, current_token.line_number, current_token.column_number);
        write_node->data.write_statement.is_newline = is_newline_required;
        write_node->data.write_statement.argument_expressions = first_argument_expression;
        return write_node;
    }

    if (current_token.type == TOKEN_KW_FREE) {
        parser_advance_token(parser);
        parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after 'libere' / 'free'.");
        IjklAstNode* target_lvalue = parse_expression(parser);
        parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after free argument.");
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after free statement.");

        IjklAstNode* free_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_FREE, current_token.line_number, current_token.column_number);
        free_node->data.free_statement.target_lvalue = target_lvalue;
        return free_node;
    }

    /* Comando de montagem embutida (inline assembly / CI) */
    if (current_token.type == TOKEN_KW_ASM) {
        parser_advance_token(parser);
        const char* target_builtin_name = NULL;
        if (parser_check_type(parser, TOKEN_IDENTIFIER)) {
            IjklToken identifier_token = parser_advance_token(parser);
            target_builtin_name = identifier_token.lexeme;
        }
        parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' after 'asm'.");
        size_t raw_ci_code_length = 0;
        char* raw_ci_code_text = ijkl_lexer_scan_raw_block(parser->lexer_instance, &raw_ci_code_length);
        parser->lookahead_count = 0; /* Invalida lookahead apos escaneamento direto do bloco no lexer */
        parser_match_type(parser, TOKEN_SEMICOLON); /* Ponto-e-virgula opcional apos '}' */

        IjklAstNode* asm_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_ASM, current_token.line_number, current_token.column_number);
        asm_node->data.asm_statement.target_builtin_name = target_builtin_name;
        asm_node->data.asm_statement.ci_code_text = raw_ci_code_text;
        asm_node->data.asm_statement.ci_code_length = raw_ci_code_length;
        return asm_node;
    }

    /* Declaração de variável local */
    if (is_type_start(parser, 0) || is_user_type_variable_declaration(parser)) {
        IjklTypeSpec* variable_type_specification = parse_type_specification(parser);
        IjklToken variable_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected variable name.");

        IjklAstNode* initial_value_expression = NULL;
        if (parser_match_type(parser, TOKEN_EQUAL) || parser_match_type(parser, TOKEN_ASSIGN_PASCAL)) {
            initial_value_expression = parse_expression(parser);
        }

        IjklAstNode* first_variable_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_DECL_VAR, variable_name_token.line_number, variable_name_token.column_number);
        first_variable_node->data.variable_declaration.variable_name = variable_name_token.lexeme;
        first_variable_node->data.variable_declaration.type_specification = variable_type_specification;
        first_variable_node->data.variable_declaration.initial_value_expression = initial_value_expression;
        IjklAstNode* last_variable_node = first_variable_node;

        while (parser_match_type(parser, TOKEN_COMMA)) {
            IjklToken extra_variable_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected variable name after comma.");
            IjklAstNode* extra_initial_value_expression = NULL;
            if (parser_match_type(parser, TOKEN_EQUAL) || parser_match_type(parser, TOKEN_ASSIGN_PASCAL)) {
                extra_initial_value_expression = parse_expression(parser);
            }
            IjklAstNode* extra_variable_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_DECL_VAR, extra_variable_name_token.line_number, extra_variable_name_token.column_number);
            extra_variable_node->data.variable_declaration.variable_name = extra_variable_name_token.lexeme;
            extra_variable_node->data.variable_declaration.type_specification = variable_type_specification;
            extra_variable_node->data.variable_declaration.initial_value_expression = extra_initial_value_expression;
            last_variable_node->next_sibling = extra_variable_node;
            last_variable_node = extra_variable_node;
        }

        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
        return first_variable_node;
    }

    /*
     * Atribuição ou chamada:
     * Avalia primeiro o l-value ou expressão invocável para determinar
     * se a instrução encerra em atribuição ('=' ou ':=') ou chamada com ';'.
     */
    IjklAstNode* expression_node = parse_postfix_and_deref_expression(parser);

    if (parser_check_type(parser, TOKEN_ASSIGN_PASCAL) || parser_check_type(parser, TOKEN_EQUAL)) {
        IjklToken assignment_operator_token = parser_advance_token(parser);
        bool is_pascal_assignment = (assignment_operator_token.type == TOKEN_ASSIGN_PASCAL);
        IjklAstNode* assigned_value_expression = parse_expression(parser);
        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after assignment.");

        IjklAstNode* assignment_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_ASSIGN, assignment_operator_token.line_number, assignment_operator_token.column_number);
        assignment_node->data.assignment_statement.target_lvalue = expression_node;
        assignment_node->data.assignment_statement.value_expression = assigned_value_expression;
        assignment_node->data.assignment_statement.is_pascal_assignment = is_pascal_assignment;
        return assignment_node;
    }

    parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after expression statement.");

    if (expression_node && expression_node->kind == AST_NODE_EXPR_CALL) {
        IjklAstNode* procedure_call_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_STMT_CALL, expression_node->line_number, expression_node->column_number);
        procedure_call_node->data.call_statement.callable_expression = expression_node->data.call_expression.callable_expression;
        procedure_call_node->data.call_statement.argument_expressions = expression_node->data.call_expression.argument_expressions;
        return procedure_call_node;
    }

    return expression_node;
}

/* --- Declarações de Nível Superior --- */

static IjklAstNode* parse_routine_declaration(IjklParser* parser) {
    bool is_public = true;
    bool has_explicit_visibility = false;

    if (parser_match_type(parser, TOKEN_KW_PUBLIC)) {
        is_public = true;
        has_explicit_visibility = true;
    } else if (parser_match_type(parser, TOKEN_KW_PRIVATE)) {
        is_public = false;
        has_explicit_visibility = true;
    }

    if (!has_explicit_visibility) {
        const IjklVocabulary* vocabulary = ijkl_lexer_get_vocabulary(parser->lexer_instance);
        const char* kw_public_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_PUBLIC);
        const char* kw_private_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_PRIVATE);
        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                 "Expected '%s' or '%s' before routine declaration.",
                 kw_public_str ? kw_public_str : "publico",
                 kw_private_str ? kw_private_str : "privado");
        parser_report_error(parser, error_buffer);
        is_public = false;
    }

    IjklToken routine_name_token;
    if (parser_check_type(parser, TOKEN_KW_MAIN) ||
        parser_check_type(parser, TOKEN_KW_INITIALIZE) ||
        parser_check_type(parser, TOKEN_KW_FINALIZE) ||
        parser_check_type(parser, TOKEN_IDENTIFIER) ||
        is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
        routine_name_token = parser_advance_token(parser);
    } else {
        routine_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected routine name.");
    }
    parser_consume_token(parser, TOKEN_LPAREN, "Expected '(' after routine name.");

    IjklAstNode* first_parameter_node = NULL;
    IjklAstNode* last_parameter_node = NULL;

    if (!parser_check_type(parser, TOKEN_RPAREN)) {
        do {
            const char* parameter_name = NULL;
            IjklTypeSpec* parameter_type_specification = NULL;
            size_t parameter_line = 0;
            size_t parameter_column = 0;
            bool is_by_reference = false;

            if (parser_match_type(parser, TOKEN_KW_REF)) {
                is_by_reference = true;
            }

            /*
             * Padronização Sintática Universal: [ref] <tipo> <identificador>
             * O delimitador ':' é reservado estrita e exclusivamente para o tipo de retorno de funções.
             * Caso a sintaxe legada 'identificador: tipo' seja detectada, reporta erro orientativo pedagógico.
             */
            if (parser_peek_token(parser, 0).type == TOKEN_IDENTIFIER &&
                parser_peek_token(parser, 1).type == TOKEN_COLON) {
                parser_report_error(parser, "Erro sintático: Parâmetros devem seguir o padrão '<tipo> <identificador>'. O delimitador ':' é reservado exclusivamente para o tipo de retorno de funções (ex.: 'funcao(inteiro valor): inteiro').");
                IjklToken parameter_name_token = parser_advance_token(parser);
                parameter_name = parameter_name_token.lexeme;
                parameter_line = parameter_name_token.line_number;
                parameter_column = parameter_name_token.column_number;
                parser_consume_token(parser, TOKEN_COLON, "Expected ':' after parameter name.");
                parameter_type_specification = parse_type_specification_internal(parser, false);
            } else {
                parameter_type_specification = parse_type_specification(parser);
                IjklToken parameter_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected parameter name.");
                parameter_name = parameter_name_token.lexeme;
                parameter_line = parameter_name_token.line_number;
                parameter_column = parameter_name_token.column_number;
            }

            IjklAstNode* parameter_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_PARAMETER, parameter_line, parameter_column);
            parameter_node->data.parameter.parameter_name = parameter_name;
            parameter_node->data.parameter.type_specification = parameter_type_specification;
            parameter_node->data.parameter.is_by_reference = is_by_reference;

            if (!first_parameter_node) first_parameter_node = parameter_node;
            else last_parameter_node->next_sibling = parameter_node;
            last_parameter_node = parameter_node;
        } while (parser_match_type(parser, TOKEN_COMMA));
    }

    parser_consume_token(parser, TOKEN_RPAREN, "Expected ')' after parameter list.");

    /* Distinção funcional: rotina com ':' define Função com tipo de retorno; sem ':', define Procedimento */
    bool is_function_routine = false;
    IjklTypeSpec* return_type_specification = NULL;
    if (parser_match_type(parser, TOKEN_COLON)) {
        is_function_routine = true;
        return_type_specification = parse_type_specification_internal(parser, false);
    }

    IjklAstNode* routine_body_block = parse_block_statement(parser);

    IjklAstNode* routine_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_DECL_ROUTINE, routine_name_token.line_number, routine_name_token.column_number);
    routine_node->data.routine_declaration.routine_name = routine_name_token.lexeme;
    routine_node->data.routine_declaration.is_function = is_function_routine;
    routine_node->data.routine_declaration.is_public = is_public;
    routine_node->data.routine_declaration.return_type_spec = return_type_specification;
    routine_node->data.routine_declaration.parameters = first_parameter_node;
    routine_node->data.routine_declaration.routine_body = routine_body_block;
    return routine_node;
}

static IjklAstNode* parse_constant_declaration(IjklParser* parser) {
    IjklToken constant_keyword_token = parser_advance_token(parser);
    IjklTypeSpec* constant_type_specification = NULL;
    IjklToken constant_name_token;

    /*
     * Suporte à ordem uniforme: constante <tipo> <identificador> = <expressao>;
     * e à forma com tipo inferido: constante <identificador> = <expressao>;
     */
    if (is_type_start(parser, 0) ||
        (parser_peek_token(parser, 0).type == TOKEN_IDENTIFIER && parser_peek_token(parser, 1).type == TOKEN_IDENTIFIER)) {
        constant_type_specification = parse_type_specification(parser);
        constant_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected constant name.");
    } else {
        constant_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected constant name.");
        if (parser_match_type(parser, TOKEN_COLON)) {
            constant_type_specification = parse_type_specification(parser);
        }
    }

    parser_consume_token(parser, TOKEN_EQUAL, "Expected '=' in constant declaration.");
    IjklAstNode* initial_value_expression = parse_expression(parser);
    parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after constant declaration.");

    IjklAstNode* constant_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_DECL_CONST, constant_keyword_token.line_number, constant_keyword_token.column_number);
    constant_node->data.constant_declaration.constant_name = constant_name_token.lexeme;
    constant_node->data.constant_declaration.type_specification = constant_type_specification;
    constant_node->data.constant_declaration.initial_value_expression = initial_value_expression;
    return constant_node;
}

static IjklAstNode* parse_type_declaration(IjklParser* parser) {
    IjklToken type_keyword_token = parser_advance_token(parser);
    IjklToken type_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected type name.");
    parser_consume_token(parser, TOKEN_EQUAL, "Expected '=' in type declaration.");
    IjklTypeSpec* defined_type_specification = parse_type_specification_internal(parser, false);
    parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after type declaration.");

    IjklAstNode* type_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_DECL_TYPE, type_keyword_token.line_number, type_keyword_token.column_number);
    type_node->data.type_declaration.defined_type_name = type_name_token.lexeme;
    type_node->data.type_declaration.type_specification = defined_type_specification;
    return type_node;
}

/* --- Unidade de Compilação Completa --- */

static IjklAstNode* parse_single_compilation_unit(IjklParser* parser) {
    if (parser_check_type(parser, TOKEN_EOF)) return NULL;

    /* O compilador espera encontrar 'import', 'program' ou 'unit' como primeiro identificador no idioma selecionado */
    if (!parser_check_type(parser, TOKEN_KW_IMPORT) &&
        !parser_check_type(parser, TOKEN_KW_PROGRAM) &&
        !parser_check_type(parser, TOKEN_KW_UNIT)) {
        const IjklVocabulary* vocabulary = ijkl_lexer_get_vocabulary(parser->lexer_instance);
        const char* kw_import_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_IMPORT);
        const char* kw_program_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_PROGRAM);
        const char* kw_unit_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_UNIT);

        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                 "Expected '%s', '%s' or '%s' at start of file.",
                 kw_import_str ? kw_import_str : "import",
                 kw_program_str ? kw_program_str : "program",
                 kw_unit_str ? kw_unit_str : "unit");
        parser_report_error(parser, error_buffer);
        return NULL;
    }

    /* 1. Declarações de importação precedendo o cabeçalho (Requisito 7) */
    IjklAstNode* first_import_node = NULL;
    IjklAstNode* last_import_node = NULL;

    while (parser_match_type(parser, TOKEN_KW_IMPORT)) {
        IjklToken first_segment;
        if (is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
            first_segment = parser_advance_token(parser);
        } else {
            first_segment = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected module identifier after 'import'.");
        }
        char full_import_path[512];
        strncpy(full_import_path, first_segment.lexeme ? first_segment.lexeme : "", sizeof(full_import_path) - 1);
        full_import_path[sizeof(full_import_path) - 1] = '\0';

        while (parser_match_type(parser, TOKEN_DOT)) {
            IjklToken path_segment_token;
            if (is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
                path_segment_token = parser_advance_token(parser);
            } else {
                path_segment_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected module path segment after '.'.");
            }
            strncat(full_import_path, ".", sizeof(full_import_path) - strlen(full_import_path) - 1);
            strncat(full_import_path, path_segment_token.lexeme ? path_segment_token.lexeme : "", sizeof(full_import_path) - strlen(full_import_path) - 1);
        }

        parser_consume_token(parser, TOKEN_SEMICOLON, "Expected ';' after import declaration.");

        const char* stored_path = ijkl_memory_arena_duplicate_string(parser->memory_arena, full_import_path);
        IjklAstNode* import_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_IMPORT, first_segment.line_number, first_segment.column_number);
        import_node->data.import_declaration.module_path = stored_path;

        if (!first_import_node) first_import_node = import_node;
        else last_import_node->next_sibling = import_node;
        last_import_node = import_node;
    }

    if (parser_check_type(parser, TOKEN_EOF)) {
        /* Arquivo continha apenas imports ou vazio */
        return first_import_node;
    }

    /* 2. Determina se é 'unidade' / 'unit' (módulo) ou 'programa' / 'program' (Requisito 5 estrito) */
    bool is_explicit_unit = parser_match_type(parser, TOKEN_KW_UNIT);
    bool is_explicit_program = false;
    if (!is_explicit_unit) {
        is_explicit_program = parser_match_type(parser, TOKEN_KW_PROGRAM);
    }

    if (!is_explicit_unit && !is_explicit_program) {
        const IjklVocabulary* vocabulary = ijkl_lexer_get_vocabulary(parser->lexer_instance);
        const char* kw_program_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_PROGRAM);
        const char* kw_unit_str = ijkl_vocabulary_get_keyword_word(vocabulary, TOKEN_KW_UNIT);

        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                 "Expected '%s' or '%s' header declaration.",
                 kw_program_str ? kw_program_str : "program",
                 kw_unit_str ? kw_unit_str : "unit");
        parser_report_error(parser, error_buffer);
        return NULL;
    }

    IjklToken unit_name_token;
    if (is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
        unit_name_token = parser_advance_token(parser);
    } else {
        unit_name_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected program or module identifier.");
    }

    char full_unit_path[512];
    strncpy(full_unit_path, unit_name_token.lexeme ? unit_name_token.lexeme : "", sizeof(full_unit_path) - 1);
    full_unit_path[sizeof(full_unit_path) - 1] = '\0';

    bool has_dotted_path = false;
    while (parser_match_type(parser, TOKEN_DOT)) {
        has_dotted_path = true;
        IjklToken path_segment_token;
        if (is_token_name_or_keyword(parser_peek_token(parser, 0).type)) {
            path_segment_token = parser_advance_token(parser);
        } else {
            path_segment_token = parser_consume_token(parser, TOKEN_IDENTIFIER, "Expected module path segment after '.'.");
        }
        strncat(full_unit_path, ".", sizeof(full_unit_path) - strlen(full_unit_path) - 1);
        strncat(full_unit_path, path_segment_token.lexeme ? path_segment_token.lexeme : "", sizeof(full_unit_path) - strlen(full_unit_path) - 1);
    }

    bool is_module = is_explicit_unit || has_dotted_path;

    if (is_module) {
        parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' to start module body.");

        IjklAstNode* first_declaration_node = NULL;
        IjklAstNode* last_declaration_node = NULL;

        while (!parser_check_type(parser, TOKEN_RBRACE) && !parser_check_type(parser, TOKEN_EOF)) {
            if (parser->error_count >= 30) {
                break;
            }
            IjklToken token_before = parser_peek_token(parser, 0);
            IjklAstNode* current_declaration = NULL;
            if (parser_check_type(parser, TOKEN_KW_CONST)) {
                current_declaration = parse_constant_declaration(parser);
            } else if (parser_check_type(parser, TOKEN_KW_TYPE)) {
                current_declaration = parse_type_declaration(parser);
            } else if (is_routine_declaration(parser)) {
                current_declaration = parse_routine_declaration(parser);
            } else if (parser_check_type(parser, TOKEN_KW_ASM)) {
                current_declaration = parse_statement(parser);
            } else if (is_type_start(parser, 0) || is_user_type_variable_declaration(parser)) {
                current_declaration = parse_statement(parser);
            } else {
                parser_report_error(parser, "Executable statements are not allowed directly in namespace body. All executable code must reside inside functions or procedures.");
                parser_synchronize(parser);
                continue;
            }

            if (current_declaration) {
                if (!first_declaration_node) first_declaration_node = current_declaration;
                else last_declaration_node->next_sibling = current_declaration;
                last_declaration_node = current_declaration;
            }

            IjklToken token_after = parser_peek_token(parser, 0);
            if (token_before.line_number == token_after.line_number &&
                token_before.column_number == token_after.column_number &&
                !parser_check_type(parser, TOKEN_EOF)) {
                parser_synchronize(parser);
            }
        }

        parser_consume_token(parser, TOKEN_RBRACE, "Expected '}' to close module body.");

        const char* stored_module_path = ijkl_memory_arena_duplicate_string(parser->memory_arena, full_unit_path);
        IjklAstNode* module_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_MODULE, unit_name_token.line_number, unit_name_token.column_number);
        module_node->data.module_declaration.qualified_module_path = stored_module_path;
        module_node->data.module_declaration.imports = first_import_node;
        module_node->data.module_declaration.declarations = first_declaration_node;
        return module_node;
    }

    /* Bloco do Programa Principal: <NomePrograma> { ... } */
    parser_consume_token(parser, TOKEN_LBRACE, "Expected '{' after program name.");

    IjklAstNode* first_declaration_node = NULL;
    IjklAstNode* last_declaration_node = NULL;

    while (!parser_check_type(parser, TOKEN_RBRACE) && !parser_check_type(parser, TOKEN_EOF)) {
        IjklToken token_before = parser_peek_token(parser, 0);
        if (parser_check_type(parser, TOKEN_KW_CONST)) {
            IjklAstNode* constant_declaration = parse_constant_declaration(parser);
            if (!first_declaration_node) first_declaration_node = constant_declaration;
            else last_declaration_node->next_sibling = constant_declaration;
            last_declaration_node = constant_declaration;
        } else if (parser_check_type(parser, TOKEN_KW_TYPE)) {
            IjklAstNode* type_declaration = parse_type_declaration(parser);
            if (!first_declaration_node) first_declaration_node = type_declaration;
            else last_declaration_node->next_sibling = type_declaration;
            last_declaration_node = type_declaration;
        } else if (is_routine_declaration(parser)) {
            IjklAstNode* routine_declaration = parse_routine_declaration(parser);
            if (!first_declaration_node) first_declaration_node = routine_declaration;
            else last_declaration_node->next_sibling = routine_declaration;
            last_declaration_node = routine_declaration;
        } else if (parser_check_type(parser, TOKEN_KW_ASM)) {
            IjklAstNode* asm_declaration = parse_statement(parser);
            if (!first_declaration_node) first_declaration_node = asm_declaration;
            else last_declaration_node->next_sibling = asm_declaration;
            last_declaration_node = asm_declaration;
        } else if (is_type_start(parser, 0) || is_user_type_variable_declaration(parser)) {
            /* Declaração de variável no corpo do programa */
            IjklAstNode* variable_declaration = parse_statement(parser);
            if (!first_declaration_node) first_declaration_node = variable_declaration;
            else last_declaration_node->next_sibling = variable_declaration;
            last_declaration_node = variable_declaration;
        } else {
            parser_report_error(parser, "Executable statements are not allowed directly in namespace body. All executable code must reside inside functions or procedures (e.g. 'privado principal(): inteiro { ... }').");
            parser_synchronize(parser);
            continue;
        }

        IjklToken token_after = parser_peek_token(parser, 0);
        if (token_before.line_number == token_after.line_number &&
            token_before.column_number == token_after.column_number &&
            !parser_check_type(parser, TOKEN_EOF)) {
            parser_synchronize(parser);
        }
    }

    parser_consume_token(parser, TOKEN_RBRACE, "Expected '}' to close program body.");

    IjklAstNode* program_node = ijkl_ast_node_create(parser->memory_arena, AST_NODE_PROGRAM, unit_name_token.line_number, unit_name_token.column_number);
    program_node->data.program.program_name = unit_name_token.lexeme;
    program_node->data.program.imports = first_import_node;
    program_node->data.program.declarations = first_declaration_node;
    program_node->data.program.entry_point = NULL;

    return program_node;
}

IjklAstNode* ijkl_parser_parse_compilation_unit(IjklParser* parser) {
    if (!parser) return NULL;

    IjklAstNode* first_compilation_unit = NULL;
    IjklAstNode* last_compilation_unit = NULL;

    while (!parser_check_type(parser, TOKEN_EOF)) {
        IjklAstNode* current_unit = parse_single_compilation_unit(parser);
        if (!current_unit) {
            break;
        }

        if (!first_compilation_unit) {
            first_compilation_unit = current_unit;
        } else {
            last_compilation_unit->next_sibling = current_unit;
        }
        last_compilation_unit = current_unit;
    }

    return first_compilation_unit;
}

IjklParser* ijkl_parser_create(IjklLexer* lexer_instance,
                               IjklMemoryArena* memory_arena,
                               const char* source_file_path) {
    IjklParser* parser;
    if (memory_arena) {
        parser = (IjklParser*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklParser));
    } else {
        parser = (IjklParser*)ijkl_memory_heap_allocate(sizeof(IjklParser));
    }

    memset(parser, 0, sizeof(IjklParser));
    parser->lexer_instance = lexer_instance;
    parser->memory_arena = memory_arena;
    parser->source_file_path = source_file_path;
    parser->lookahead_count = 0;
    parser->error_count = 0;
    parser->is_panic_mode = false;

    return parser;
}

void ijkl_parser_destroy(IjklParser* parser) {
    if (!parser) return;
    if (!parser->memory_arena) {
        ijkl_memory_heap_free(parser);
    }
}

size_t ijkl_parser_get_error_count(const IjklParser* parser) {
    return parser ? parser->error_count : 0;
}
