#include "utf8.h"

uint32_t ijkl_utf8_decode_next_codepoint(const char** stream_cursor) {
    if (!stream_cursor || !*stream_cursor || !**stream_cursor) {
        return 0;
    }

    const uint8_t* source_bytes = (const uint8_t*)(*stream_cursor);
    uint32_t decoded_codepoint = 0;
    size_t sequence_length_in_bytes = 0;

    if (source_bytes[0] < 0x80) {
        /* Sequência de 1 byte: ASCII de 7 bits */
        decoded_codepoint = source_bytes[0];
        sequence_length_in_bytes = 1;
    } else if ((source_bytes[0] & 0xE0) == 0xC0) {
        /* Sequência de 2 bytes: 110xxxxx 10xxxxxx */
        if ((source_bytes[1] & 0xC0) != 0x80) {
            (*stream_cursor)++;
            return IJKL_UTF8_REPLACEMENT_CHARACTER;
        }
        decoded_codepoint = ((source_bytes[0] & 0x1F) << 6) | (source_bytes[1] & 0x3F);
        sequence_length_in_bytes = 2;
    } else if ((source_bytes[0] & 0xF0) == 0xE0) {
        /* Sequência de 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx */
        if ((source_bytes[1] & 0xC0) != 0x80 || (source_bytes[2] & 0xC0) != 0x80) {
            (*stream_cursor)++;
            return IJKL_UTF8_REPLACEMENT_CHARACTER;
        }
        decoded_codepoint = ((source_bytes[0] & 0x0F) << 12) |
                            ((source_bytes[1] & 0x3F) << 6) |
                            (source_bytes[2] & 0x3F);
        sequence_length_in_bytes = 3;
    } else if ((source_bytes[0] & 0xF8) == 0xF0) {
        /* Sequência de 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        if ((source_bytes[1] & 0xC0) != 0x80 ||
            (source_bytes[2] & 0xC0) != 0x80 ||
            (source_bytes[3] & 0xC0) != 0x80) {
            (*stream_cursor)++;
            return IJKL_UTF8_REPLACEMENT_CHARACTER;
        }
        decoded_codepoint = ((source_bytes[0] & 0x07) << 18) |
                            ((source_bytes[1] & 0x3F) << 12) |
                            ((source_bytes[2] & 0x3F) << 6) |
                            (source_bytes[3] & 0x3F);
        sequence_length_in_bytes = 4;
    } else {
        /* Byte inicial inválido para UTF-8 */
        (*stream_cursor)++;
        return IJKL_UTF8_REPLACEMENT_CHARACTER;
    }

    /* Valida surrogates do UTF-16 (0xD800 até 0xDFFF) e limite superior Unicode (0x10FFFF) */
    if ((decoded_codepoint >= 0xD800 && decoded_codepoint <= 0xDFFF) || decoded_codepoint > 0x10FFFF) {
        *stream_cursor += sequence_length_in_bytes;
        return IJKL_UTF8_REPLACEMENT_CHARACTER;
    }

    *stream_cursor += sequence_length_in_bytes;
    return decoded_codepoint;
}

size_t ijkl_utf8_encode_codepoint(uint32_t unicode_codepoint, char* destination_buffer) {
    if (!destination_buffer) return 0;
    uint8_t* target = (uint8_t*)destination_buffer;

    if (unicode_codepoint <= 0x7F) {
        target[0] = (uint8_t)unicode_codepoint;
        return 1;
    }
    if (unicode_codepoint <= 0x7FF) {
        target[0] = (uint8_t)(0xC0 | ((unicode_codepoint >> 6) & 0x1F));
        target[1] = (uint8_t)(0x80 | (unicode_codepoint & 0x3F));
        return 2;
    }
    if (unicode_codepoint <= 0xFFFF) {
        target[0] = (uint8_t)(0xE0 | ((unicode_codepoint >> 12) & 0x0F));
        target[1] = (uint8_t)(0x80 | ((unicode_codepoint >> 6) & 0x3F));
        target[2] = (uint8_t)(0x80 | (unicode_codepoint & 0x3F));
        return 3;
    }
    if (unicode_codepoint <= 0x10FFFF) {
        target[0] = (uint8_t)(0xF0 | ((unicode_codepoint >> 18) & 0x07));
        target[1] = (uint8_t)(0x80 | ((unicode_codepoint >> 12) & 0x3F));
        target[2] = (uint8_t)(0x80 | ((unicode_codepoint >> 6) & 0x3F));
        target[3] = (uint8_t)(0x80 | (unicode_codepoint & 0x3F));
        return 4;
    }

    return 0;
}

bool ijkl_utf8_is_valid_codepoint(uint32_t unicode_codepoint) {
    if (unicode_codepoint > 0x10FFFF) return false;
    if (unicode_codepoint >= 0xD800 && unicode_codepoint <= 0xDFFF) return false;
    return true;
}

uint32_t ijkl_utf8_codepoint_to_lowercase(uint32_t unicode_codepoint) {
    /* ASCII padrão (A-Z) */
    if (unicode_codepoint >= 'A' && unicode_codepoint <= 'Z') {
        return unicode_codepoint + ('a' - 'A');
    }

    /* Latin-1 Supplement: À (0xC0) até Ö (0xD6) */
    if (unicode_codepoint >= 0x00C0 && unicode_codepoint <= 0x00D6) {
        return unicode_codepoint + 0x20;
    }

    /* Latin-1 Supplement: Ø (0xD8) até Þ (0xDE) */
    if (unicode_codepoint >= 0x00D8 && unicode_codepoint <= 0x00DE) {
        return unicode_codepoint + 0x20;
    }

    return unicode_codepoint;
}

uint32_t ijkl_utf8_codepoint_to_uppercase(uint32_t unicode_codepoint) {
    /* ASCII padrão (a-z) */
    if (unicode_codepoint >= 'a' && unicode_codepoint <= 'z') {
        return unicode_codepoint - ('a' - 'A');
    }

    /* Latin-1 Supplement: à (0xE0) até ö (0xF6) */
    if (unicode_codepoint >= 0x00E0 && unicode_codepoint <= 0x00F6) {
        return unicode_codepoint - 0x20;
    }

    /* Latin-1 Supplement: ø (0xF8) até þ (0xFE) */
    if (unicode_codepoint >= 0x00F8 && unicode_codepoint <= 0x00FE) {
        return unicode_codepoint - 0x20;
    }

    return unicode_codepoint;
}

uint8_t ijkl_utf8_hash_codepoint(uint32_t unicode_codepoint) {
    /* Dispersão simples de 32 bits para tabela com 256 baldes */
    return (uint8_t)((unicode_codepoint ^ (unicode_codepoint >> 8) ^ (unicode_codepoint >> 16)) & 0xFF);
}
