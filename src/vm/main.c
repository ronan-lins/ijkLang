#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "memory.h"
#include "vm/ci_loader.h"
#include "vm/vm_core.h"
#include "vm/vm_plugins.h"
#include "vm/vm_debugger.h"

/*
 * ==============================================================================
 * PROGRAMA PRINCIPAL: MÁQUINA VIRTUAL EDUCATIVA IJKLANG (IJKL-M)
 * ==============================================================================
 * Executa o Código Intermediário (*.ci) compilado, com suporte a depurador
 * interativo no terminal e carga dinâmica de plugins de dispositivos periféricos.
 * ==============================================================================
 */

static void print_banner(void) {
    printf("ijkLang Virtual Machine (ijkL-M) v%s\n", IJKL_VERSION_STRING);
}

static void print_usage(const char* executable_name) {
    print_banner();
    printf("\nUsage: %s [options] <program[.ci]>\n\n", executable_name);
    printf("Options:\n");
    printf("  -d, --debug             Run inside the interactive CLI debugger\n");
    printf("  -s, --stats             Display execution performance and memory metrics\n");
    printf("  -p, --plugin <path>     Load external device peripheral plugin (.dll / .so)\n");
    printf("  -m, --mem <megabytes>   Configure linear memory capacity in MB (default: 16)\n");
    printf("  -v, --version           Display VM version\n");
    printf("  -h, --help              Display this help message\n\n");
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

/* Resolve o caminho do arquivo de código intermediário (.ci), inferindo a extensão caso omitida */
static const char* resolve_input_ci_file_path(IjklMemoryArena* memory_arena,
                                             const char* raw_ci_path) {
    if (!raw_ci_path) return NULL;
    if (has_extension(raw_ci_path, ".ci")) {
        return raw_ci_path;
    }

    size_t raw_length = strlen(raw_ci_path);
    size_t candidate_length = raw_length + 4; /* raw + ".ci" + '\0' */
    char* candidate_path = (char*)ijkl_memory_arena_allocate(memory_arena, candidate_length);
    snprintf(candidate_path, candidate_length, "%s.ci", raw_ci_path);

    /* Se o arquivo com '.ci' existir no disco, adota-o */
    FILE* test_handle = fopen(candidate_path, "r");
    if (test_handle) {
        fclose(test_handle);
        return candidate_path;
    }

    /* Caso o arquivo sem extensão exista explicitamente no disco, usa o original */
    FILE* raw_handle = fopen(raw_ci_path, "r");
    if (raw_handle) {
        fclose(raw_handle);
        return raw_ci_path;
    }

    /* Se nenhum existir, adota o nome com '.ci' para clareza da mensagem de erro */
    return candidate_path;
}

int main(int argument_count, char* argument_values[]) {
    if (argument_count < 2) {
        print_usage(argument_values[0]);
        return 1;
    }

    const char* input_ci_file_path = NULL;
    const char* plugin_file_paths[IJKL_VM_MAX_LOADED_PLUGINS];
    size_t plugin_file_paths_count = 0;
    bool enable_debugger_mode = false;
    bool print_execution_statistics = false;
    size_t memory_capacity_in_megabytes = 16;

    int program_args_start_index = -1;

    for (int i = 1; i < argument_count; ++i) {
        const char* argument = argument_values[i];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            print_usage(argument_values[0]);
            return 0;
        } else if (strcmp(argument, "-v") == 0 || strcmp(argument, "--version") == 0) {
            print_banner();
            return 0;
        } else if (strcmp(argument, "-d") == 0 || strcmp(argument, "--debug") == 0) {
            enable_debugger_mode = true;
        } else if (strcmp(argument, "-s") == 0 || strcmp(argument, "--stats") == 0) {
            print_execution_statistics = true;
        } else if (strcmp(argument, "-m") == 0 || strcmp(argument, "--mem") == 0) {
            if (i + 1 < argument_count) {
                memory_capacity_in_megabytes = (size_t)atoi(argument_values[++i]);
                if (memory_capacity_in_megabytes < 1) memory_capacity_in_megabytes = 1;
            }
        } else if (strcmp(argument, "-p") == 0 || strcmp(argument, "--plugin") == 0) {
            if (i + 1 < argument_count && plugin_file_paths_count < IJKL_VM_MAX_LOADED_PLUGINS) {
                plugin_file_paths[plugin_file_paths_count++] = argument_values[++i];
            }
        } else if (argument[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argument);
            print_usage(argument_values[0]);
            return 1;
        } else if (!input_ci_file_path) {
            input_ci_file_path = argument;
        } else {
            if (program_args_start_index < 0) {
                program_args_start_index = i;
            }
        }
    }

    int program_argc = 0;
    char** program_argv = NULL;
    if (program_args_start_index >= 0) {
        program_argc = argument_count - program_args_start_index;
        program_argv = &argument_values[program_args_start_index];
    }

    if (!input_ci_file_path) {
        fprintf(stderr, "[Error] No intermediate code file (*.ci) specified.\n");
        return 1;
    }

    /* 1. Inicializa arena de memória para o carregamento do programa */
    IjklMemoryArena* memory_arena = ijkl_memory_arena_create(8 * 1024 * 1024);
    if (!memory_arena) {
        fprintf(stderr, "[Error] Failed to initialize loader memory arena.\n");
        return 1;
    }

    /* Resolve o arquivo de código intermediário, inferindo '.ci' caso omitido */
    input_ci_file_path = resolve_input_ci_file_path(memory_arena, input_ci_file_path);

    /* 2. Carrega e decodifica o arquivo .ci */
    IjklCiProgram ci_program;
    memset(&ci_program, 0, sizeof(IjklCiProgram));
    if (!ijkl_ci_loader_load_from_file(memory_arena, input_ci_file_path, &ci_program)) {
        fprintf(stderr, "[Error] Failed to load intermediate code file '%s'.\n", input_ci_file_path);
        ijkl_memory_arena_destroy(memory_arena);
        return 1;
    }

    /* 3. Inicializa a Máquina Virtual */
    size_t memory_bytes = memory_capacity_in_megabytes * 1024 * 1024;
    IjklVirtualMachine virtual_machine;
    if (!ijkl_virtual_machine_initialize(&virtual_machine, memory_arena, memory_bytes)) {
        fprintf(stderr, "[Error] Failed to initialize virtual machine processor.\n");
        ijkl_memory_arena_destroy(memory_arena);
        return 1;
    }

    /* 4. Carrega plugins periféricos adicionais se especificados */
    IjklVmPluginsManager plugins_manager;
    ijkl_vm_plugins_manager_initialize(&plugins_manager, &virtual_machine.io_bus);

    for (size_t p = 0; p < plugin_file_paths_count; ++p) {
        if (!ijkl_vm_plugins_manager_load_plugin(&plugins_manager, plugin_file_paths[p], NULL)) {
            fprintf(stderr, "[Warning] Could not load device plugin '%s'.\n", plugin_file_paths[p]);
        }
    }

    /* 5. Carrega o vetor de instruções e posiciona o ponteiro no ponto de entrada */
    ijkl_virtual_machine_load_instructions(&virtual_machine,
                                           ci_program.instructions,
                                           ci_program.instructions_count);

    virtual_machine.instruction_pointer = ci_program.entry_point_instruction_index;

    /* Requisito 14: Passar contagem de argumentos e ponteiro de strings para a pilha da função main */
    uint64_t args_array_addr = 0;
    if (program_argc > 0) {
        args_array_addr = ijkl_vm_memory_allocate_heap(&virtual_machine.memory, (size_t)program_argc * 8);
        for (int a = 0; a < program_argc; ++a) {
            size_t str_len = strlen(program_argv[a]) + 1;
            uint64_t str_addr = ijkl_vm_memory_allocate_heap(&virtual_machine.memory, str_len);
            ijkl_vm_memory_write_string(&virtual_machine.memory, str_addr, program_argv[a]);
            ijkl_vm_memory_write_integer(&virtual_machine.memory, args_array_addr + (size_t)a * 8, (int64_t)str_addr);
        }
    }
    ijkl_vm_memory_push_stack(&virtual_machine.memory, (int64_t)program_argc);
    ijkl_vm_memory_push_stack(&virtual_machine.memory, (int64_t)args_array_addr);

    /* 6. Execução com ou sem depurador interativo */
    bool execution_success = false;

    if (enable_debugger_mode) {
        print_banner();
        printf("[Debug] Interactive debugging active for '%s' (%zu instructions).\n",
               input_ci_file_path, ci_program.instructions_count);
        printf("[Debug] Entry point set at instruction %zu.\n\n", ci_program.entry_point_instruction_index);

        IjklVmDebugger debugger;
        ijkl_vm_debugger_initialize(&debugger, &virtual_machine, true);

        virtual_machine.execution_state = VM_STATE_RUNNING;
        while (virtual_machine.execution_state == VM_STATE_RUNNING) {
            if (!ijkl_vm_debugger_before_instruction_hook(&debugger)) {
                break;
            }
            if (!ijkl_virtual_machine_step(&virtual_machine)) {
                break;
            }
        }

        execution_success = (virtual_machine.execution_state == VM_STATE_HALTED);
        ijkl_vm_debugger_destroy(&debugger);
    } else {
        execution_success = ijkl_virtual_machine_run(&virtual_machine);
    }

    /* 7. Trata resultados e diagnósticos */
    int exit_code = 0;
    if (virtual_machine.execution_state == VM_STATE_ERROR) {
        fprintf(stderr, "\n[VM Runtime Error] %s (Line: %zu, Error Code: %d)\n",
                virtual_machine.error_message_buffer,
                virtual_machine.error_line_number,
                virtual_machine.last_error_code);
        exit_code = 1;
    } else if (!execution_success && virtual_machine.execution_state != VM_STATE_HALTED) {
        fprintf(stderr, "\n[VM Warning] Execution terminated abnormally.\n");
        exit_code = 1;
    } else if (virtual_machine.execution_state == VM_STATE_HALTED) {
        /* Requisito 14: O valor de retorno da função main (em r0) deve ser utilizado como código de saída */
        exit_code = (int)virtual_machine.registers[IJKL_VM_REG_R0].as.integer_value;
    }

    if (print_execution_statistics) {
        printf("\n--- ijkLang VM Execution Statistics ---\n");
        printf("  Executed Instructions: %llu cycles\n", (unsigned long long)virtual_machine.total_executed_cycles_count);
        printf("  Peak Stack Allocated:  %zu bytes\n", virtual_machine.memory.total_peak_stack_bytes);
        printf("  Total Heap Allocated:  %zu bytes\n", virtual_machine.memory.total_heap_allocated_bytes);
        printf("---------------------------------------\n");
    }

    /* 8. Destruição e liberação limpa de todos os recursos (Zero Leaks) */
    ijkl_vm_plugins_manager_destroy(&plugins_manager);
    ijkl_virtual_machine_destroy(&virtual_machine);
    ijkl_memory_arena_destroy(memory_arena);

    return exit_code;
}
