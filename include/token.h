#ifndef IJKL_TOKEN_H
#define IJKL_TOKEN_H

#include "common.h"

/*
 * ==============================================================================
 * MÓDULO: TOKENS E LÉXICO DA IJKLANG
 * ==============================================================================
 * Define os tipos de símbolos léxicos, literais e operadores da linguagem ijkLang.
 * ==============================================================================
 */

/* Enumeração completa de tipos de tokens da ijkLang (em inglês com prefixo de módulo) */
typedef enum {
    /* Especiais */
    TOKEN_EOF = 0,
    TOKEN_ERROR,

    /* Identificadores e Literais */
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_REAL,
    TOKEN_FLOAT,
    TOKEN_CHAR,
    TOKEN_STRING,

    /* Palavras Reservadas: Controle de Fluxo e Decisão */
    TOKEN_KW_IF,
    TOKEN_KW_ELSE,
    TOKEN_KW_SWITCH,
    TOKEN_KW_CASE,
    TOKEN_KW_DEFAULT,

    /* Palavras Reservadas: Laços e Interrupção */
    TOKEN_KW_WHILE,
    TOKEN_KW_FOR,
    TOKEN_KW_FROM,
    TOKEN_KW_TO,
    TOKEN_KW_STEP,
    TOKEN_KW_REPEAT,
    TOKEN_KW_BREAK,
    TOKEN_KW_RETURN,

    /* Palavras Reservadas: Memória e E/S (Verbos Imperativos) */
    TOKEN_KW_ALLOC,
    TOKEN_KW_FREE,
    TOKEN_KW_READ,
    TOKEN_KW_WRITE,
    TOKEN_KW_WRITELN,

    /* Palavras Reservadas: Tipos de Dados e Estruturas */
    TOKEN_KW_INT,
    TOKEN_KW_REAL,
    TOKEN_KW_FLOAT,
    TOKEN_KW_CHAR,
    TOKEN_KW_STRING,
    TOKEN_KW_BOOL,
    TOKEN_KW_ARRAY,
    TOKEN_KW_STRUCT,
    TOKEN_KW_POINTER,
    TOKEN_KW_REF,
    TOKEN_KW_CONST,
    TOKEN_KW_TYPE,

    /* Palavras Reservadas: Literais Booleanos e Nulo */
    TOKEN_KW_TRUE,
    TOKEN_KW_FALSE,
    TOKEN_KW_NULL,

    /* Palavras Reservadas: Operadores Textuais */
    TOKEN_KW_AND,
    TOKEN_KW_OR,
    TOKEN_KW_NOT,
    TOKEN_KW_DIV,
    TOKEN_KW_MOD,

    /* Palavras Reservadas: Modularização, Unidades e Visibilidade */
    TOKEN_KW_UNIT,
    TOKEN_KW_PROGRAM,
    TOKEN_KW_IMPORT,
    TOKEN_KW_PUBLIC,
    TOKEN_KW_PRIVATE,
    TOKEN_KW_OF,
    TOKEN_KW_MAIN,
    TOKEN_KW_INITIALIZE,
    TOKEN_KW_FINALIZE,
    TOKEN_KW_ASM,

    /* Operadores e Delimitadores */
    TOKEN_AT,                    /* @  - Desreferenciação de ponteiro */
    TOKEN_DOLLAR,                /* $  - Custo de memória em bytes */
    TOKEN_AMPERSAND,             /* &  - Endereço de memória */
    TOKEN_DOT,                   /* .  - Acesso a membro / módulo */
    TOKEN_COMMA,                 /* ,  - Separador de lista */
    TOKEN_COLON,                 /* :  - Mapeamento tipo função / rótulo */
    TOKEN_SEMICOLON,             /* ;  - Terminador de instrução */
    TOKEN_LPAREN,                /* (  */
    TOKEN_RPAREN,                /* )  */
    TOKEN_LBRACKET,              /* [  */
    TOKEN_RBRACKET,              /* ]  */
    TOKEN_LBRACE,                /* {  */
    TOKEN_RBRACE,                /* }  */
    TOKEN_ASSIGN_PASCAL,         /* := - Atribuição alternativa */
    TOKEN_EQUAL,                 /* =  - Atribuição ou comparação com tolerância */
    TOKEN_EQUAL_STRICT,          /* == - Comparação estrita bit-a-bit */
    TOKEN_NOT_EQUAL,             /* != - Diferença estilo C */
    TOKEN_NOT_EQUAL_PASCAL,      /* <> - Diferença estilo Pascal */
    TOKEN_LESS,                  /* <  */
    TOKEN_GREATER,               /* >  */
    TOKEN_LESS_EQUAL,            /* <= */
    TOKEN_GREATER_EQUAL,         /* >= */
    TOKEN_PLUS,                  /* +  */
    TOKEN_MINUS,                 /* -  */
    TOKEN_STAR,                  /* *  */
    TOKEN_SLASH,                 /* /  */
    TOKEN_CARET,                 /* ^  - Potenciação */
    TOKEN_LOGICAL_AND,           /* && */
    TOKEN_LOGICAL_OR,            /* || */
    TOKEN_EXCLAMATION            /* !  - Negação lógica estilo C */
} IjklTokenType;

/* União para armazenamento dos valores literais decodificados do token */
typedef union IjklTokenValue {
    int64_t integer_value;
    double real_value;
    float float_value;
    char char_value;
    const char* string_value;
    bool boolean_value;
} IjklTokenValue;

/* Estrutura representativa de um Token emitido pelo analisador léxico */
typedef struct IjklToken {
    IjklTokenType type;
    const char* lexeme;             /* Texto literal original capturado no código-fonte */
    const char* normalized_lexeme;  /* Texto canonizado (minúsculo e sem diacríticos complementares) */
    size_t length;                  /* Comprimento em bytes do lexema original */
    size_t line_number;             /* Número da linha (iniciando em 1) */
    size_t column_number;           /* Número da coluna (iniciando em 1) */
    IjklTokenValue value;           /* Valor semântico convertido (para literais e erros) */
} IjklToken;

/* Retorna o nome textual legível de um IjklTokenType para exibição e depuração */
const char* ijkl_token_type_name(IjklTokenType token_type);

/* Imprime a representação formatada de um Token no fluxo de saída especificado */
void ijkl_token_print(const IjklToken* token, FILE* output_stream);

/* Retorna verdadeiro se o token for uma palavra-chave reservada */
bool ijkl_token_type_is_keyword(IjklTokenType token_type);

/* Retorna verdadeiro se o token for um literal numérico, caractere ou texto */
bool ijkl_token_type_is_literal(IjklTokenType token_type);

#endif /* IJKL_TOKEN_H */
