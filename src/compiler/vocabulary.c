#include "vocabulary.h"

/* Tabela de correspondência canônica entre nomes em arquivos .words e IjklTokenType */
typedef struct CanonicalKeywordPair {
    const char* canonical_name;
    IjklTokenType token_type;
} CanonicalKeywordPair;

static const CanonicalKeywordPair CANONICAL_KEYWORD_TABLE[] = {
    {"KW_IF",         TOKEN_KW_IF},
    {"KW_ELSE",       TOKEN_KW_ELSE},
    {"KW_SWITCH",     TOKEN_KW_SWITCH},
    {"KW_CASE",       TOKEN_KW_CASE},
    {"KW_DEFAULT",    TOKEN_KW_DEFAULT},
    {"KW_WHILE",      TOKEN_KW_WHILE},
    {"KW_FOR",        TOKEN_KW_FOR},
    {"KW_FROM",       TOKEN_KW_FROM},
    {"KW_TO",         TOKEN_KW_TO},
    {"KW_STEP",       TOKEN_KW_STEP},
    {"KW_REPEAT",     TOKEN_KW_REPEAT},
    {"KW_BREAK",      TOKEN_KW_BREAK},
    {"KW_RETURN",     TOKEN_KW_RETURN},
    {"KW_ALLOC",      TOKEN_KW_ALLOC},
    {"KW_FREE",       TOKEN_KW_FREE},
    {"KW_READ",       TOKEN_KW_READ},
    {"KW_WRITE",      TOKEN_KW_WRITE},
    {"KW_WRITELN",    TOKEN_KW_WRITELN},
    {"KW_INT",        TOKEN_KW_INT},
    {"KW_REAL",       TOKEN_KW_REAL},
    {"KW_FLOAT",      TOKEN_KW_FLOAT},
    {"KW_CHAR",       TOKEN_KW_CHAR},
    {"KW_STRING",     TOKEN_KW_STRING},
    {"KW_BOOL",       TOKEN_KW_BOOL},
    {"KW_ARRAY",      TOKEN_KW_ARRAY},
    {"KW_STRUCT",     TOKEN_KW_STRUCT},
    {"KW_POINTER",    TOKEN_KW_POINTER},
    {"KW_REF",        TOKEN_KW_REF},
    {"KW_CONST",      TOKEN_KW_CONST},
    {"KW_TYPE",       TOKEN_KW_TYPE},
    {"KW_TRUE",       TOKEN_KW_TRUE},
    {"KW_FALSE",      TOKEN_KW_FALSE},
    {"KW_NULL",       TOKEN_KW_NULL},
    {"KW_AND",        TOKEN_KW_AND},
    {"KW_OR",         TOKEN_KW_OR},
    {"KW_NOT",        TOKEN_KW_NOT},
    {"KW_DIV",        TOKEN_KW_DIV},
    {"KW_MOD",        TOKEN_KW_MOD},
    {"KW_UNIT",       TOKEN_KW_UNIT},
    {"KW_PROGRAM",    TOKEN_KW_PROGRAM},
    {"KW_IMPORT",     TOKEN_KW_IMPORT},
    {"KW_PUBLIC",     TOKEN_KW_PUBLIC},
    {"KW_PRIVATE",    TOKEN_KW_PRIVATE},
    {"KW_OF",         TOKEN_KW_OF},
    {"KW_MAIN",       TOKEN_KW_MAIN},
    {"KW_INITIALIZE", TOKEN_KW_INITIALIZE},
    {"KW_FINALIZE",   TOKEN_KW_FINALIZE},
    {"KW_ASM",        TOKEN_KW_ASM},
    {NULL,            TOKEN_EOF}
};

/* Dispersão FNV-1a para cadeias de caracteres */
static uint8_t calculate_string_hash_bucket(const char* string_to_hash) {
    uint32_t hash_accumulator = 2166136261u;
    const uint8_t* byte_cursor = (const uint8_t*)string_to_hash;
    while (*byte_cursor) {
        hash_accumulator ^= *byte_cursor++;
        hash_accumulator *= 16777619u;
    }
    return (uint8_t)(hash_accumulator & 0xFF);
}

