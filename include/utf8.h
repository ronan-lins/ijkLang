#ifndef IJKL_UTF8_H
#define IJKL_UTF8_H

#include "common.h"

/* Codepoint sentinela utilizado para representar sequência UTF-8 malformada (Unicode Replacement Character) */
#define IJKL_UTF8_REPLACEMENT_CHARACTER 0xFFFD

/*
 * ==============================================================================
 * MÓDULO: UTF-8 E CARACTERES UNICODE
 * ==============================================================================
 * Fornece decodificação, codificação e normalização de fluxo de caracteres UTF-8.
 * ==============================================================================
 */

/*
 * Decodifica o próximo codepoint Unicode de uma sequência de bytes UTF-8.
 *
 * EFEITO COLATERAL / MUTAÇÃO DE PONTEIRO:
 * O ponteiro referenciado por 'stream_cursor' é incrementado pelo número de bytes
 * consumidos (entre 1 e 4 bytes se a sequência for válida, ou 1 byte em caso de
 * byte inválido).
 *
 * Retorna o codepoint Unicode de 32 bits ou IJKL_UTF8_REPLACEMENT_CHARACTER em caso de erro.
 */
uint32_t ijkl_utf8_decode_next_codepoint(const char** stream_cursor);

/*
 * Codifica um codepoint Unicode em sua representação de bytes UTF-8.
 *
 * O buffer de destino 'destination_buffer' deve ter capacidade para pelo menos 4 bytes.
 * Retorna a quantidade de bytes gravados (1 a 4).
 */
size_t ijkl_utf8_encode_codepoint(uint32_t unicode_codepoint, char* destination_buffer);

/*
 * Verifica se um codepoint de 32 bits está dentro do espaço Unicode válido (0..0x10FFFF, excluindo surrogates).
 */
bool ijkl_utf8_is_valid_codepoint(uint32_t unicode_codepoint);

/*
 * Converte um codepoint para sua forma minúscula, com suporte a ASCII e Latin-1 Supplement.
 */
uint32_t ijkl_utf8_codepoint_to_lowercase(uint32_t unicode_codepoint);

/*
 * Converte um codepoint para sua forma maiúscula, com suporte a ASCII e Latin-1 Supplement.
 */
uint32_t ijkl_utf8_codepoint_to_uppercase(uint32_t unicode_codepoint);

/*
 * Calcula o hash de 8 bits de um codepoint Unicode para indexação em tabelas de dispersão.
 */
uint8_t ijkl_utf8_hash_codepoint(uint32_t unicode_codepoint);

#endif /* IJKL_UTF8_H */
