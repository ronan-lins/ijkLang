#include "lexer.h"

/* Estrutura interna completa do analisador léxico */
struct IjklLexer {
    const char* source_code;
    size_t source_length_in_bytes;
    size_t cursor_position;
    size_t current_line_number;
    size_t current_column_number;

    const IjklVocabulary* vocabulary;
    IjklMemoryArena* memory_arena;
    const char* source_file_name;

    bool has_peeked_token;
    IjklToken peeked_token;
};

IjklLexer* ijkl_lexer_create(const char* source_code,
                             size_t source_length_in_bytes,
                             const IjklVocabulary* vocabulary,
                             IjklMemoryArena* memory_arena,
                             const char* source_file_name) {
    IjklLexer* lexer;
    if (memory_arena) {
        lexer = (IjklLexer*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklLexer));
    } else {
        lexer = (IjklLexer*)ijkl_memory_heap_allocate(sizeof(IjklLexer));
    }

    lexer->source_code = source_code;
    lexer->source_length_in_bytes = source_length_in_bytes;
    lexer->cursor_position = 0;
    lexer->current_line_number = 1;
    lexer->current_column_number = 1;
    lexer->vocabulary = vocabulary;
    lexer->memory_arena = memory_arena;

    if (source_file_name) {
        if (memory_arena) {
            lexer->source_file_name = ijkl_memory_arena_duplicate_string(memory_arena, source_file_name);
        } else {
            lexer->source_file_name = ijkl_strdup(source_file_name);
        }
    } else {
        lexer->source_file_name = NULL;
    }

    lexer->has_peeked_token = false;
    memset(&lexer->peeked_token, 0, sizeof(IjklToken));

    return lexer;
}

void ijkl_lexer_destroy(IjklLexer* lexer) {
    if (!lexer) return;
    /* Se o lexer foi alocado fora de uma arena, libera a estrutura e o nome do arquivo */
    if (!lexer->memory_arena) {
        if (lexer->source_file_name) {
            ijkl_memory_heap_free((void*)lexer->source_file_name);
        }
        ijkl_memory_heap_free(lexer);
    }
}

static inline char lexer_peek_character(const IjklLexer* lexer) {
    if (lexer->cursor_position >= lexer->source_length_in_bytes) return '\0';
    return lexer->source_code[lexer->cursor_position];
}

static inline char lexer_peek_character_ahead(const IjklLexer* lexer, size_t lookahead_offset) {
    if (lexer->cursor_position + lookahead_offset >= lexer->source_length_in_bytes) return '\0';
    return lexer->source_code[lexer->cursor_position + lookahead_offset];
}

/*
 * EFEITO COLATERAL / MUTAÇÃO DE CURSOR:
 * Consome o caractere apontado por cursor_position e atualiza as coordenadas
 * de linha e coluna no código-fonte.
 */
static inline char lexer_advance_character(IjklLexer* lexer) {
    if (lexer->cursor_position >= lexer->source_length_in_bytes) return '\0';
    char current_character = lexer->source_code[lexer->cursor_position++];
    if (current_character == '\n') {
        lexer->current_line_number++;
        lexer->current_column_number = 1;
    } else {
        lexer->current_column_number++;
    }
    return current_character;
}

static const char* lexer_allocate_string(IjklLexer* lexer, const char* source_string, size_t string_length) {
    if (!source_string) return NULL;
    if (lexer->memory_arena) {
        return ijkl_memory_arena_duplicate_string_slice(lexer->memory_arena, source_string, string_length);
    } else {
        char* heap_copy = (char*)ijkl_memory_heap_allocate(string_length + 1);
        memcpy(heap_copy, source_string, string_length);
        heap_copy[string_length] = '\0';
        return heap_copy;
    }
}

/*
 * Avalia se o caractere sob o cursor pode iniciar ou compor um identificador
 * (considerando tanto ASCII quanto sequências UTF-8 mapeadas no vocabulário).
 */