static char* trim_string_whitespace(char* string_to_trim) {
    if (!string_to_trim) return NULL;
    while (*string_to_trim && isspace((unsigned char)*string_to_trim)) {
        string_to_trim++;
    }
    if (!*string_to_trim) return string_to_trim;

    char* end_cursor = string_to_trim + strlen(string_to_trim) - 1;
    while (end_cursor > string_to_trim && isspace((unsigned char)*end_cursor)) {
        *end_cursor = '\0';
        end_cursor--;
    }
    return string_to_trim;
}

IjklTokenType ijkl_vocabulary_find_canonical_keyword_token(const char* canonical_keyword_name) {
    if (!canonical_keyword_name) return TOKEN_EOF;
    for (size_t index = 0; CANONICAL_KEYWORD_TABLE[index].canonical_name != NULL; index++) {
        if (strcmp(CANONICAL_KEYWORD_TABLE[index].canonical_name, canonical_keyword_name) == 0) {
            return CANONICAL_KEYWORD_TABLE[index].token_type;
        }
    }
    return TOKEN_EOF;
}

IjklVocabulary* ijkl_vocabulary_create(IjklMemoryArena* memory_arena) {
    IjklVocabulary* vocabulary;
    if (memory_arena) {
        vocabulary = (IjklVocabulary*)ijkl_memory_arena_allocate(memory_arena, sizeof(IjklVocabulary));
    } else {
        vocabulary = (IjklVocabulary*)ijkl_memory_heap_allocate(sizeof(IjklVocabulary));
    }

    memset(vocabulary, 0, sizeof(IjklVocabulary));
    vocabulary->memory_arena = memory_arena;
    return vocabulary;
}

static void vocabulary_add_char_map_single(IjklVocabulary* vocabulary, uint32_t unicode_codepoint, char ascii_base_character) {
    uint8_t hash_bucket = ijkl_utf8_hash_codepoint(unicode_codepoint);

    for (IjklCharMapEntry* entry = vocabulary->complementary_character_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (entry->unicode_codepoint == unicode_codepoint) {
            entry->ascii_base_character = ascii_base_character;
            return;
        }
    }

    IjklCharMapEntry* new_entry;
    if (vocabulary->memory_arena) {
        new_entry = (IjklCharMapEntry*)ijkl_memory_arena_allocate(vocabulary->memory_arena, sizeof(IjklCharMapEntry));
    } else {
        new_entry = (IjklCharMapEntry*)ijkl_memory_heap_allocate(sizeof(IjklCharMapEntry));
    }

    new_entry->unicode_codepoint = unicode_codepoint;
    new_entry->ascii_base_character = ascii_base_character;
    new_entry->next_entry = vocabulary->complementary_character_map[hash_bucket];
    vocabulary->complementary_character_map[hash_bucket] = new_entry;
    vocabulary->total_complementary_characters++;
}

void ijkl_vocabulary_add_complementary_char(IjklVocabulary* vocabulary, uint32_t unicode_codepoint, char ascii_base_character) {
    if (!vocabulary || unicode_codepoint == 0) return;

    /* Registra variante minúscula */
    uint32_t lowercase_codepoint = ijkl_utf8_codepoint_to_lowercase(unicode_codepoint);
    char lowercase_ascii = (char)tolower((unsigned char)ascii_base_character);
    vocabulary_add_char_map_single(vocabulary, lowercase_codepoint, lowercase_ascii);

    /* Registra variante maiúscula */
    uint32_t uppercase_codepoint = ijkl_utf8_codepoint_to_uppercase(unicode_codepoint);
    char uppercase_ascii = (char)toupper((unsigned char)ascii_base_character);
    vocabulary_add_char_map_single(vocabulary, uppercase_codepoint, uppercase_ascii);
}

