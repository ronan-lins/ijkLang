#include "config.h"

static char* trim_config_whitespace(char* string_to_trim) {
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

void ijkl_config_init_defaults(IjklCompilerConfig* compiler_config) {
    if (!compiler_config) return;
    memset(compiler_config, 0, sizeof(IjklCompilerConfig));
    strncpy(compiler_config->active_language, "pt-BR", sizeof(compiler_config->active_language) - 1);
    strncpy(compiler_config->keywords_directory, "keywords", sizeof(compiler_config->keywords_directory) - 1);
    strncpy(compiler_config->framework_directory, "lib", sizeof(compiler_config->framework_directory) - 1);
    compiler_config->is_strict_mode = false;
}

bool ijkl_config_load_from_file(IjklCompilerConfig* compiler_config,
                               const char* ini_file_path,
                               const char* executable_directory) {
    if (!compiler_config) return false;
    ijkl_config_init_defaults(compiler_config);

    char full_resolved_ini_path[1024];
    FILE* ini_file = NULL;

    if (ini_file_path && *ini_file_path) {
        ini_file = fopen(ini_file_path, "r");
    }

    /* Caso não encontre no caminho direto, tenta buscar junto ao diretório do executável */
    if (!ini_file && executable_directory && *executable_directory && ini_file_path) {
        snprintf(full_resolved_ini_path, sizeof(full_resolved_ini_path), "%s%c%s",
                 executable_directory, IJKL_PATH_SEPARATOR, ini_file_path);
        ini_file = fopen(full_resolved_ini_path, "r");
    }

    if (!ini_file) {
        /* Fallback: se o arquivo não existe, mantém as opções padrão */
        return false;
    }

    char line_buffer[1024];
    char current_section_name[64] = "";

    while (fgets(line_buffer, sizeof(line_buffer), ini_file)) {
        char* comment_start = strpbrk(line_buffer, ";#");
        if (comment_start) {
            *comment_start = '\0';
        }

        char* trimmed_line = trim_config_whitespace(line_buffer);
        if (!*trimmed_line) continue;

        if (trimmed_line[0] == '[' && trimmed_line[strlen(trimmed_line) - 1] == ']') {
            trimmed_line[strlen(trimmed_line) - 1] = '\0';
            strncpy(current_section_name, trim_config_whitespace(trimmed_line + 1), sizeof(current_section_name) - 1);
            continue;
        }

        char* equals_separator = strchr(trimmed_line, '=');
        if (!equals_separator) continue;
        *equals_separator = '\0';

        char* config_key = trim_config_whitespace(trimmed_line);
        char* config_value = trim_config_whitespace(equals_separator + 1);

        if (strcmp(current_section_name, "general") == 0 || strcmp(current_section_name, "geral") == 0) {
            if (strcmp(config_key, "language") == 0 || strcmp(config_key, "idioma") == 0) {
                strncpy(compiler_config->active_language, config_value, sizeof(compiler_config->active_language) - 1);
            }
        } else if (strcmp(current_section_name, "directories") == 0 || strcmp(current_section_name, "diretorios") == 0) {
            if (strcmp(config_key, "keywords") == 0) {
                strncpy(compiler_config->keywords_directory, config_value, sizeof(compiler_config->keywords_directory) - 1);
            } else if (strcmp(config_key, "framework") == 0 || strcmp(config_key, "biblioteca") == 0) {
                strncpy(compiler_config->framework_directory, config_value, sizeof(compiler_config->framework_directory) - 1);
            }
        } else if (strcmp(current_section_name, "compiler") == 0 || strcmp(current_section_name, "compilador") == 0) {
            if (strcmp(config_key, "strict") == 0 || strcmp(config_key, "estrito") == 0) {
                compiler_config->is_strict_mode = (strcmp(config_value, "true") == 0 || strcmp(config_value, "1") == 0);
            }
        }
    }

    fclose(ini_file);
    return true;
}
