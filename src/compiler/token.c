#include "token.h"

const char* ijkl_token_type_name(IjklTokenType token_type) {
    switch (token_type) {
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_INTEGER: return "TOKEN_INTEGER";
        case TOKEN_REAL: return "TOKEN_REAL";
        case TOKEN_FLOAT: return "TOKEN_FLOAT";
        case TOKEN_CHAR: return "TOKEN_CHAR";
        case TOKEN_STRING: return "TOKEN_STRING";

        case TOKEN_KW_IF: return "TOKEN_KW_IF";
        case TOKEN_KW_ELSE: return "TOKEN_KW_ELSE";
        case TOKEN_KW_SWITCH: return "TOKEN_KW_SWITCH";
        case TOKEN_KW_CASE: return "TOKEN_KW_CASE";
        case TOKEN_KW_DEFAULT: return "TOKEN_KW_DEFAULT";
        case TOKEN_KW_WHILE: return "TOKEN_KW_WHILE";
        case TOKEN_KW_FOR: return "TOKEN_KW_FOR";
        case TOKEN_KW_FROM: return "TOKEN_KW_FROM";
        case TOKEN_KW_TO: return "TOKEN_KW_TO";
        case TOKEN_KW_STEP: return "TOKEN_KW_STEP";
        case TOKEN_KW_REPEAT: return "TOKEN_KW_REPEAT";
        case TOKEN_KW_BREAK: return "TOKEN_KW_BREAK";
        case TOKEN_KW_RETURN: return "TOKEN_KW_RETURN";
        case TOKEN_KW_ALLOC: return "TOKEN_KW_ALLOC";
        case TOKEN_KW_FREE: return "TOKEN_KW_FREE";
        case TOKEN_KW_READ: return "TOKEN_KW_READ";
        case TOKEN_KW_WRITE: return "TOKEN_KW_WRITE";
        case TOKEN_KW_WRITELN: return "TOKEN_KW_WRITELN";
        case TOKEN_KW_INT: return "TOKEN_KW_INT";
        case TOKEN_KW_REAL: return "TOKEN_KW_REAL";
        case TOKEN_KW_FLOAT: return "TOKEN_KW_FLOAT";
        case TOKEN_KW_CHAR: return "TOKEN_KW_CHAR";
        case TOKEN_KW_STRING: return "TOKEN_KW_STRING";
        case TOKEN_KW_BOOL: return "TOKEN_KW_BOOL";
        case TOKEN_KW_ARRAY: return "TOKEN_KW_ARRAY";
        case TOKEN_KW_STRUCT: return "TOKEN_KW_STRUCT";
        case TOKEN_KW_POINTER: return "TOKEN_KW_POINTER";
        case TOKEN_KW_REF: return "TOKEN_KW_REF";
        case TOKEN_KW_CONST: return "TOKEN_KW_CONST";
        case TOKEN_KW_TYPE: return "TOKEN_KW_TYPE";
        case TOKEN_KW_TRUE: return "TOKEN_KW_TRUE";
        case TOKEN_KW_FALSE: return "TOKEN_KW_FALSE";
        case TOKEN_KW_NULL: return "TOKEN_KW_NULL";
        case TOKEN_KW_AND: return "TOKEN_KW_AND";
        case TOKEN_KW_OR: return "TOKEN_KW_OR";
        case TOKEN_KW_NOT: return "TOKEN_KW_NOT";
        case TOKEN_KW_DIV: return "TOKEN_KW_DIV";
        case TOKEN_KW_MOD: return "TOKEN_KW_MOD";
        case TOKEN_KW_UNIT: return "TOKEN_KW_UNIT";
        case TOKEN_KW_PROGRAM: return "TOKEN_KW_PROGRAM";
        case TOKEN_KW_IMPORT: return "TOKEN_KW_IMPORT";
        case TOKEN_KW_PUBLIC: return "TOKEN_KW_PUBLIC";
        case TOKEN_KW_PRIVATE: return "TOKEN_KW_PRIVATE";
        case TOKEN_KW_OF: return "TOKEN_KW_OF";
        case TOKEN_KW_MAIN: return "TOKEN_KW_MAIN";
        case TOKEN_KW_INITIALIZE: return "TOKEN_KW_INITIALIZE";
        case TOKEN_KW_FINALIZE: return "TOKEN_KW_FINALIZE";
        case TOKEN_KW_ASM: return "TOKEN_KW_ASM";

        case TOKEN_AT: return "TOKEN_AT";
        case TOKEN_DOLLAR: return "TOKEN_DOLLAR";
        case TOKEN_AMPERSAND: return "TOKEN_AMPERSAND";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_LBRACKET: return "TOKEN_LBRACKET";
        case TOKEN_RBRACKET: return "TOKEN_RBRACKET";
        case TOKEN_LBRACE: return "TOKEN_LBRACE";
        case TOKEN_RBRACE: return "TOKEN_RBRACE";
        case TOKEN_ASSIGN_PASCAL: return "TOKEN_ASSIGN_PASCAL";
        case TOKEN_EQUAL: return "TOKEN_EQUAL";
        case TOKEN_EQUAL_STRICT: return "TOKEN_EQUAL_STRICT";
        case TOKEN_NOT_EQUAL: return "TOKEN_NOT_EQUAL";
        case TOKEN_NOT_EQUAL_PASCAL: return "TOKEN_NOT_EQUAL_PASCAL";
        case TOKEN_LESS: return "TOKEN_LESS";
        case TOKEN_GREATER: return "TOKEN_GREATER";
        case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL";
        case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_CARET: return "TOKEN_CARET";
        case TOKEN_LOGICAL_AND: return "TOKEN_LOGICAL_AND";
        case TOKEN_LOGICAL_OR: return "TOKEN_LOGICAL_OR";
        case TOKEN_EXCLAMATION: return "TOKEN_EXCLAMATION";
        default: return "TOKEN_UNKNOWN";
    }
}