void ijkl_vocabulary_add_reserved_word(IjklVocabulary* vocabulary, const char* normalized_word, IjklTokenType token_type) {
    if (!vocabulary || !normalized_word || !*normalized_word) return;

    uint8_t hash_bucket = calculate_string_hash_bucket(normalized_word);
    for (IjklKeywordEntry* entry = vocabulary->reserved_words_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->normalized_word, normalized_word) == 0) {
            entry->token_type = token_type;
            return;
        }
    }

    IjklKeywordEntry* new_entry;
    char* stored_word;
    if (vocabulary->memory_arena) {
        new_entry = (IjklKeywordEntry*)ijkl_memory_arena_allocate(vocabulary->memory_arena, sizeof(IjklKeywordEntry));
        stored_word = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, normalized_word);
    } else {
        new_entry = (IjklKeywordEntry*)ijkl_memory_heap_allocate(sizeof(IjklKeywordEntry));
        stored_word = ijkl_strdup(normalized_word);
    }

    new_entry->normalized_word = stored_word;
    new_entry->token_type = token_type;
    new_entry->next_entry = vocabulary->reserved_words_map[hash_bucket];
    vocabulary->reserved_words_map[hash_bucket] = new_entry;
    vocabulary->total_reserved_words++;
}

static uint8_t calculate_alias_hash_bucket(const char* name) {
    if (!name) return 0;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; name[i] != '\0'; ++i) {
        hash ^= (uint8_t)name[i];
        hash *= 16777619u;
    }
    return (uint8_t)(hash & 0xFF);
}

void ijkl_vocabulary_add_framework_unit(IjklVocabulary* vocabulary,
                                       const char* source_alias_name,
                                       const char* target_canonical_name) {
    if (!vocabulary || !source_alias_name || !target_canonical_name) return;

    uint8_t hash_bucket = calculate_alias_hash_bucket(source_alias_name);
    for (IjklFrameworkAliasEntry* entry = vocabulary->framework_units_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->source_alias_name, source_alias_name) == 0) {
            entry->target_canonical_name = vocabulary->memory_arena ?
                ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, target_canonical_name) :
                ijkl_strdup(target_canonical_name);
            return;
        }
    }

    IjklFrameworkAliasEntry* new_entry;
    if (vocabulary->memory_arena) {
        new_entry = (IjklFrameworkAliasEntry*)ijkl_memory_arena_allocate(
            vocabulary->memory_arena, sizeof(IjklFrameworkAliasEntry));
        new_entry->source_alias_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, source_alias_name);
        new_entry->target_canonical_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, target_canonical_name);
    } else {
        new_entry = (IjklFrameworkAliasEntry*)ijkl_memory_heap_allocate(sizeof(IjklFrameworkAliasEntry));
        new_entry->source_alias_name = ijkl_strdup(source_alias_name);
        new_entry->target_canonical_name = ijkl_strdup(target_canonical_name);
    }

    new_entry->next_entry = vocabulary->framework_units_map[hash_bucket];
    vocabulary->framework_units_map[hash_bucket] = new_entry;
    vocabulary->total_framework_units_count++;
}

const char* ijkl_vocabulary_resolve_framework_unit(const IjklVocabulary* vocabulary,
                                                  const char* candidate_unit_name) {
    if (!vocabulary || !candidate_unit_name || !*candidate_unit_name) return candidate_unit_name;

    uint8_t hash_bucket = calculate_alias_hash_bucket(candidate_unit_name);
    for (const IjklFrameworkAliasEntry* entry = vocabulary->framework_units_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->source_alias_name, candidate_unit_name) == 0) {
            return entry->target_canonical_name;
        }
    }
    return candidate_unit_name;
}

