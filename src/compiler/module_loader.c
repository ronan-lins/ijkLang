#define _CRT_SECURE_NO_WARNINGS
#include "module_loader.h"
#include "lexer.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define strtok_s(str, delim, ctx) strtok_r(str, delim, ctx)
#endif

/*
 * EFEITO COLATERAL:
 * Lê integralmente um arquivo em disco e retorna o buffer alocado na arena.
 */
static char* read_file_content_to_buffer(IjklMemoryArena* memory_arena,
                                         const char* file_path,
                                         size_t* out_file_size) {
    if (!memory_arena || !file_path) return NULL;

    FILE* file_handle = fopen(file_path, "rb");
    if (!file_handle) return NULL;

    fseek(file_handle, 0, SEEK_END);
    long file_size = ftell(file_handle);
    fseek(file_handle, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file_handle);
        return NULL;
    }

    size_t allocation_size = (size_t)file_size;
    char* buffer = (char*)ijkl_memory_arena_allocate(memory_arena, allocation_size + 1);
    size_t read_bytes = fread(buffer, 1, allocation_size, file_handle);
    buffer[read_bytes] = '\0';
    fclose(file_handle);

    /* Tratamento transparente de marca de ordem de byte UTF-8 (BOM: 0xEF, 0xBB, 0xBF) */
    if (read_bytes >= 3 &&
        (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB &&
        (unsigned char)buffer[2] == 0xBF) {
        memmove(buffer, buffer + 3, read_bytes - 3 + 1);
        read_bytes -= 3;
    }

    if (out_file_size) {
        *out_file_size = read_bytes;
    }
    return buffer;
}

/*
 * Normaliza separadores de caminho convertendo barras invertidas em barras normais.
 */
static void normalize_path_separators(char* path_buffer) {
    if (!path_buffer) return;
    for (size_t i = 0; path_buffer[i] != '\0'; ++i) {
        if (path_buffer[i] == '\\') {
            path_buffer[i] = '/';
        }
    }
}

/*
 * EFEITO COLATERAL:
 * Resolve um caminho relativo de forma insensível a maiúsculas e minúsculas,
 * navegando diretório a diretório e verificando a existência no sistema de arquivos.
 * Suporta Windows e POSIX (Linux/macOS).
 */
static bool resolve_case_insensitive_relative_path(const char* base_directory,
                                                   const char* relative_path,
                                                   char* output_resolved_path,
                                                   size_t output_capacity,
                                                   bool* out_is_ambiguous) {
    if (out_is_ambiguous) *out_is_ambiguous = false;
    if (!relative_path || !output_resolved_path || output_capacity == 0) return false;

    char current_path[4096];
    if (base_directory && *base_directory) {
        snprintf(current_path, sizeof(current_path), "%s", base_directory);
    } else {
        snprintf(current_path, sizeof(current_path), ".");
    }
    normalize_path_separators(current_path);

    /* Remove barra final se presente no diretório base */
    size_t current_len = strlen(current_path);
    while (current_len > 0 && current_path[current_len - 1] == '/') {
        current_path[--current_len] = '\0';
    }

    char normalized_relative[4096];
    snprintf(normalized_relative, sizeof(normalized_relative), "%s", relative_path);
    normalize_path_separators(normalized_relative);

    char* context_token = NULL;
    char* segment = strtok_s(normalized_relative, "/", &context_token);

    while (segment != NULL) {
        int matching_entries_count = 0;
        char matched_entry_name[256] = "";

#if defined(_WIN32) || defined(_WIN64)
        char search_pattern[4096];
        snprintf(search_pattern, sizeof(search_pattern), "%s/*", current_path);
        WIN32_FIND_DATAA find_data;
        HANDLE find_handle = FindFirstFileA(search_pattern, &find_data);

        if (find_handle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                    continue;
                }
                if (ijkl_string_case_insensitive_equals(find_data.cFileName, segment)) {
                    matching_entries_count++;
                    snprintf(matched_entry_name, sizeof(matched_entry_name), "%s", find_data.cFileName);
                }
            } while (FindNextFileA(find_handle, &find_data));
            FindClose(find_handle);
        }