void ijkl_token_print(const IjklToken* token, FILE* output_stream) {
    if (!token) return;
    FILE* stream = output_stream ? output_stream : stdout;

    fprintf(stream, "[%4zu:%3zu] %-25s \"%s\"",
            token->line_number,
            token->column_number,
            ijkl_token_type_name(token->type),
            token->lexeme ? token->lexeme : "");

    if (token->type == TOKEN_IDENTIFIER && token->normalized_lexeme &&
        token->lexeme && strcmp(token->lexeme, token->normalized_lexeme) != 0) {
        fprintf(stream, " [normalized: %s]", token->normalized_lexeme);
    } else if (token->type == TOKEN_INTEGER) {
        fprintf(stream, " (value: %lld)", (long long)token->value.integer_value);
    } else if (token->type == TOKEN_REAL) {
        fprintf(stream, " (value: %g)", token->value.real_value);
    } else if (token->type == TOKEN_FLOAT) {
        fprintf(stream, " (value: %g)", token->value.float_value);
    } else if (token->type == TOKEN_CHAR) {
        fprintf(stream, " (char: '%c')", token->value.char_value);
    } else if (token->type == TOKEN_STRING && token->value.string_value) {
        fprintf(stream, " (text: \"%s\")", token->value.string_value);
    } else if (token->type == TOKEN_ERROR && token->value.string_value) {
        fprintf(stream, " [ERROR: %s]", token->value.string_value);
    }

    fprintf(stream, "\n");
}

bool ijkl_token_type_is_keyword(IjklTokenType token_type) {
    return token_type >= TOKEN_KW_IF && token_type <= TOKEN_KW_FINALIZE;
}

bool ijkl_token_type_is_literal(IjklTokenType token_type) {
    switch (token_type) {
        case TOKEN_INTEGER:
        case TOKEN_REAL:
        case TOKEN_FLOAT:
        case TOKEN_CHAR:
        case TOKEN_STRING:
        case TOKEN_KW_TRUE:
        case TOKEN_KW_FALSE:
        case TOKEN_KW_NULL:
            return true;
        default:
            return false;
    }
}