static bool is_identifier_part(IjklLexer* lexer,
                              bool is_first_character,
                              size_t* out_byte_length,
                              char* out_normalized_ascii) {
    if (lexer->cursor_position >= lexer->source_length_in_bytes) return false;
    char first_byte = lexer->source_code[lexer->cursor_position];

    /* Letras e sublinhado da tabela ASCII padrão */
    if ((first_byte >= 'a' && first_byte <= 'z') ||
        (first_byte >= 'A' && first_byte <= 'Z') ||
        first_byte == '_') {
        *out_byte_length = 1;
        *out_normalized_ascii = first_byte;
        return true;
    }

    /* Dígitos decimais são válidos apenas a partir do segundo caractere do identificador */
    if (!is_first_character && (first_byte >= '0' && first_byte <= '9')) {
        *out_byte_length = 1;
        *out_normalized_ascii = first_byte;
        return true;
    }

    /* Sequências de múltiplos bytes em UTF-8 (acentuação e caracteres internacionais) */
    if ((unsigned char)first_byte >= 0x80) {
        const char* stream_cursor = &lexer->source_code[lexer->cursor_position];
        const char* previous_stream_cursor = stream_cursor;
        uint32_t unicode_codepoint = ijkl_utf8_decode_next_codepoint(&stream_cursor);

        if (unicode_codepoint != 0 && unicode_codepoint != IJKL_UTF8_REPLACEMENT_CHARACTER) {
            char normalized_character = ijkl_vocabulary_map_codepoint_to_ascii(lexer->vocabulary, unicode_codepoint);
            if (normalized_character != '\0') {
                *out_byte_length = (size_t)(stream_cursor - previous_stream_cursor);
                *out_normalized_ascii = normalized_character;
                return true;
            }
        }
    }

    return false;
}

/*
 * Ignora caracteres de espaço em branco e comentários de linha (//) e de bloco.
 * Retorna false e popula 'error_token' se um bloco de comentário não for fechado antes do fim do arquivo.
 */
static bool skip_whitespace_and_comments(IjklLexer* lexer, IjklToken* error_token) {
    while (lexer->cursor_position < lexer->source_length_in_bytes) {
        char current_character = lexer_peek_character(lexer);

        /* Espaços, tabulações e quebras de linha */
        if (current_character == ' ' || current_character == '\t' ||
            current_character == '\r' || current_character == '\n') {
            lexer_advance_character(lexer);
            continue;
        }

        /* Comentários de linha única iniciados por // */
        if (current_character == '/' && lexer_peek_character_ahead(lexer, 1) == '/') {
            lexer_advance_character(lexer); /* Consome a primeira barra */
            lexer_advance_character(lexer); /* Consome a segunda barra */
            while (lexer->cursor_position < lexer->source_length_in_bytes &&
                   lexer_peek_character(lexer) != '\n') {
                lexer_advance_character(lexer);
            }
            if (lexer->cursor_position < lexer->source_length_in_bytes &&
                lexer_peek_character(lexer) == '\n') {
                lexer_advance_character(lexer);
            }
            continue;
        }

        /* Comentários de bloco delimitados por barras e asteriscos */
        if (current_character == '/' && lexer_peek_character_ahead(lexer, 1) == '*') {
            size_t comment_start_line = lexer->current_line_number;
            size_t comment_start_column = lexer->current_column_number;
            lexer_advance_character(lexer); /* Consome a barra */
            lexer_advance_character(lexer); /* Consome o asterisco */

            bool block_comment_was_closed = false;
            while (lexer->cursor_position < lexer->source_length_in_bytes) {
                if (lexer_peek_character(lexer) == '*' &&
                    lexer_peek_character_ahead(lexer, 1) == '/') {
                    lexer_advance_character(lexer); /* Consome o asterisco final */
                    lexer_advance_character(lexer); /* Consome a barra final */
                    block_comment_was_closed = true;
                    break;
                }
                lexer_advance_character(lexer);
            }

            if (!block_comment_was_closed) {
                memset(error_token, 0, sizeof(IjklToken));
                error_token->type = TOKEN_ERROR;
                error_token->line_number = comment_start_line;
                error_token->column_number = comment_start_column;
                error_token->lexeme = lexer_allocate_string(lexer, "/*", 2);
                error_token->length = 2;
                error_token->normalized_lexeme = error_token->lexeme;
                const char* error_message = "Unclosed block comment before end of file.";
                error_token->value.string_value = lexer_allocate_string(lexer, error_message, strlen(error_message));
                return false;
            }
            continue;
        }

        break;
    }
    return true;
}