#else
        DIR* directory_handle = opendir(current_path);
        if (directory_handle) {
            struct dirent* directory_entry;
            while ((directory_entry = readdir(directory_handle)) != NULL) {
                if (strcmp(directory_entry->d_name, ".") == 0 || strcmp(directory_entry->d_name, "..") == 0) {
                    continue;
                }
                if (ijkl_string_case_insensitive_equals(directory_entry->d_name, segment)) {
                    matching_entries_count++;
                    snprintf(matched_entry_name, sizeof(matched_entry_name), "%s", directory_entry->d_name);
                }
            }
            closedir(directory_handle);
        }
#endif

        if (matching_entries_count == 0) {
            return false;
        }

        if (matching_entries_count > 1) {
            if (out_is_ambiguous) *out_is_ambiguous = true;
            return false;
        }

        /* Anexa o segmento com a caixa real identificada no disco */
        snprintf(current_path + strlen(current_path), sizeof(current_path) - strlen(current_path),
                 "/%s", matched_entry_name);

        segment = strtok_s(NULL, "/", &context_token);
    }

    snprintf(output_resolved_path, output_capacity, "%s", current_path);
    return true;
}

IjklModuleLoader* ijkl_module_loader_create(IjklMemoryArena* memory_arena,
                                           IjklVocabulary* active_vocabulary,
                                           const char* base_search_directory,
                                           const char* framework_search_directory) {
    if (!memory_arena || !active_vocabulary) return NULL;

    IjklModuleLoader* loader = (IjklModuleLoader*)ijkl_memory_arena_allocate(
        memory_arena, sizeof(IjklModuleLoader));
    loader->memory_arena = memory_arena;
    loader->active_vocabulary = active_vocabulary;
    loader->base_search_directory = base_search_directory ? base_search_directory : ".";
    loader->framework_search_directory = framework_search_directory ? framework_search_directory : "lib";
    loader->framework_vocabulary = NULL;
    loader->first_loaded_module = NULL;
    loader->last_loaded_module = NULL;
    loader->total_loaded_modules_count = 0;
    loader->total_errors_count = 0;
    return loader;
}

void ijkl_module_loader_set_framework_vocabulary(IjklModuleLoader* module_loader,
                                                IjklVocabulary* framework_vocabulary) {
    if (module_loader) {
        module_loader->framework_vocabulary = framework_vocabulary;
    }
}

void ijkl_module_loader_destroy(IjklModuleLoader* module_loader) {
    (void)module_loader;
    /* Toda a memória é liberada com a destruição da IjklMemoryArena */
}

/*
 * EFEITO COLATERAL:
 * Localiza um arquivo de módulo testando as três variantes obrigatórias:
 * 1. Nome.Qualificado.ijk
 * 2. Nome/Qualificado.ijk (apenas o último ponto como separador)
 * 3. Nome/Do/Modulo/Qualificado.ijk (todos os pontos como separadores)
 */