void ijkl_vocabulary_add_framework_member(IjklVocabulary* vocabulary,
                                         const char* source_alias_name,
                                         const char* target_canonical_name) {
    if (!vocabulary || !source_alias_name || !target_canonical_name) return;

    uint8_t hash_bucket = calculate_alias_hash_bucket(source_alias_name);
    for (IjklFrameworkAliasEntry* entry = vocabulary->framework_members_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->source_alias_name, source_alias_name) == 0) {
            entry->target_canonical_name = vocabulary->memory_arena ?
                ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, target_canonical_name) :
                ijkl_strdup(target_canonical_name);
            return;
        }
    }

    IjklFrameworkAliasEntry* new_entry;
    if (vocabulary->memory_arena) {
        new_entry = (IjklFrameworkAliasEntry*)ijkl_memory_arena_allocate(
            vocabulary->memory_arena, sizeof(IjklFrameworkAliasEntry));
        new_entry->source_alias_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, source_alias_name);
        new_entry->target_canonical_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, target_canonical_name);
    } else {
        new_entry = (IjklFrameworkAliasEntry*)ijkl_memory_heap_allocate(sizeof(IjklFrameworkAliasEntry));
        new_entry->source_alias_name = ijkl_strdup(source_alias_name);
        new_entry->target_canonical_name = ijkl_strdup(target_canonical_name);
    }

    new_entry->next_entry = vocabulary->framework_members_map[hash_bucket];
    vocabulary->framework_members_map[hash_bucket] = new_entry;
    vocabulary->total_framework_members_count++;
}

const char* ijkl_vocabulary_resolve_framework_member(const IjklVocabulary* vocabulary,
                                                    const char* candidate_member_name) {
    if (!vocabulary || !candidate_member_name || !*candidate_member_name) return candidate_member_name;

    uint8_t hash_bucket = calculate_alias_hash_bucket(candidate_member_name);
    for (const IjklFrameworkAliasEntry* entry = vocabulary->framework_members_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->source_alias_name, candidate_member_name) == 0) {
            return entry->target_canonical_name;
        }
    }
    return candidate_member_name;
}

char ijkl_vocabulary_map_codepoint_to_ascii(const IjklVocabulary* vocabulary, uint32_t unicode_codepoint) {
    if (unicode_codepoint >= 'a' && unicode_codepoint <= 'z') return (char)unicode_codepoint;
    if (unicode_codepoint >= 'A' && unicode_codepoint <= 'Z') return (char)unicode_codepoint;
    if (unicode_codepoint >= '0' && unicode_codepoint <= '9') return (char)unicode_codepoint;
    if (unicode_codepoint == '_') return '_';

    if (!vocabulary) return '\0';

    uint8_t hash_bucket = ijkl_utf8_hash_codepoint(unicode_codepoint);
    for (const IjklCharMapEntry* entry = vocabulary->complementary_character_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (entry->unicode_codepoint == unicode_codepoint) {
            return entry->ascii_base_character;
        }
    }

    /* Fallback para maiúsculas Latin-1 */
    uint32_t lowercase_codepoint = ijkl_utf8_codepoint_to_lowercase(unicode_codepoint);
    if (lowercase_codepoint != unicode_codepoint) {
        uint8_t lowercase_bucket = ijkl_utf8_hash_codepoint(lowercase_codepoint);
        for (const IjklCharMapEntry* entry = vocabulary->complementary_character_map[lowercase_bucket];
             entry != NULL;
             entry = entry->next_entry) {
            if (entry->unicode_codepoint == lowercase_codepoint) {
                return (char)toupper((unsigned char)entry->ascii_base_character);
            }
        }
    }

    return '\0';
}

char* ijkl_vocabulary_normalize_string(const IjklVocabulary* vocabulary,
                                      IjklMemoryArena* destination_arena,
                                      const char* source_string,
                                      size_t source_length) {
    if (!source_string) return NULL;

    size_t maximum_buffer_size = (source_length > 0 ? source_length : strlen(source_string)) + 1;
    char* temporary_buffer = (char*)ijkl_memory_heap_allocate(maximum_buffer_size);
    size_t output_index = 0;

    const char* stream_cursor = source_string;
    const char* stream_end = source_length > 0 ? source_string + source_length : NULL;

    while (*stream_cursor && (!stream_end || stream_cursor < stream_end)) {
        uint32_t unicode_codepoint = ijkl_utf8_decode_next_codepoint(&stream_cursor);
        if (unicode_codepoint == 0) break;

        char mapped_ascii_character = ijkl_vocabulary_map_codepoint_to_ascii(vocabulary, unicode_codepoint);
        if (mapped_ascii_character != '\0') {
            temporary_buffer[output_index++] = mapped_ascii_character;
        } else if (unicode_codepoint < 128) {
            temporary_buffer[output_index++] = (char)unicode_codepoint;
        }
    }
    temporary_buffer[output_index] = '\0';

    char* final_normalized_string;
    if (destination_arena) {
        final_normalized_string = ijkl_memory_arena_duplicate_string_slice(destination_arena, temporary_buffer, output_index);
        ijkl_memory_heap_free(temporary_buffer);
    } else {
        final_normalized_string = temporary_buffer;
    }

    return final_normalized_string;
}