IjklToken ijkl_lexer_next_token(IjklLexer* lexer) {
    if (lexer->has_peeked_token) {
        lexer->has_peeked_token = false;
        return lexer->peeked_token;
    }

    IjklToken error_token;
    if (!skip_whitespace_and_comments(lexer, &error_token)) {
        return error_token;
    }

    IjklToken generated_token;
    memset(&generated_token, 0, sizeof(IjklToken));
    generated_token.line_number = lexer->current_line_number;
    generated_token.column_number = lexer->current_column_number;

    /* Fim do fluxo de código-fonte */
    if (lexer->cursor_position >= lexer->source_length_in_bytes) {
        generated_token.type = TOKEN_EOF;
        generated_token.lexeme = lexer_allocate_string(lexer, "", 0);
        generated_token.normalized_lexeme = generated_token.lexeme;
        generated_token.length = 0;
        return generated_token;
    }

    size_t token_start_position = lexer->cursor_position;
    size_t matched_byte_length = 0;
    char normalized_character = '\0';

    /* Identificadores e Palavras Reservadas */
    if (is_identifier_part(lexer, true, &matched_byte_length, &normalized_character)) {
        char normalized_identifier_buffer[512];
        size_t normalized_length = 0;

        while (is_identifier_part(lexer, false, &matched_byte_length, &normalized_character)) {
            if (normalized_length + 1 < sizeof(normalized_identifier_buffer)) {
                normalized_identifier_buffer[normalized_length++] = normalized_character;
            }
            /* Consome os bytes correspondentes da sequência de caracteres */
            for (size_t byte_index = 0; byte_index < matched_byte_length; byte_index++) {
                lexer_advance_character(lexer);
            }
        }
        normalized_identifier_buffer[normalized_length] = '\0';

        size_t raw_token_length = lexer->cursor_position - token_start_position;
        generated_token.lexeme = lexer_allocate_string(lexer,
                                                      &lexer->source_code[token_start_position],
                                                      raw_token_length);
        generated_token.length = raw_token_length;
        generated_token.normalized_lexeme = lexer_allocate_string(lexer,
                                                                 normalized_identifier_buffer,
                                                                 normalized_length);

        /* Pesquisa no vocabulário se o lexema normalizado é uma palavra-chave */
        IjklTokenType keyword_token_type;
        if (ijkl_vocabulary_lookup_keyword(lexer->vocabulary, normalized_identifier_buffer, &keyword_token_type)) {
            generated_token.type = keyword_token_type;
        } else {
            generated_token.type = TOKEN_IDENTIFIER;
        }

        return generated_token;
    }

    char current_character = lexer_peek_character(lexer);

    /* Literais Numéricos (Inteiro, Hexadecimal, Real de precisão dupla e Flutuante de precisão simples) */
    if (isdigit((unsigned char)current_character)) {
        /* Notação Hexadecimal 0x... */
        if (current_character == '0' &&
            (lexer_peek_character_ahead(lexer, 1) == 'x' || lexer_peek_character_ahead(lexer, 1) == 'X')) {
            lexer_advance_character(lexer); /* Consome '0' */
            lexer_advance_character(lexer); /* Consome 'x' */

            size_t hexadecimal_digits_start = lexer->cursor_position;
            while (isxdigit((unsigned char)lexer_peek_character(lexer))) {
                lexer_advance_character(lexer);
            }

            if (lexer->cursor_position == hexadecimal_digits_start) {
                generated_token.type = TOKEN_ERROR;
                size_t error_length = lexer->cursor_position - token_start_position;
                generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], error_length);
                generated_token.length = error_length;
                generated_token.normalized_lexeme = generated_token.lexeme;
                const char* error_message = "Invalid hexadecimal literal without digits after '0x'.";
                generated_token.value.string_value = lexer_allocate_string(lexer, error_message, strlen(error_message));
                return generated_token;
            }

            size_t raw_length = lexer->cursor_position - token_start_position;
            generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], raw_length);
            generated_token.length = raw_length;
            generated_token.normalized_lexeme = generated_token.lexeme;
            generated_token.type = TOKEN_INTEGER;
            generated_token.value.integer_value = (int64_t)strtoull(generated_token.lexeme, NULL, 16);
            return generated_token;
        }

        /* Dígitos decimais inteiros */
        while (isdigit((unsigned char)lexer_peek_character(lexer))) {
            lexer_advance_character(lexer);
        }

        bool is_fractional_number = false;

        /* Ponto decimal seguido de dígito */
        if (lexer_peek_character(lexer) == '.' && isdigit((unsigned char)lexer_peek_character_ahead(lexer, 1))) {
            is_fractional_number = true;
            lexer_advance_character(lexer); /* Consome o ponto '.' */
            while (isdigit((unsigned char)lexer_peek_character(lexer))) {
                lexer_advance_character(lexer);
            }
        }

        /* Notação científica de expoente (e / E) */
        if (lexer_peek_character(lexer) == 'e' || lexer_peek_character(lexer) == 'E') {
            char lookahead_after_e = lexer_peek_character_ahead(lexer, 1);
            if (isdigit((unsigned char)lookahead_after_e) ||
                ((lookahead_after_e == '+' || lookahead_after_e == '-') &&
                 isdigit((unsigned char)lexer_peek_character_ahead(lexer, 2)))) {
                is_fractional_number = true;
                lexer_advance_character(lexer); /* Consome 'e' ou 'E' */
                if (lexer_peek_character(lexer) == '+' || lexer_peek_character(lexer) == '-') {
                    lexer_advance_character(lexer);
                }
                while (isdigit((unsigned char)lexer_peek_character(lexer))) {
                    lexer_advance_character(lexer);
                }
            }
        }

        /* Sufixo flutuante explícito 'f' ou 'F' */
        if (lexer_peek_character(lexer) == 'f' || lexer_peek_character(lexer) == 'F') {
            lexer_advance_character(lexer);
            size_t raw_length = lexer->cursor_position - token_start_position;
            generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], raw_length);
            generated_token.length = raw_length;
            generated_token.normalized_lexeme = generated_token.lexeme;
            generated_token.type = TOKEN_FLOAT;
            generated_token.value.float_value = (float)strtod(generated_token.lexeme, NULL);
            return generated_token;
        }

        size_t raw_length = lexer->cursor_position - token_start_position;
        generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], raw_length);
        generated_token.length = raw_length;
        generated_token.normalized_lexeme = generated_token.lexeme;

        if (is_fractional_number) {
            generated_token.type = TOKEN_REAL;
            generated_token.value.real_value = strtod(generated_token.lexeme, NULL);
        } else {
            generated_token.type = TOKEN_INTEGER;
            generated_token.value.integer_value = (int64_t)strtoll(generated_token.lexeme, NULL, 10);
        }
        return generated_token;
    }

    /* Literais de Caractere e Cadeias de Caracteres ('...' ou "...") */
    if (current_character == '\'' || current_character == '"') {
        char quote_delimiter_character = lexer_advance_character(lexer);
        char unescaped_literal_buffer[2048];
        size_t unescaped_literal_length = 0;
        bool literal_was_properly_closed = false;

        while (lexer->cursor_position < lexer->source_length_in_bytes) {
            char peeked_literal_character = lexer_peek_character(lexer);

            if (peeked_literal_character == quote_delimiter_character) {
                lexer_advance_character(lexer);
                literal_was_properly_closed = true;
                break;
            }
            if (peeked_literal_character == '\n' || peeked_literal_character == '\r') {
                /* Quebras de linha físicas sem escape não são permitidas no interior de literais */
                break;
            }

            if (peeked_literal_character == '\\') {
                lexer_advance_character(lexer); /* Consome a barra invertida de escape */
                char escape_character = lexer_advance_character(lexer);
                char unescaped_value;
                switch (escape_character) {
                    case 'n':  unescaped_value = '\n'; break;
                    case 't':  unescaped_value = '\t'; break;
                    case 'r':  unescaped_value = '\r'; break;
                    case '\\': unescaped_value = '\\'; break;
                    case '\'': unescaped_value = '\''; break;
                    case '"':  unescaped_value = '"';  break;
                    case '0':  unescaped_value = '\0'; break;
                    default:   unescaped_value = escape_character; break;
                }
                if (unescaped_literal_length + 1 < sizeof(unescaped_literal_buffer)) {
                    unescaped_literal_buffer[unescaped_literal_length++] = unescaped_value;
                }
            } else {
                char character_to_store = lexer_advance_character(lexer);
                if (unescaped_literal_length + 1 < sizeof(unescaped_literal_buffer)) {
                    unescaped_literal_buffer[unescaped_literal_length++] = character_to_store;
                }
            }
        }
        unescaped_literal_buffer[unescaped_literal_length] = '\0';

        if (!literal_was_properly_closed) {
            generated_token.type = TOKEN_ERROR;
            size_t error_length = lexer->cursor_position - token_start_position;
            generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], error_length);
            generated_token.length = error_length;
            generated_token.normalized_lexeme = generated_token.lexeme;
            const char* error_message = "Unclosed string or character literal.";
            generated_token.value.string_value = lexer_allocate_string(lexer, error_message, strlen(error_message));
            return generated_token;
        }

        size_t raw_length = lexer->cursor_position - token_start_position;
        generated_token.lexeme = lexer_allocate_string(lexer, &lexer->source_code[token_start_position], raw_length);
        generated_token.length = raw_length;
        generated_token.normalized_lexeme = generated_token.lexeme;

        /* Regra normativa da ijkLang: aspas simples delimitam caractere ('c'); aspas duplas delimitam texto ("...") */
        if (quote_delimiter_character == '\'') {
            generated_token.type = TOKEN_CHAR;
            generated_token.value.char_value = (unescaped_literal_length > 0) ? unescaped_literal_buffer[0] : '\0';
        } else {
            generated_token.type = TOKEN_STRING;
            generated_token.value.string_value = lexer_allocate_string(lexer,
                                                                      unescaped_literal_buffer,
                                                                      unescaped_literal_length);
        }
        return generated_token;
    }

    /* Operadores e Delimitadores Sintáticos */
    lexer_advance_character(lexer);

    switch (current_character) {
        case '@':
            generated_token.type = TOKEN_AT;
            generated_token.lexeme = lexer_allocate_string(lexer, "@", 1);
            break;
        case '$':
            generated_token.type = TOKEN_DOLLAR;
            generated_token.lexeme = lexer_allocate_string(lexer, "$", 1);
            break;
        case '&':
            if (lexer_peek_character(lexer) == '&') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_LOGICAL_AND;
                generated_token.lexeme = lexer_allocate_string(lexer, "&&", 2);
            } else {
                generated_token.type = TOKEN_AMPERSAND;
                generated_token.lexeme = lexer_allocate_string(lexer, "&", 1);
            }
            break;
        case '|':
            if (lexer_peek_character(lexer) == '|') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_LOGICAL_OR;
                generated_token.lexeme = lexer_allocate_string(lexer, "||", 2);
            } else {
                generated_token.type = TOKEN_ERROR;
                generated_token.lexeme = lexer_allocate_string(lexer, "|", 1);
                const char* error_message = "Unknown operator '|'. Use '||' or 'or'.";
                generated_token.value.string_value = lexer_allocate_string(lexer, error_message, strlen(error_message));
            }
            break;
        case '!':
            if (lexer_peek_character(lexer) == '=') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_NOT_EQUAL;
                generated_token.lexeme = lexer_allocate_string(lexer, "!=", 2);
            } else {
                generated_token.type = TOKEN_EXCLAMATION;
                generated_token.lexeme = lexer_allocate_string(lexer, "!", 1);
            }
            break;
        case '.':
            generated_token.type = TOKEN_DOT;
            generated_token.lexeme = lexer_allocate_string(lexer, ".", 1);
            break;
        case ',':
            generated_token.type = TOKEN_COMMA;
            generated_token.lexeme = lexer_allocate_string(lexer, ",", 1);
            break;
        case ':':
            if (lexer_peek_character(lexer) == '=') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_ASSIGN_PASCAL;
                generated_token.lexeme = lexer_allocate_string(lexer, ":=", 2);
            } else {
                generated_token.type = TOKEN_COLON;
                generated_token.lexeme = lexer_allocate_string(lexer, ":", 1);
            }
            break;
        case ';':
            generated_token.type = TOKEN_SEMICOLON;
            generated_token.lexeme = lexer_allocate_string(lexer, ";", 1);
            break;
        case '(':
            generated_token.type = TOKEN_LPAREN;
            generated_token.lexeme = lexer_allocate_string(lexer, "(", 1);
            break;
        case ')':
            generated_token.type = TOKEN_RPAREN;
            generated_token.lexeme = lexer_allocate_string(lexer, ")", 1);
            break;
        case '[':
            generated_token.type = TOKEN_LBRACKET;
            generated_token.lexeme = lexer_allocate_string(lexer, "[", 1);
            break;
        case ']':
            generated_token.type = TOKEN_RBRACKET;
            generated_token.lexeme = lexer_allocate_string(lexer, "]", 1);
            break;
        case '{':
            generated_token.type = TOKEN_LBRACE;
            generated_token.lexeme = lexer_allocate_string(lexer, "{", 1);
            break;
        case '}':
            generated_token.type = TOKEN_RBRACE;
            generated_token.lexeme = lexer_allocate_string(lexer, "}", 1);
            break;
        case '=':
            if (lexer_peek_character(lexer) == '=') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_EQUAL_STRICT;
                generated_token.lexeme = lexer_allocate_string(lexer, "==", 2);
            } else {
                generated_token.type = TOKEN_EQUAL;
                generated_token.lexeme = lexer_allocate_string(lexer, "=", 1);
            }
            break;
        case '<':
            if (lexer_peek_character(lexer) == '>') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_NOT_EQUAL_PASCAL;
                generated_token.lexeme = lexer_allocate_string(lexer, "<>", 2);
            } else if (lexer_peek_character(lexer) == '=') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_LESS_EQUAL;
                generated_token.lexeme = lexer_allocate_string(lexer, "<=", 2);
            } else {
                generated_token.type = TOKEN_LESS;
                generated_token.lexeme = lexer_allocate_string(lexer, "<", 1);
            }
            break;
        case '>':
            if (lexer_peek_character(lexer) == '=') {
                lexer_advance_character(lexer);
                generated_token.type = TOKEN_GREATER_EQUAL;
                generated_token.lexeme = lexer_allocate_string(lexer, ">=", 2);
            } else {
                generated_token.type = TOKEN_GREATER;
                generated_token.lexeme = lexer_allocate_string(lexer, ">", 1);
            }
            break;
        case '+':
            generated_token.type = TOKEN_PLUS;
            generated_token.lexeme = lexer_allocate_string(lexer, "+", 1);
            break;
        case '-':
            generated_token.type = TOKEN_MINUS;
            generated_token.lexeme = lexer_allocate_string(lexer, "-", 1);
            break;
        case '*':
            generated_token.type = TOKEN_STAR;
            generated_token.lexeme = lexer_allocate_string(lexer, "*", 1);
            break;
        case '/':
            generated_token.type = TOKEN_SLASH;
            generated_token.lexeme = lexer_allocate_string(lexer, "/", 1);
            break;
        case '^':
            generated_token.type = TOKEN_CARET;
            generated_token.lexeme = lexer_allocate_string(lexer, "^", 1);
            break;
        default: {
            char unrecognized_character_buffer[2] = {current_character, '\0'};
            generated_token.type = TOKEN_ERROR;
            generated_token.lexeme = lexer_allocate_string(lexer, unrecognized_character_buffer, 1);
            const char* error_message = "Unrecognized character in source code.";
            generated_token.value.string_value = lexer_allocate_string(lexer, error_message, strlen(error_message));
            break;
        }
    }

    generated_token.length = strlen(generated_token.lexeme);
    generated_token.normalized_lexeme = generated_token.lexeme;
    return generated_token;
}