bool ijkl_module_loader_resolve_module_path(const IjklModuleLoader* module_loader,
                                            const char* qualified_module_name,
                                            char* output_resolved_path,
                                            size_t output_buffer_capacity) {
    if (!module_loader || !qualified_module_name || !output_resolved_path || output_buffer_capacity == 0) {
        return false;
    }

    const char* names_to_check[2];
    size_t names_count = 1;
    names_to_check[0] = qualified_module_name;

    const char* canonical_name = module_loader->active_vocabulary ?
        ijkl_vocabulary_resolve_framework_unit(module_loader->active_vocabulary, qualified_module_name) : NULL;
    if (canonical_name && !ijkl_string_case_insensitive_equals(canonical_name, qualified_module_name)) {
        names_to_check[names_count++] = canonical_name;
    }

    char candidate_variants_storage[16][1024];
    const char* candidate_variants[16];
    size_t unique_variants_count = 0;

    for (size_t n = 0; n < names_count; ++n) {
        const char* name = names_to_check[n];

        /* Variante 1: Nome.Qualificado.ijk (diretório raiz do projeto) */
        char variant_buffer[1024];
        snprintf(variant_buffer, sizeof(variant_buffer), "%s.ijk", name);
        if (unique_variants_count < 16) {
            snprintf(candidate_variants_storage[unique_variants_count], sizeof(candidate_variants_storage[0]), "%s", variant_buffer);
            candidate_variants[unique_variants_count] = candidate_variants_storage[unique_variants_count];
            unique_variants_count++;
        }

        /* Variante 2: Apenas o último ponto substituído por '/' (A.B.C/D.ijk) */
        snprintf(variant_buffer, sizeof(variant_buffer), "%s", name);
        char* last_dot = strrchr(variant_buffer, '.');
        if (last_dot) {
            *last_dot = '/';
            snprintf(variant_buffer + strlen(variant_buffer), sizeof(variant_buffer) - strlen(variant_buffer), ".ijk");
            bool is_duplicate = false;
            for (size_t v = 0; v < unique_variants_count; ++v) {
                if (strcmp(candidate_variants[v], variant_buffer) == 0) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate && unique_variants_count < 16) {
                snprintf(candidate_variants_storage[unique_variants_count], sizeof(candidate_variants_storage[0]), "%s", variant_buffer);
                candidate_variants[unique_variants_count] = candidate_variants_storage[unique_variants_count];
                unique_variants_count++;
            }
        }

        /* Variante 3: Todos os pontos substituídos por '/' (A/B/C/D.ijk) */
        snprintf(variant_buffer, sizeof(variant_buffer), "%s", name);
        for (size_t i = 0; variant_buffer[i] != '\0'; ++i) {
            if (variant_buffer[i] == '.') {
                variant_buffer[i] = '/';
            }
        }
        snprintf(variant_buffer + strlen(variant_buffer), sizeof(variant_buffer) - strlen(variant_buffer), ".ijk");
        bool is_duplicate_all = false;
        for (size_t v = 0; v < unique_variants_count; ++v) {
            if (strcmp(candidate_variants[v], variant_buffer) == 0) {
                is_duplicate_all = true;
                break;
            }
        }
        if (!is_duplicate_all && unique_variants_count < 16) {
            snprintf(candidate_variants_storage[unique_variants_count], sizeof(candidate_variants_storage[0]), "%s", variant_buffer);
            candidate_variants[unique_variants_count] = candidate_variants_storage[unique_variants_count];
            unique_variants_count++;
        }

        /* Variante 4: Híbrida - diretórios intermediários e namespace folha composto (A/B/C.D.ijk)
         * Substitui todos os pontos por '/' com exceção do último ponto */
        snprintf(variant_buffer, sizeof(variant_buffer), "%s", name);
        last_dot = strrchr(variant_buffer, '.');
        if (last_dot) {
            for (char* ptr = variant_buffer; ptr < last_dot; ++ptr) {
                if (*ptr == '.') {
                    *ptr = '/';
                }
            }
            snprintf(variant_buffer + strlen(variant_buffer), sizeof(variant_buffer) - strlen(variant_buffer), ".ijk");
            bool is_duplicate_leaf = false;
            for (size_t v = 0; v < unique_variants_count; ++v) {
                if (strcmp(candidate_variants[v], variant_buffer) == 0) {
                    is_duplicate_leaf = true;
                    break;
                }
            }
            if (!is_duplicate_leaf && unique_variants_count < 16) {
                snprintf(candidate_variants_storage[unique_variants_count], sizeof(candidate_variants_storage[0]), "%s", variant_buffer);
                candidate_variants[unique_variants_count] = candidate_variants_storage[unique_variants_count];
                unique_variants_count++;
            }
        }

        /* Variante 5: Partições gerais onde os k primeiros pontos viram '/' e o restante '.' */
        size_t name_length = strlen(name);
        for (size_t i = 0; i < name_length; ++i) {
            if (name[i] == '.') {
                snprintf(variant_buffer, sizeof(variant_buffer), "%s", name);
                for (size_t j = 0; j <= i; ++j) {
                    if (variant_buffer[j] == '.') {
                        variant_buffer[j] = '/';
                    }
                }
                snprintf(variant_buffer + strlen(variant_buffer), sizeof(variant_buffer) - strlen(variant_buffer), ".ijk");
                bool is_dup_part = false;
                for (size_t v = 0; v < unique_variants_count; ++v) {
                    if (strcmp(candidate_variants[v], variant_buffer) == 0) {
                        is_dup_part = true;
                        break;
                    }
                }
                if (!is_dup_part && unique_variants_count < 16) {
                    snprintf(candidate_variants_storage[unique_variants_count], sizeof(candidate_variants_storage[0]), "%s", variant_buffer);
                    candidate_variants[unique_variants_count] = candidate_variants_storage[unique_variants_count];
                    unique_variants_count++;
                }
            }
        }
    }

    /* Testa cada variante no sistema de arquivos: primeiro no diretório base, depois no framework */
    char matched_paths[16][1024];
    size_t matching_variants_count = 0;

    char parent_search_directory[1024] = "";
    const char* search_directories[4];
    size_t search_directories_count = 0;
    search_directories[search_directories_count++] = module_loader->base_search_directory;

    /* Adiciona o diretório pai do arquivo de entrada para permitir que subpastas (ex.: tests/)
       importem módulos da raiz do projeto */
    if (module_loader->base_search_directory && strcmp(module_loader->base_search_directory, ".") != 0) {
        snprintf(parent_search_directory, sizeof(parent_search_directory), "%s", module_loader->base_search_directory);
        normalize_path_separators(parent_search_directory);
        char* last_slash = strrchr(parent_search_directory, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (strlen(parent_search_directory) > 0 &&
                !ijkl_string_case_insensitive_equals(parent_search_directory, module_loader->base_search_directory)) {
                search_directories[search_directories_count++] = parent_search_directory;
            }
        }
    }

    if (module_loader->framework_search_directory &&
        *module_loader->framework_search_directory &&
        !ijkl_string_case_insensitive_equals(module_loader->framework_search_directory, module_loader->base_search_directory)) {
        search_directories[search_directories_count++] = module_loader->framework_search_directory;
    }

    for (size_t d = 0; d < search_directories_count; ++d) {
        const char* current_base_dir = search_directories[d];
        for (size_t v = 0; v < unique_variants_count; ++v) {
            char resolved_candidate[1024];
            bool is_ambiguous = false;
            if (resolve_case_insensitive_relative_path(current_base_dir,
                                                      candidate_variants[v],
                                                      resolved_candidate,
                                                      sizeof(resolved_candidate),
                                                      &is_ambiguous)) {
                if (is_ambiguous) {
                    fprintf(stderr, "[Error] Ambiguous file casing found on disk for module '%s' (variant '%s').\n",
                            qualified_module_name, candidate_variants[v]);
                    return false;
                }
                /* Evita adicionar caminhos duplicados */
                bool already_matched = false;
                for (size_t m = 0; m < matching_variants_count; ++m) {
                    if (ijkl_string_case_insensitive_equals(matched_paths[m], resolved_candidate)) {
                        already_matched = true;
                        break;
                    }
                }
                if (!already_matched && matching_variants_count < 16) {
                    snprintf(matched_paths[matching_variants_count], sizeof(matched_paths[0]), "%s", resolved_candidate);
                    matching_variants_count++;
                }
            }
        }
        if (matching_variants_count > 0) {
            break;
        }
    }

    if (matching_variants_count == 0) {
        fprintf(stderr, "[Error] Module '%s' not found. Searched from '%s' for variants:\n",
                qualified_module_name, module_loader->base_search_directory);
        for (size_t v = 0; v < unique_variants_count; ++v) {
            fprintf(stderr, "  - %s\n", candidate_variants[v]);
        }
        return false;
    }

    if (matching_variants_count > 1) {
        fprintf(stderr, "[Error] Ambiguous module reference '%s'. Multiple candidate files match on disk:\n",
                qualified_module_name);
        for (size_t m = 0; m < matching_variants_count; ++m) {
            fprintf(stderr, "  - %s\n", matched_paths[m]);
        }
        return false;
    }

    snprintf(output_resolved_path, output_buffer_capacity, "%s", matched_paths[0]);
    return true;
}

