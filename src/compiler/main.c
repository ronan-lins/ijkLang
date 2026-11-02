#include "common.h"
#include "memory.h"
#include "config.h"
#include "vocabulary.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "symtab.h"
#include "semantic.h"
#include "codegen.h"
#include "module_loader.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#endif

typedef struct IjklCommandLineOptions {
    const char* input_file_path;
    const char* output_intermediate_file_path;
    const char* configuration_file_path;
    const char* language_override;
    bool should_dump_tokens;
    bool should_dump_abstract_syntax_tree;
    bool should_dump_symbol_table;
    bool should_dump_intermediate_code;
    bool use_native_builtins;
    bool is_verbose_output_enabled;
} IjklCommandLineOptions;

static void print_project_banner(void) {
    printf("%s v%s\n", IJKL_PROJECT_NAME, IJKL_VERSION_STRING);
}

static void print_usage_instructions(const char* executable_program_name) {
    print_project_banner();
    printf("\nUsage: %s [options] <source_file[.ijk]>\n\n", executable_program_name);
    printf("Options:\n");
    printf("  -o <output.ci>    Specify output intermediate code file (default: <source_file>.ci)\n");
    printf("  -ini <path>       Specify custom configuration file (default: ijkl.ini)\n");
    printf("  --lang, -l <lang> Specify source language dictionary (e.g. pt-BR, en-US)\n");
    printf("  --tokens          Display token stream from lexical analyzer\n");
    printf("  --ast             Display Abstract Syntax Tree (AST)\n");
    printf("  --symtab          Display symbol table and scopes\n");
    printf("  --ci, --codegen   Display generated intermediate code (.ci)\n");
    printf("  --native-builtins Omit pure CI built-in routines (resolved natively by VM)\n");
    printf("  -v, --version     Display compiler version\n");
    printf("  -h, --help        Display this help message\n");
}

/* Localiza o diretório onde o arquivo binário do compilador está armazenado */
static void resolve_executable_directory(char* destination_buffer, size_t maximum_buffer_length) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD path_length = GetModuleFileNameA(NULL, destination_buffer, (DWORD)maximum_buffer_length);
    if (path_length > 0) {
        char* last_directory_separator = strrchr(destination_buffer, '\\');
        if (last_directory_separator) {
            *last_directory_separator = '\0';
            return;
        }
    }
#else
    ssize_t path_length = readlink("/proc/self/exe", destination_buffer, maximum_buffer_length - 1);
    if (path_length != -1) {
        destination_buffer[path_length] = '\0';
        char* last_directory_separator = strrchr(destination_buffer, '/');
        if (last_directory_separator) {
            *last_directory_separator = '\0';
            return;
        }
    }
#endif
    destination_buffer[0] = '.';
    destination_buffer[1] = '\0';
}

static char* read_entire_file_to_memory(const char* target_file_path, size_t* out_file_size_in_bytes) {
    FILE* file_handle = fopen(target_file_path, "rb");
    if (!file_handle) return NULL;

    fseek(file_handle, 0, SEEK_END);
    long detected_file_size = ftell(file_handle);
    fseek(file_handle, 0, SEEK_SET);

    if (detected_file_size < 0) {
        fclose(file_handle);
        return NULL;
    }

    size_t allocation_size = (size_t)detected_file_size;
    char* file_content_buffer = (char*)ijkl_memory_heap_allocate(allocation_size + 1);
    size_t actual_read_bytes = fread(file_content_buffer, 1, allocation_size, file_handle);
    file_content_buffer[actual_read_bytes] = '\0';
    fclose(file_handle);

    /* Tratamento transparente de marca de ordem de byte UTF-8 (BOM: 0xEF, 0xBB, 0xBF) */
    if (actual_read_bytes >= 3 &&
        (unsigned char)file_content_buffer[0] == 0xEF &&
        (unsigned char)file_content_buffer[1] == 0xBB &&
        (unsigned char)file_content_buffer[2] == 0xBF) {
        memmove(file_content_buffer, file_content_buffer + 3, actual_read_bytes - 3 + 1);
        actual_read_bytes -= 3;
    }

    if (out_file_size_in_bytes) {
        *out_file_size_in_bytes = actual_read_bytes;
    }
    return file_content_buffer;
}