bool ijkl_vocabulary_lookup_keyword(const IjklVocabulary* vocabulary,
                                   const char* normalized_lexeme,
                                   IjklTokenType* out_token_type) {
    if (!vocabulary || !normalized_lexeme || !*normalized_lexeme) return false;

    uint8_t hash_bucket = calculate_string_hash_bucket(normalized_lexeme);
    for (const IjklKeywordEntry* entry = vocabulary->reserved_words_map[hash_bucket];
         entry != NULL;
         entry = entry->next_entry) {
        if (strcmp(entry->normalized_word, normalized_lexeme) == 0) {
            if (out_token_type) {
                *out_token_type = entry->token_type;
            }
            return true;
        }
    }

    return false;
}

bool ijkl_vocabulary_load_file(IjklVocabulary* vocabulary, const char* words_file_path) {
    if (!vocabulary || !words_file_path) return false;

    FILE* words_file = fopen(words_file_path, "r");
    if (!words_file) return false;

    char line_buffer[1024];
    int current_active_section = 0; /* 1 = [complementary_alphabet], 2 = [reserved_words] */

    while (fgets(line_buffer, sizeof(line_buffer), words_file)) {
        /* Trata e descarta comentários iniciados com ';' ou '#' */
        char* comment_start = strpbrk(line_buffer, ";#");
        if (comment_start) {
            *comment_start = '\0';
        }

        char* trimmed_line = trim_string_whitespace(line_buffer);
        if (!*trimmed_line) continue;

        /* Identificação de seções [secao] */
        if (trimmed_line[0] == '[' && trimmed_line[strlen(trimmed_line) - 1] == ']') {
            trimmed_line[strlen(trimmed_line) - 1] = '\0';
            char* section_name = trim_string_whitespace(trimmed_line + 1);

            if (strcmp(section_name, "complementary_alphabet") == 0 ||
                strcmp(section_name, "alfabeto_complementar") == 0) {
                current_active_section = 1;
            } else if (strcmp(section_name, "reserved_words") == 0 ||
                       strcmp(section_name, "palavras_reservadas") == 0) {
                current_active_section = 2;
            } else if (strcmp(section_name, "framework_units") == 0 ||
                       strcmp(section_name, "unidades_framework") == 0) {
                current_active_section = 3;
            } else if (strcmp(section_name, "framework_members") == 0 ||
                       strcmp(section_name, "membros_framework") == 0) {
                current_active_section = 4;
            } else {
                current_active_section = 0;
            }
            continue;
        }

        char* equals_separator = strchr(trimmed_line, '=');
        if (!equals_separator) continue;
        *equals_separator = '\0';

        char* entry_key = trim_string_whitespace(trimmed_line);
        char* entry_value = trim_string_whitespace(equals_separator + 1);

        if (current_active_section == 1) {
            /* Alfabeto complementar: chave = caractere base ASCII; valor = lista de caracteres acentuados */
            if (strlen(entry_key) == 1) {
                char ascii_base_char = entry_key[0];
                const char* value_cursor = entry_value;

                while (*value_cursor) {
                    while (*value_cursor && isspace((unsigned char)*value_cursor)) value_cursor++;
                    if (!*value_cursor) break;

                    uint32_t unicode_codepoint = ijkl_utf8_decode_next_codepoint(&value_cursor);
                    if (unicode_codepoint != 0 && unicode_codepoint != IJKL_UTF8_REPLACEMENT_CHARACTER) {
                        ijkl_vocabulary_add_complementary_char(vocabulary, unicode_codepoint, ascii_base_char);
                    }
                }
            }
        } else if (current_active_section == 2) {
            /* Palavras reservadas: chave = KW_...; valor = uma ou mais palavras separadas por espaço */
            IjklTokenType token_type = ijkl_vocabulary_find_canonical_keyword_token(entry_key);
            if (token_type != TOKEN_EOF) {
                if (strcmp(entry_key, "KW_MAIN") == 0 && entry_value && *entry_value) {
                    char entry_name_copy[64];
                    strncpy(entry_name_copy, entry_value, sizeof(entry_name_copy) - 1);
                    entry_name_copy[sizeof(entry_name_copy) - 1] = '\0';
                    char* space_delimiter = strpbrk(entry_name_copy, " \t\r\n");
                    if (space_delimiter) {
                        *space_delimiter = '\0';
                    }
                    char* normalized_entry_name = ijkl_vocabulary_normalize_string(vocabulary,
                                                                                  vocabulary->memory_arena,
                                                                                  entry_name_copy,
                                                                                  strlen(entry_name_copy));
                    if (vocabulary->memory_arena) {
                        vocabulary->entry_point_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, normalized_entry_name);
                    } else {
                        vocabulary->entry_point_name = ijkl_strdup(normalized_entry_name);
                    }
                } else if (strcmp(entry_key, "KW_INITIALIZE") == 0 && entry_value && *entry_value) {
                    char init_name_copy[64];
                    strncpy(init_name_copy, entry_value, sizeof(init_name_copy) - 1);
                    init_name_copy[sizeof(init_name_copy) - 1] = '\0';
                    char* space_delimiter = strpbrk(init_name_copy, " \t\r\n");
                    if (space_delimiter) {
                        *space_delimiter = '\0';
                    }
                    char* normalized_init_name = ijkl_vocabulary_normalize_string(vocabulary,
                                                                                 vocabulary->memory_arena,
                                                                                 init_name_copy,
                                                                                 strlen(init_name_copy));
                    if (vocabulary->memory_arena) {
                        vocabulary->initialize_routine_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, normalized_init_name);
                    } else {
                        vocabulary->initialize_routine_name = ijkl_strdup(normalized_init_name);
                    }
                } else if (strcmp(entry_key, "KW_FINALIZE") == 0 && entry_value && *entry_value) {
                    char fin_name_copy[64];
                    strncpy(fin_name_copy, entry_value, sizeof(fin_name_copy) - 1);
                    fin_name_copy[sizeof(fin_name_copy) - 1] = '\0';
                    char* space_delimiter = strpbrk(fin_name_copy, " \t\r\n");
                    if (space_delimiter) {
                        *space_delimiter = '\0';
                    }
                    char* normalized_fin_name = ijkl_vocabulary_normalize_string(vocabulary,
                                                                                vocabulary->memory_arena,
                                                                                fin_name_copy,
                                                                                strlen(fin_name_copy));
                    if (vocabulary->memory_arena) {
                        vocabulary->finalize_routine_name = ijkl_memory_arena_duplicate_string(vocabulary->memory_arena, normalized_fin_name);
                    } else {
                        vocabulary->finalize_routine_name = ijkl_strdup(normalized_fin_name);
                    }
                }

                char* single_word = strtok(entry_value, " \t\r\n");
                while (single_word) {
                    char* normalized_word = ijkl_vocabulary_normalize_string(vocabulary,
                                                                            vocabulary->memory_arena,
                                                                            single_word,
                                                                            strlen(single_word));
                    if (normalized_word && *normalized_word) {
                        ijkl_vocabulary_add_reserved_word(vocabulary, normalized_word, token_type);
                    }
                    single_word = strtok(NULL, " \t\r\n");
                }
            }
        } else if (current_active_section == 3) {
            /* Mapeamento de unidades do framework: alias = canonico */
            if (entry_key && *entry_key && entry_value && *entry_value) {
                ijkl_vocabulary_add_framework_unit(vocabulary, entry_key, entry_value);
                ijkl_vocabulary_add_framework_unit(vocabulary, entry_value, entry_value);
            }
        } else if (current_active_section == 4) {
            /* Mapeamento de membros do framework: alias = canonico */
            if (entry_key && *entry_key && entry_value && *entry_value) {
                ijkl_vocabulary_add_framework_member(vocabulary, entry_key, entry_value);
                ijkl_vocabulary_add_framework_member(vocabulary, entry_value, entry_value);
            }
        }
    }

    fclose(words_file);
    return true;
}