/* Busca módulo já registrado no carregador pelo nome lógico (case-insensitive) */
static IjklLoadedModule* find_loaded_module_by_name(IjklModuleLoader* module_loader,
                                                    const char* logical_name) {
    if (!module_loader || !logical_name) return NULL;
    const char* canonical_target = NULL;
    if (module_loader->active_vocabulary) {
        canonical_target = ijkl_vocabulary_resolve_framework_unit(module_loader->active_vocabulary, logical_name);
    }
    if (!canonical_target) {
        canonical_target = logical_name;
    }

    IjklLoadedModule* current = module_loader->first_loaded_module;
    while (current) {
        if (current->logical_name) {
            if (ijkl_string_case_insensitive_equals(current->logical_name, logical_name)) {
                return current;
            }
            const char* canonical_current = NULL;
            if (module_loader->active_vocabulary) {
                canonical_current = ijkl_vocabulary_resolve_framework_unit(
                    module_loader->active_vocabulary, current->logical_name);
            }
            if (!canonical_current) {
                canonical_current = current->logical_name;
            }
            if (canonical_target && canonical_current &&
                ijkl_string_case_insensitive_equals(canonical_current, canonical_target)) {
                return current;
            }
        }
        current = current->next_module;
    }
    return NULL;
}

/*
 * EFEITO COLATERAL:
 * Carrega recursivamente uma unidade e suas dependências.
 */