static bool has_extension(const char* file_path, const char* expected_extension) {
    if (!file_path || !expected_extension) return false;
    size_t file_path_length = strlen(file_path);
    size_t extension_length = strlen(expected_extension);
    if (file_path_length < extension_length) return false;

#if defined(_WIN32) || defined(_WIN64)
    return (_stricmp(file_path + file_path_length - extension_length, expected_extension) == 0);
#else
    return (strcasecmp(file_path + file_path_length - extension_length, expected_extension) == 0);
#endif
}

/* Resolve o caminho do arquivo-fonte de entrada, inferindo '.ijk' quando a extensão for omitida */
static const char* resolve_input_source_file_path(IjklMemoryArena* memory_arena,
                                                 const char* raw_input_path) {
    if (!raw_input_path) return NULL;
    if (has_extension(raw_input_path, ".ijk")) {
        return raw_input_path;
    }

    size_t raw_length = strlen(raw_input_path);
    size_t candidate_length = raw_length + 5; /* raw + ".ijk" + '\0' */
    char* candidate_path = (char*)ijkl_memory_arena_allocate(memory_arena, candidate_length);
    snprintf(candidate_path, candidate_length, "%s.ijk", raw_input_path);

    /* Se o arquivo com '.ijk' existir no disco, adota-o */
    FILE* test_handle = fopen(candidate_path, "rb");
    if (test_handle) {
        fclose(test_handle);
        return candidate_path;
    }

    /* Caso o arquivo sem extensão exista explicitamente no disco, usa o original */
    FILE* raw_handle = fopen(raw_input_path, "rb");
    if (raw_handle) {
        fclose(raw_handle);
        return raw_input_path;
    }

    /* Se nenhum existir, adota o nome com '.ijk' para consistência pedagógica da mensagem de erro */
    return candidate_path;
}

/* Deriva o caminho padrão do arquivo de saída intermediária (.ci) a partir do arquivo-fonte (.ijk) */
static char* create_default_output_intermediate_file_path(IjklMemoryArena* memory_arena,
                                                          const char* input_file_path) {
    if (!input_file_path) return NULL;

    size_t input_path_length = strlen(input_file_path);
    const char* last_dot = strrchr(input_file_path, '.');
    const char* last_forward_slash = strrchr(input_file_path, '/');
    const char* last_backward_slash = strrchr(input_file_path, '\\');
    const char* last_directory_separator = (last_forward_slash > last_backward_slash)
                                               ? last_forward_slash
                                               : last_backward_slash;

    size_t base_path_length = input_path_length;
    if (last_dot && (!last_directory_separator || last_dot > last_directory_separator)) {
        base_path_length = (size_t)(last_dot - input_file_path);
    }

    size_t allocated_size = base_path_length + 4; /* base + ".ci" + '\0' */
    char* default_output_path = (char*)ijkl_memory_arena_allocate(memory_arena, allocated_size);
    memcpy(default_output_path, input_file_path, base_path_length);
    default_output_path[base_path_length] = '\0';
    strcat(default_output_path, ".ci");

    return default_output_path;
}