const char* ijkl_vocabulary_get_entry_point_name(const IjklVocabulary* vocabulary) {
    return vocabulary ? vocabulary->entry_point_name : NULL;
}

const char* ijkl_vocabulary_get_initialize_name(const IjklVocabulary* vocabulary) {
    return vocabulary ? vocabulary->initialize_routine_name : NULL;
}

const char* ijkl_vocabulary_get_finalize_name(const IjklVocabulary* vocabulary) {
    return vocabulary ? vocabulary->finalize_routine_name : NULL;
}

const char* ijkl_vocabulary_get_keyword_word(const IjklVocabulary* vocabulary, IjklTokenType token_type) {
    if (!vocabulary) return NULL;

    for (size_t bucket_index = 0; bucket_index < 256; bucket_index++) {
        for (const IjklKeywordEntry* entry = vocabulary->reserved_words_map[bucket_index];
             entry != NULL;
             entry = entry->next_entry) {
            if (entry->token_type == token_type) {
                return entry->normalized_word;
            }
        }
    }

    return NULL;
}

bool ijkl_vocabulary_is_obsolete_keyword(const char* candidate_word,
                                         char* out_obsolete_info,
                                         size_t max_info_length) {
    if (!candidate_word || !*candidate_word) return false;

    typedef struct ObsoleteKeywordDef {
        const char* keyword_name;
        const char* diagnostic_message;
    } ObsoleteKeywordDef;

    static const ObsoleteKeywordDef OBSOLETE_KEYWORDS[] = {
        {"inicio", "O delimitador 'inicio' e obsoleto. A sintaxe de blocos da ijkLang utiliza exclusivamente '{' e '}'."},
        {"fim", "O delimitador 'fim' e obsoleto. A sintaxe de blocos da ijkLang utiliza exclusivamente '{' e '}'."},
        {"fimse", "O fechamento 'fimse' e obsoleto. Utilize '{' e '}' para delimitar o bloco condicional."},
        {"fimpara", "O fechamento 'fimpara' e obsoleto. Utilize '{' e '}' para delimitar o corpo do laco."},
        {"fimenquanto", "O fechamento 'fimenquanto' e obsoleto. Utilize '{' e '}' para delimitar o corpo do laco."},
        {"entao", "A palavra 'entao' e obsoleta. Utilize a condicao entre parenteses seguida por '{'."},
        {"faca", "A palavra 'faca' e obsoleta. Utilize a condicao ou cabecalho seguido por '{'."},
        {"funcao", "A palavra 'funcao' e obsoleta na ijkLang. Declare rotinas diretamente como 'nome(parametros): tipo { ... }'."},
        {"procedimento", "A palavra 'procedimento' e obsoleta na ijkLang. Declare rotinas sem tipo de retorno: 'nome(parametros) { ... }'."},
        {NULL, NULL}
    };

    for (size_t index = 0; OBSOLETE_KEYWORDS[index].keyword_name != NULL; index++) {
        if (ijkl_string_case_insensitive_equals(candidate_word, OBSOLETE_KEYWORDS[index].keyword_name)) {
            if (out_obsolete_info && max_info_length > 0) {
                strncpy(out_obsolete_info, OBSOLETE_KEYWORDS[index].diagnostic_message, max_info_length - 1);
                out_obsolete_info[max_info_length - 1] = '\0';
            }
            return true;
        }
    }

    return false;
}