static IjklLoadedModule* load_module_recursive(IjklModuleLoader* module_loader,
                                               const char* logical_name,
                                               const char* explicit_source_path) {
    if (!module_loader || !logical_name) return NULL;

    /* Verifica se o módulo já foi carregado ou está em processo de carga (ciclo/recursão mútua) */
    IjklLoadedModule* existing = find_loaded_module_by_name(module_loader, logical_name);
    if (existing) {
        if (existing->is_currently_loading) {
            /* Ciclo de dependência permitido: retorna sem recursão infinita (Requisito 9) */
            return existing;
        }
        return existing;
    }

    char resolved_file_path[1024];
    if (explicit_source_path) {
        snprintf(resolved_file_path, sizeof(resolved_file_path), "%s", explicit_source_path);
    } else {
        if (!ijkl_module_loader_resolve_module_path(module_loader, logical_name,
                                                    resolved_file_path, sizeof(resolved_file_path))) {
            module_loader->total_errors_count++;
            return NULL;
        }
    }

    /* Lê o conteúdo do arquivo */
    size_t source_length = 0;
    char* source_buffer = read_file_content_to_buffer(module_loader->memory_arena,
                                                      resolved_file_path, &source_length);
    if (!source_buffer) {
        fprintf(stderr, "[Error] Failed to read source file '%s'.\n", resolved_file_path);
        module_loader->total_errors_count++;
        return NULL;
    }

    /* Determina se a unidade pertence ao framework padrão */
    bool is_framework_unit = false;
    if (strncmp(logical_name, "ijkL.", 5) == 0 || strncmp(logical_name, "ijkl.", 5) == 0) {
        is_framework_unit = true;
    }
    if (module_loader->active_vocabulary) {
        const char* canonical_name = ijkl_vocabulary_resolve_framework_unit(module_loader->active_vocabulary, logical_name);
        if (canonical_name && (strncmp(canonical_name, "ijkL.", 5) == 0 || strncmp(canonical_name, "ijkl.", 5) == 0)) {
            is_framework_unit = true;
        }
    }

    IjklVocabulary* lexer_vocabulary = module_loader->active_vocabulary;
    if (is_framework_unit && module_loader->framework_vocabulary) {
        lexer_vocabulary = module_loader->framework_vocabulary;
    }

    /* Instancia lexer e parser */
    IjklLexer* lexer = ijkl_lexer_create(source_buffer, source_length,
                                        lexer_vocabulary,
                                        module_loader->memory_arena,
                                        resolved_file_path);
    IjklParser* parser = ijkl_parser_create(lexer, module_loader->memory_arena, resolved_file_path);
    IjklAstNode* unit_ast = ijkl_parser_parse_compilation_unit(parser);
    size_t parse_errors = ijkl_parser_get_error_count(parser);

    ijkl_parser_destroy(parser);
    ijkl_lexer_destroy(lexer);

    if (parse_errors > 0 || !unit_ast) {
        fprintf(stderr, "[Error] Parsing failed with %zu error(s) in '%s'.\n",
                parse_errors, resolved_file_path);
        module_loader->total_errors_count += (parse_errors > 0 ? parse_errors : 1);
        return NULL;
    }

    /* Validação do Requisito 5: cada arquivo deve conter exatamente uma unidade */
    if (unit_ast->next_sibling != NULL) {
        fprintf(stderr, "[Error] File '%s' contains multiple compilation units. Exactly one 'unit' or 'program' per file is required.\n",
                resolved_file_path);
        module_loader->total_errors_count++;
        return NULL;
    }

    /* Unidades do tipo 'program' não podem ser importadas */
    if (unit_ast->kind == AST_NODE_PROGRAM) {
        fprintf(stderr, "[Error] Cannot import program unit '%s' from '%s'. Only 'unit' modules can be imported.\n",
                unit_ast->data.program.program_name ? unit_ast->data.program.program_name : logical_name,
                resolved_file_path);
        module_loader->total_errors_count++;
        return NULL;
    }

    /* Validação do Requisito 6: correspondência estrita entre o nome lógico declarado e o importado */
    const char* declared_name = NULL;
    if (unit_ast->kind == AST_NODE_MODULE) {
        declared_name = unit_ast->data.module_declaration.qualified_module_path;
    } else if (unit_ast->kind == AST_NODE_PROGRAM) {
        declared_name = unit_ast->data.program.program_name;
    }

    const char* canonical_declared = NULL;
    if (module_loader->active_vocabulary && declared_name) {
        canonical_declared = ijkl_vocabulary_resolve_framework_unit(module_loader->active_vocabulary, declared_name);
    }
    if (!canonical_declared) {
        canonical_declared = declared_name;
    }

    const char* canonical_logical = NULL;
    if (module_loader->active_vocabulary && logical_name) {
        canonical_logical = ijkl_vocabulary_resolve_framework_unit(module_loader->active_vocabulary, logical_name);
    }
    if (!canonical_logical) {
        canonical_logical = logical_name;
    }

    if (declared_name &&
        !ijkl_string_case_insensitive_equals(declared_name, logical_name) &&
        !ijkl_string_case_insensitive_equals(canonical_declared, canonical_logical)) {
        fprintf(stderr, "[Error] Module identity mismatch in '%s': declared unit '%s' does not match imported module name '%s'.\n",
                resolved_file_path, declared_name, logical_name);
        module_loader->total_errors_count++;
        return NULL;
    }

    /* Aloca cópia persistente do caminho na arena para evitar ponteiro pendente */
    size_t path_length = strlen(resolved_file_path);
    char* persistent_path = (char*)ijkl_memory_arena_allocate(module_loader->memory_arena, path_length + 1);
    memcpy(persistent_path, resolved_file_path, path_length + 1);

    /* Registra o módulo carregado */
    IjklLoadedModule* new_module = (IjklLoadedModule*)ijkl_memory_arena_allocate(
        module_loader->memory_arena, sizeof(IjklLoadedModule));
    new_module->logical_name = declared_name ? declared_name : logical_name;
    new_module->source_file_path = persistent_path;
    new_module->source_code_buffer = source_buffer;
    new_module->source_length_in_bytes = source_length;
    new_module->compilation_unit_ast = unit_ast;
    new_module->is_currently_loading = true;
    new_module->is_loading_completed = false;
    new_module->next_module = NULL;

    if (!module_loader->first_loaded_module) {
        module_loader->first_loaded_module = new_module;
        module_loader->last_loaded_module = new_module;
    } else {
        module_loader->last_loaded_module->next_module = new_module;
        module_loader->last_loaded_module = new_module;
    }
    module_loader->total_loaded_modules_count++;

    /* Varre e carrega recursivamente todas as importações declaradas */
    const IjklAstNode* import_node = NULL;
    if (unit_ast->kind == AST_NODE_PROGRAM) {
        import_node = unit_ast->data.program.imports;
    } else if (unit_ast->kind == AST_NODE_MODULE) {
        import_node = unit_ast->data.module_declaration.imports;
    }

    while (import_node) {
        if (import_node->kind == AST_NODE_IMPORT) {
            const char* dep_name = import_node->data.import_declaration.module_path;
            load_module_recursive(module_loader, dep_name, NULL);
        }
        import_node = import_node->next_sibling;
    }

    new_module->is_currently_loading = false;
    new_module->is_loading_completed = true;
    return new_module;
}