IjklToken ijkl_lexer_peek_token(IjklLexer* lexer) {
    if (!lexer->has_peeked_token) {
        lexer->peeked_token = ijkl_lexer_next_token(lexer);
        lexer->has_peeked_token = true;
    }
    return lexer->peeked_token;
}

bool ijkl_lexer_dump_tokens(const char* source_code,
                            size_t source_length_in_bytes,
                            const IjklVocabulary* vocabulary,
                            IjklMemoryArena* memory_arena,
                            const char* source_file_name,
                            FILE* output_stream) {
    IjklLexer* lexer = ijkl_lexer_create(source_code,
                                         source_length_in_bytes,
                                         vocabulary,
                                         memory_arena,
                                         source_file_name);
    bool execution_encountered_lexical_error = false;
    IjklToken scanned_token;

    do {
        scanned_token = ijkl_lexer_next_token(lexer);
        ijkl_token_print(&scanned_token, output_stream);
        if (scanned_token.type == TOKEN_ERROR) {
            execution_encountered_lexical_error = true;
        }
    } while (scanned_token.type != TOKEN_EOF);

    ijkl_lexer_destroy(lexer);
    return !execution_encountered_lexical_error;
}

const IjklVocabulary* ijkl_lexer_get_vocabulary(const IjklLexer* lexer) {
    return lexer ? lexer->vocabulary : NULL;
}