static bool is_directory_present(const char* directory_path) {
    if (!directory_path || !*directory_path) return false;
#if defined(_WIN32) || defined(_WIN64)
    DWORD file_attributes = GetFileAttributesA(directory_path);
    return (file_attributes != INVALID_FILE_ATTRIBUTES && (file_attributes & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat directory_stat;
    if (stat(directory_path, &directory_stat) != 0) return false;
    return S_ISDIR(directory_stat.st_mode);
#endif
}

/* Localiza e resolve o diretório raiz da biblioteca padrão (framework) ijkL */
static const char* resolve_framework_directory(IjklMemoryArena* memory_arena,
                                               const IjklCompilerConfig* compiler_config,
                                               const char* executable_directory) {
    char candidate_path[1024];

    /* Tentativa 1: Diretório relativo direto ao diretório de trabalho corrente */
    if (is_directory_present(compiler_config->framework_directory)) {
        size_t path_length = strlen(compiler_config->framework_directory) + 1;
        char* resolved_path = (char*)ijkl_memory_arena_allocate(memory_arena, path_length);
        memcpy(resolved_path, compiler_config->framework_directory, path_length);
        return resolved_path;
    }

    /* Tentativa 2: Caminho relativo ao diretório do executável */
    if (executable_directory && *executable_directory) {
        snprintf(candidate_path, sizeof(candidate_path), "%s%c%s",
                 executable_directory,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->framework_directory);
        if (is_directory_present(candidate_path)) {
            size_t path_length = strlen(candidate_path) + 1;
            char* resolved_path = (char*)ijkl_memory_arena_allocate(memory_arena, path_length);
            memcpy(resolved_path, candidate_path, path_length);
            return resolved_path;
        }

        /* Tentativa 3: Diretório pai do executável (comum em compilações Debug/Release) */
        snprintf(candidate_path, sizeof(candidate_path), "%s%c..%c%s",
                 executable_directory,
                 IJKL_PATH_SEPARATOR,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->framework_directory);
        if (is_directory_present(candidate_path)) {
            size_t path_length = strlen(candidate_path) + 1;
            char* resolved_path = (char*)ijkl_memory_arena_allocate(memory_arena, path_length);
            memcpy(resolved_path, candidate_path, path_length);
            return resolved_path;
        }

        /* Tentativa 4: Diretório raiz do projeto (dois níveis acima de bin/Debug) */
        snprintf(candidate_path, sizeof(candidate_path), "%s%c..%c..%c%s",
                 executable_directory,
                 IJKL_PATH_SEPARATOR,
                 IJKL_PATH_SEPARATOR,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->framework_directory);
        if (is_directory_present(candidate_path)) {
            size_t path_length = strlen(candidate_path) + 1;
            char* resolved_path = (char*)ijkl_memory_arena_allocate(memory_arena, path_length);
            memcpy(resolved_path, candidate_path, path_length);
            return resolved_path;
        }
    }

    return compiler_config->framework_directory;
}

static bool resolve_and_load_vocabulary(IjklVocabulary* vocabulary,
                                       const IjklCompilerConfig* compiler_config,
                                       const char* executable_directory) {
    char candidate_words_path[1024];

    /* Tentativa 1: Caminho relativo direto ao diretório de trabalho corrente */
    snprintf(candidate_words_path, sizeof(candidate_words_path), "%s%c%s.words",
             compiler_config->keywords_directory,
             IJKL_PATH_SEPARATOR,
             compiler_config->active_language);
    if (ijkl_vocabulary_load_file(vocabulary, candidate_words_path)) {
        return true;
    }

    /* Tentativa 2: Caminho junto ao diretório do executável */
    if (executable_directory && *executable_directory) {
        snprintf(candidate_words_path, sizeof(candidate_words_path), "%s%c%s%c%s.words",
                 executable_directory,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->keywords_directory,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->active_language);
        if (ijkl_vocabulary_load_file(vocabulary, candidate_words_path)) {
            return true;
        }

        /* Tentativa 3: Diretório pai do executável (comum em compilações Debug/Release) */
        snprintf(candidate_words_path, sizeof(candidate_words_path), "%s%c..%c%s%c%s.words",
                 executable_directory,
                 IJKL_PATH_SEPARATOR,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->keywords_directory,
                 IJKL_PATH_SEPARATOR,
                 compiler_config->active_language);
        if (ijkl_vocabulary_load_file(vocabulary, candidate_words_path)) {
            return true;
        }
    }

    return false;
}

int main(int argument_count, char* argument_vector[]) {
    IjklCommandLineOptions command_line_options = {
        .input_file_path = NULL,
        .output_intermediate_file_path = NULL,
        .configuration_file_path = "ijkl.ini",
        .language_override = NULL,
        .should_dump_tokens = false,
        .should_dump_abstract_syntax_tree = false,
        .should_dump_symbol_table = false,
        .is_verbose_output_enabled = false
    };

    if (argument_count < 2) {
        print_usage_instructions(argument_vector[0]);
        return EXIT_FAILURE;
    }

    for (int argument_index = 1; argument_index < argument_count; argument_index++) {
        const char* current_argument = argument_vector[argument_index];

        if (strcmp(current_argument, "-h") == 0 || strcmp(current_argument, "--help") == 0) {
            print_usage_instructions(argument_vector[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(current_argument, "-v") == 0 || strcmp(current_argument, "--version") == 0) {
            print_project_banner();
            return EXIT_SUCCESS;
        } else if (strcmp(current_argument, "--tokens") == 0) {
            command_line_options.should_dump_tokens = true;
        } else if (strcmp(current_argument, "--ast") == 0) {
            command_line_options.should_dump_abstract_syntax_tree = true;
        } else if (strcmp(current_argument, "--symtab") == 0) {
            command_line_options.should_dump_symbol_table = true;
        } else if (strcmp(current_argument, "--ci") == 0 || strcmp(current_argument, "--codegen") == 0) {
            command_line_options.should_dump_intermediate_code = true;
        } else if (strcmp(current_argument, "--native-builtins") == 0) {
            command_line_options.use_native_builtins = true;
        } else if (strcmp(current_argument, "-o") == 0 && argument_index + 1 < argument_count) {
            command_line_options.output_intermediate_file_path = argument_vector[++argument_index];
        } else if (strcmp(current_argument, "-ini") == 0 && argument_index + 1 < argument_count) {
            command_line_options.configuration_file_path = argument_vector[++argument_index];
        } else if ((strcmp(current_argument, "--lang") == 0 ||
                    strcmp(current_argument, "-lang") == 0 ||
                    strcmp(current_argument, "-l") == 0) && argument_index + 1 < argument_count) {
            command_line_options.language_override = argument_vector[++argument_index];
        } else if (current_argument[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", current_argument);
            print_usage_instructions(argument_vector[0]);
            return EXIT_FAILURE;
        } else {
            command_line_options.input_file_path = current_argument;
        }
    }

    if (!command_line_options.input_file_path) {
        fprintf(stderr, "Error: no input file (*.ijk) provided.\n\n");
        print_usage_instructions(argument_vector[0]);
        return EXIT_FAILURE;
    }

    char executable_directory[512];
    resolve_executable_directory(executable_directory, sizeof(executable_directory));

    print_project_banner();

    /* Cria a Arena de Memória para o ciclo de vida desta compilação */
    IjklMemoryArena* compilation_arena = ijkl_memory_arena_create(65536);

    /* Resolve o arquivo-fonte de entrada, inferindo '.ijk' caso a extensão não tenha sido informada */
    command_line_options.input_file_path =
        resolve_input_source_file_path(compilation_arena, command_line_options.input_file_path);

    /* Se a saída (-o) não foi explicitada e nenhuma flag exclusiva de inspeção foi ativada,
       o compilador assume como saída o caminho do primeiro arquivo .ijk com extensão .ci */
    if (!command_line_options.output_intermediate_file_path) {
        if (!command_line_options.should_dump_tokens &&
            !command_line_options.should_dump_abstract_syntax_tree &&
            !command_line_options.should_dump_symbol_table &&
            !command_line_options.should_dump_intermediate_code) {
            command_line_options.output_intermediate_file_path =
                create_default_output_intermediate_file_path(compilation_arena,
                                                            command_line_options.input_file_path);
        }
    }

    /* Carrega opções de configuração do arquivo ijkl.ini */
    IjklCompilerConfig compiler_config;
    if (!ijkl_config_load_from_file(&compiler_config,
                                   command_line_options.configuration_file_path,
                                   executable_directory)) {
        /* Se o arquivo .ini não for encontrado, as opções padrão são mantidas */
    }

    /* Sobrescreve o idioma caso informado na linha de comando (--lang / -l) */
    if (command_line_options.language_override) {
        strncpy(compiler_config.active_language,
                command_line_options.language_override,
                sizeof(compiler_config.active_language) - 1);
        compiler_config.active_language[sizeof(compiler_config.active_language) - 1] = '\0';
    }

    /* Validação da configuração do idioma: exige exatamente um único idioma por compilação (Requisito 3) */
    if (!compiler_config.active_language[0] ||
        strpbrk(compiler_config.active_language, ",; \t\r\n") != NULL) {
        fprintf(stderr, "[Error] Invalid language configuration '%s'. Exactly one language must be specified per compilation in ijkl.ini or via --lang.\n",
                compiler_config.active_language);
        ijkl_memory_arena_destroy(compilation_arena);
        return EXIT_FAILURE;
    }

    /* Carrega o vocabulário (palavras reservadas e alfabeto complementar) */
    IjklVocabulary* vocabulary = ijkl_vocabulary_create(compilation_arena);
    bool vocabulary_loaded_successfully = resolve_and_load_vocabulary(vocabulary,
                                                                     &compiler_config,
                                                                     executable_directory);
    if (!vocabulary_loaded_successfully) {
        fprintf(stderr, "[Error] Fatal error: Could not load keywords dictionary (.words) for language '%s'.\n"
                        "        Check the 'language' and 'keywords' settings in ijkl.ini or specify a valid directory.\n",
                compiler_config.active_language);
        ijkl_memory_arena_destroy(compilation_arena);
        return EXIT_FAILURE;
    }

    /* Valida o mapeamento do ponto de entrada KW_MAIN no vocabulário carregado (Requisito 3) */
    const char* entry_point_name = ijkl_vocabulary_get_entry_point_name(vocabulary);
    if (!entry_point_name || !*entry_point_name) {
        fprintf(stderr, "[Error] The language dictionary '%s.words' does not define an entry point mapping for 'KW_MAIN' (e.g. 'KW_MAIN = principal').\n",
                compiler_config.active_language);
        ijkl_memory_arena_destroy(compilation_arena);
        return EXIT_FAILURE;
    }

    printf("[Info] Active language: %s (Entry point: '%s')\n", compiler_config.active_language, entry_point_name);
    printf("[Info] Source file: %s\n", command_line_options.input_file_path);

    /* Leitura do código-fonte completo para a memória */
    size_t source_length_in_bytes = 0;
    char* source_code_buffer = read_entire_file_to_memory(command_line_options.input_file_path,
                                                         &source_length_in_bytes);
    if (!source_code_buffer) {
        fprintf(stderr, "Error: could not open or read source file '%s'.\n",
                command_line_options.input_file_path);
        ijkl_memory_arena_destroy(compilation_arena);
        return EXIT_FAILURE;
    }

    printf("[Info] Source loaded (%zu bytes).\n\n", source_length_in_bytes);

    int process_exit_code = EXIT_SUCCESS;

    if (command_line_options.should_dump_tokens) {
        printf("--- Token Stream (Lexical Analyzer) ---\n");
        bool scan_successful = ijkl_lexer_dump_tokens(source_code_buffer,
                                                      source_length_in_bytes,
                                                      vocabulary,
                                                      compilation_arena,
                                                      command_line_options.input_file_path,
                                                      stdout);
        if (!scan_successful) {
            fprintf(stderr, "\n[Error] Errors encountered during lexical analysis.\n");
            process_exit_code = EXIT_FAILURE;
        } else {
            printf("\n[Success] Lexical analysis completed with 0 errors.\n");
        }
    }

    /* Se apenas --tokens foi solicitado e não --ast/--symtab/--ci, concluímos aqui */
    if (command_line_options.should_dump_tokens &&
        !command_line_options.should_dump_abstract_syntax_tree &&
        !command_line_options.should_dump_symbol_table &&
        !command_line_options.should_dump_intermediate_code &&
        !command_line_options.output_intermediate_file_path) {
        /* Apenas dump de tokens requisitado */
    } else {
        /* Resolução de diretório base para módulos e carregamento recursivo do grafo */
        char base_directory[1024] = ".";
        const char* last_slash = strrchr(command_line_options.input_file_path, '/');
        const char* last_backslash = strrchr(command_line_options.input_file_path, '\\');
        const char* directory_separator = last_slash > last_backslash ? last_slash : last_backslash;
        if (directory_separator) {
            size_t length_without_file_name = (size_t)(directory_separator - command_line_options.input_file_path);
            if (length_without_file_name >= sizeof(base_directory)) {
                length_without_file_name = sizeof(base_directory) - 1;
            }
            strncpy(base_directory, command_line_options.input_file_path, length_without_file_name);
            base_directory[length_without_file_name] = '\0';
        }

        const char* resolved_framework_directory =
            resolve_framework_directory(compilation_arena, &compiler_config, executable_directory);

        IjklModuleLoader* module_loader = ijkl_module_loader_create(compilation_arena,
                                                                   vocabulary,
                                                                   base_directory,
                                                                   resolved_framework_directory);

        /* Se o idioma ativo for diferente de en-US, carrega en-US.words como vocabulário canônico do framework */
        if (strcmp(compiler_config.active_language, "en-US") != 0) {
            IjklVocabulary* framework_vocabulary = ijkl_vocabulary_create(compilation_arena);
            IjklCompilerConfig canonical_english_config = compiler_config;
            strncpy(canonical_english_config.active_language, "en-US", sizeof(canonical_english_config.active_language) - 1);
            canonical_english_config.active_language[sizeof(canonical_english_config.active_language) - 1] = '\0';
            if (resolve_and_load_vocabulary(framework_vocabulary, &canonical_english_config, executable_directory)) {
                ijkl_module_loader_set_framework_vocabulary(module_loader, framework_vocabulary);
            }
        }

        IjklAstNode* compilation_unit_ast = ijkl_module_loader_load_project(module_loader, command_line_options.input_file_path);
        size_t parse_error_count = ijkl_module_loader_get_error_count(module_loader);

        if (parse_error_count > 0 || !compilation_unit_ast) {
            fprintf(stderr, "\n[Error] Parsing/Module loading encountered %zu error(s).\n", parse_error_count);
            process_exit_code = EXIT_FAILURE;
        } else {
            if (command_line_options.should_dump_abstract_syntax_tree) {
                printf("--- Abstract Syntax Tree (AST) ---\n");
                ijkl_ast_print(compilation_unit_ast, stdout);
                printf("\n[Success] AST successfully constructed.\n");
            } else if (!command_line_options.should_dump_symbol_table &&
                       !command_line_options.should_dump_intermediate_code &&
                       !command_line_options.output_intermediate_file_path) {
                printf("[Info] Syntactic analysis (Parser & AST) completed with 0 errors. (Use --ast to view the tree).\n");
            }

            /* Análise semântica preliminar e montagem da tabela de símbolos e escopos */
            IjklSymbolTable* symbol_table = ijkl_symbol_table_create(compilation_arena);
            ijkl_symbol_table_set_vocabulary(symbol_table, vocabulary);
            bool symtab_built_successfully = ijkl_symbol_table_build_from_ast(symbol_table, compilation_unit_ast);

            if (!symtab_built_successfully) {
                fprintf(stderr, "\n[Error] Symbol table construction encountered %zu semantic error(s).\n",
                        ijkl_symbol_table_get_error_count(symbol_table));
                process_exit_code = EXIT_FAILURE;
            } else {
                if (command_line_options.should_dump_symbol_table) {
                    printf("--- Symbol Table & Scopes ---\n");
                    ijkl_symbol_table_print(symbol_table, stdout);
                    printf("\n[Success] Symbol table successfully analyzed.\n");
                }

                /* Validação semântica e verificação estática de tipos */
                IjklSemanticAnalyzer* semantic_analyzer = ijkl_semantic_analyzer_create(compilation_arena, symbol_table);
                ijkl_semantic_analyzer_set_vocabulary(semantic_analyzer, vocabulary);
                bool semantic_analysis_successful = ijkl_semantic_analyzer_analyze(semantic_analyzer, compilation_unit_ast);

                if (!semantic_analysis_successful) {
                    fprintf(stderr, "\n[Error] Semantic analysis encountered %zu error(s).\n",
                            ijkl_semantic_analyzer_get_error_count(semantic_analyzer));
                    process_exit_code = EXIT_FAILURE;
                } else {
                    if (!command_line_options.should_dump_symbol_table &&
                        !command_line_options.should_dump_abstract_syntax_tree &&
                        !command_line_options.should_dump_intermediate_code &&
                        !command_line_options.output_intermediate_file_path) {
                        printf("[Info] Semantic analysis and type checking completed with 0 errors.\n");
                    }

                    /* Geração de Código Intermediário (.ci) no terminal */
                    if (command_line_options.should_dump_intermediate_code) {
                        printf("--- Intermediate Code (.ci) ---\n");
                        IjklCodeGenerator* code_generator = ijkl_code_generator_create(compilation_arena, symbol_table, stdout);
                        ijkl_code_generator_set_native_builtins(code_generator, command_line_options.use_native_builtins);
                        ijkl_code_generator_generate(code_generator, compilation_unit_ast);
                        printf("\n[Success] Intermediate code generation completed.\n");
                    }

                    /* Emissão de Código Intermediário (.ci) para arquivo em disco */
                    if (command_line_options.output_intermediate_file_path) {
                        bool file_emission_successful = ijkl_code_generator_generate_to_file(compilation_arena,
                                                                                            symbol_table,
                                                                                            compilation_unit_ast,
                                                                                            command_line_options.output_intermediate_file_path,
                                                                                            command_line_options.use_native_builtins);
                        if (!file_emission_successful) {
                            fprintf(stderr, "\n[Error] Failed to write intermediate code to '%s'.\n",
                                    command_line_options.output_intermediate_file_path);
                            process_exit_code = EXIT_FAILURE;
                        } else {
                            printf("[Info] Intermediate code written to '%s'.\n",
                                    command_line_options.output_intermediate_file_path);
                        }
                    }
                }
            }
        }
    }

    /* Liberação limpa de recursos */
    ijkl_memory_heap_free(source_code_buffer);
    ijkl_memory_arena_destroy(compilation_arena);

    return process_exit_code;
}