IjklAstNode* ijkl_module_loader_load_project(IjklModuleLoader* module_loader,
                                             const char* root_file_path) {
    if (!module_loader || !root_file_path) return NULL;

    /* Extrai o nome base preliminar a partir do nome do arquivo */
    char preliminary_name[256];
    const char* filename = strrchr(root_file_path, '/');
    if (!filename) filename = strrchr(root_file_path, '\\');
    filename = filename ? filename + 1 : root_file_path;

    snprintf(preliminary_name, sizeof(preliminary_name), "%s", filename);
    char* dot = strrchr(preliminary_name, '.');
    if (dot) *dot = '\0';

    /* Lê o conteúdo do arquivo raiz */
    size_t root_size = 0;
    char* root_buffer = read_file_content_to_buffer(module_loader->memory_arena, root_file_path, &root_size);
    if (!root_buffer) {
        fprintf(stderr, "[Error] Could not open root compilation file '%s'.\n", root_file_path);
        module_loader->total_errors_count++;
        return NULL;
    }

    /* Analisa sintaticamente o arquivo raiz para extrair o cabeçalho real */
    IjklLexer* root_lexer = ijkl_lexer_create(root_buffer, root_size,
                                             module_loader->active_vocabulary,
                                             module_loader->memory_arena,
                                             root_file_path);
    IjklParser* root_parser = ijkl_parser_create(root_lexer, module_loader->memory_arena, root_file_path);
    IjklAstNode* root_ast = ijkl_parser_parse_compilation_unit(root_parser);
    size_t root_parse_errors = ijkl_parser_get_error_count(root_parser);

    ijkl_parser_destroy(root_parser);
    ijkl_lexer_destroy(root_lexer);

    if (root_parse_errors > 0 || !root_ast) {
        fprintf(stderr, "[Error] Parsing failed with %zu error(s) in root file '%s'.\n",
                root_parse_errors, root_file_path);
        module_loader->total_errors_count += (root_parse_errors > 0 ? root_parse_errors : 1);
        return NULL;
    }

    const char* root_logical_name = preliminary_name;
    if (root_ast->kind == AST_NODE_PROGRAM && root_ast->data.program.program_name) {
        root_logical_name = root_ast->data.program.program_name;
    } else if (root_ast->kind == AST_NODE_MODULE && root_ast->data.module_declaration.qualified_module_path) {
        root_logical_name = root_ast->data.module_declaration.qualified_module_path;
    }

    IjklLoadedModule* root_module = (IjklLoadedModule*)ijkl_memory_arena_allocate(
        module_loader->memory_arena, sizeof(IjklLoadedModule));
    root_module->logical_name = root_logical_name;
    root_module->source_file_path = root_file_path;
    root_module->source_code_buffer = root_buffer;
    root_module->source_length_in_bytes = root_size;
    root_module->compilation_unit_ast = root_ast;
    root_module->is_currently_loading = true;
    root_module->is_loading_completed = false;
    root_module->next_module = NULL;

    module_loader->first_loaded_module = root_module;
    module_loader->last_loaded_module = root_module;
    module_loader->total_loaded_modules_count = 1;

    /* Carrega recursivamente as dependências importadas pela raiz */
    const IjklAstNode* import_node = NULL;
    if (root_ast->kind == AST_NODE_PROGRAM) {
        import_node = root_ast->data.program.imports;
    } else if (root_ast->kind == AST_NODE_MODULE) {
        import_node = root_ast->data.module_declaration.imports;
    }

    while (import_node) {
        if (import_node->kind == AST_NODE_IMPORT) {
            const char* dep_name = import_node->data.import_declaration.module_path;
            load_module_recursive(module_loader, dep_name, NULL);
        }
        import_node = import_node->next_sibling;
    }

    root_module->is_currently_loading = false;
    root_module->is_loading_completed = true;

    if (module_loader->total_errors_count > 0) {
        return NULL;
    }

    /* Encadeia todas as árvores AST das unidades carregadas em sequência linear segura */
    IjklAstNode* chained_ast_root = NULL;
    IjklAstNode* chained_ast_tail = NULL;
    IjklLoadedModule* current = module_loader->first_loaded_module;

    while (current) {
        if (current->compilation_unit_ast) {
            if (!chained_ast_root) {
                chained_ast_root = current->compilation_unit_ast;
            } else if (chained_ast_tail) {
                chained_ast_tail->next_sibling = current->compilation_unit_ast;
            }

            IjklAstNode* current_node_iterator = current->compilation_unit_ast;
            while (current_node_iterator->next_sibling) {
                current_node_iterator = current_node_iterator->next_sibling;
            }
            chained_ast_tail = current_node_iterator;
        }
        current = current->next_module;
    }

    return chained_ast_root;
}

size_t ijkl_module_loader_get_error_count(const IjklModuleLoader* module_loader) {
    return module_loader ? module_loader->total_errors_count : 0;
}