char* ijkl_lexer_scan_raw_block(IjklLexer* lexer, size_t* out_length) {
    if (!lexer) {
        if (out_length) *out_length = 0;
        return NULL;
    }

    /* Cancela qualquer token que já estivesse no lookahead do lexer */
    lexer->has_peeked_token = false;

    size_t start_position = lexer->cursor_position;
    int brace_depth = 1;

    while (lexer->cursor_position < lexer->source_length_in_bytes) {
        char current_character = lexer_peek_character(lexer);

        /* Comentário de linha '//' */
        if (current_character == '/' && lexer_peek_character_ahead(lexer, 1) == '/') {
            while (lexer->cursor_position < lexer->source_length_in_bytes &&
                   lexer_peek_character(lexer) != '\n') {
                lexer_advance_character(lexer);
            }
            if (lexer->cursor_position < lexer->source_length_in_bytes) {
                lexer_advance_character(lexer); /* Consome '\n' */
            }
            continue;
        }

        /* Comentário de bloco '/*' */
        if (current_character == '/' && lexer_peek_character_ahead(lexer, 1) == '*') {
            lexer_advance_character(lexer);
            lexer_advance_character(lexer);
            while (lexer->cursor_position < lexer->source_length_in_bytes) {
                if (lexer_peek_character(lexer) == '*' && lexer_peek_character_ahead(lexer, 1) == '/') {
                    lexer_advance_character(lexer);
                    lexer_advance_character(lexer);
                    break;
                }
                lexer_advance_character(lexer);
            }
            continue;
        }

        /* Literal de cadeia de caracteres entre aspas duplas */
        if (current_character == '"') {
            lexer_advance_character(lexer);
            while (lexer->cursor_position < lexer->source_length_in_bytes) {
                char string_character = lexer_advance_character(lexer);
                if (string_character == '\\' && lexer->cursor_position < lexer->source_length_in_bytes) {
                    lexer_advance_character(lexer); /* Pula escape */
                } else if (string_character == '"') {
                    break;
                }
            }
            continue;
        }

        /* Literal de caractere entre aspas simples */
        if (current_character == '\'') {
            lexer_advance_character(lexer);
            while (lexer->cursor_position < lexer->source_length_in_bytes) {
                char char_literal = lexer_advance_character(lexer);
                if (char_literal == '\\' && lexer->cursor_position < lexer->source_length_in_bytes) {
                    lexer_advance_character(lexer);
                } else if (char_literal == '\'') {
                    break;
                }
            }
            continue;
        }

        /* Aninhamento balanceado de chaves */
        if (current_character == '{') {
            brace_depth++;
            lexer_advance_character(lexer);
            continue;
        }

        if (current_character == '}') {
            brace_depth--;
            if (brace_depth == 0) {
                size_t raw_length = lexer->cursor_position - start_position;
                char* block_text;
                if (lexer->memory_arena) {
                    block_text = (char*)ijkl_memory_arena_allocate(lexer->memory_arena, raw_length + 1);
                } else {
                    block_text = (char*)ijkl_memory_heap_allocate(raw_length + 1);
                }
                memcpy(block_text, &lexer->source_code[start_position], raw_length);
                block_text[raw_length] = '\0';

                lexer_advance_character(lexer); /* Consome '}' */
                if (out_length) {
                    *out_length = raw_length;
                }
                return block_text;
            }
            lexer_advance_character(lexer);
            continue;
        }

        lexer_advance_character(lexer);
    }

    size_t raw_length = lexer->cursor_position - start_position;
    char* unclosed_text;
    if (lexer->memory_arena) {
        unclosed_text = (char*)ijkl_memory_arena_allocate(lexer->memory_arena, raw_length + 1);
    } else {
        unclosed_text = (char*)ijkl_memory_heap_allocate(raw_length + 1);
    }
    memcpy(unclosed_text, &lexer->source_code[start_position], raw_length);
    unclosed_text[raw_length] = '\0';
    if (out_length) {
        *out_length = raw_length;
    }
    return unclosed_text;
}
