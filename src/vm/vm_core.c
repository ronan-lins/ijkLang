#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "vm/vm_core.h"

/*
 * ==============================================================================
 * MÓDULO: NÚCLEO DE EXECUÇÃO DA MÁQUINA VIRTUAL (IJKLANG VM)
 * ==============================================================================
 * Implementa o ciclo de busca, decodificação e execução (fetch-decode-execute)
 * das instruções intermediárias lineares, banco de registradores de 64 bits,
 * unidade aritmética/lógica e despacho de chamadas com quadros de ativação.
 * ==============================================================================
 */

static inline uint64_t align_to_8_bytes(uint64_t address) {
    return (address + 7ULL) & ~7ULL;
}

bool ijkl_virtual_machine_initialize(IjklVirtualMachine* machine,
                                     IjklMemoryArena* memory_arena,
                                     size_t memory_capacity_bytes) {
    if (!machine) return false;

    memset(machine, 0, sizeof(IjklVirtualMachine));
    machine->memory_arena = memory_arena;

    if (!ijkl_vm_memory_initialize(&machine->memory, memory_capacity_bytes)) {
        return false;
    }

    if (!ijkl_vm_io_bus_initialize(&machine->io_bus)) {
        ijkl_vm_memory_destroy(&machine->memory);
        return false;
    }

    if (!ijkl_vm_io_bus_register_standard_console(&machine->io_bus, machine)) {
        ijkl_vm_io_bus_destroy(&machine->io_bus);
        ijkl_vm_memory_destroy(&machine->memory);
        return false;
    }

    if (!ijkl_vm_io_bus_register_dma_controller(&machine->io_bus, &machine->dma_controller, &machine->memory)) {
        ijkl_vm_io_bus_destroy(&machine->io_bus);
        ijkl_vm_memory_destroy(&machine->memory);
        return false;
    }

    if (!ijkl_vm_io_bus_register_file_system_controller(&machine->io_bus, &machine->file_io_controller, machine)) {
        ijkl_vm_io_bus_destroy(&machine->io_bus);
        ijkl_vm_memory_destroy(&machine->memory);
        return false;
    }

    if (!ijkl_vm_io_bus_register_system_clock(&machine->io_bus)) {
        ijkl_vm_io_bus_destroy(&machine->io_bus);
        ijkl_vm_memory_destroy(&machine->memory);
        return false;
    }

    ijkl_virtual_machine_reset(machine);
    return true;
}

void ijkl_virtual_machine_destroy(IjklVirtualMachine* machine) {
    if (!machine) return;

    ijkl_vm_io_bus_destroy(&machine->io_bus);
    ijkl_vm_memory_destroy(&machine->memory);
    memset(machine, 0, sizeof(IjklVirtualMachine));
}

/*
 * Utilitário interno para resolução de ponteiros de strings:
 * Converte tanto endereços virtuais no espaço linear da VM quanto ponteiros diretos do host.
 */
static const char* resolve_string_pointer(const IjklVirtualMachine* machine, const IjklVmRegisterValue* value) {
    if (!machine || !value) return NULL;
    if (value->type != VM_VALUE_TYPE_STRING && value->type != VM_VALUE_TYPE_INT) {
        return NULL;
    }
    uint64_t raw_address = value->as.unsigned_value;
    if (raw_address == 0) return NULL;
    if (raw_address < machine->memory.total_allocated_capacity_bytes) {
        if (raw_address >= IJKL_VM_PAGE_NULL_END_ADDRESS) {
            return (const char*)&machine->memory.raw_linear_memory_buffer[raw_address];
        }
        return NULL;
    }
    return value->as.string_pointer;
}

/*
 * Mapeia e copia todos os literais de texto das instruções para o segmento estático da VM,
 * garantindo que instruções de acesso a bytes possam inspecionar os caracteres.
 */
static void populate_instruction_string_literals(IjklVirtualMachine* machine) {
    if (!machine || !machine->instruction_stream || machine->instructions_count == 0) return;
    if (!machine->memory.raw_linear_memory_buffer) return;

    uint64_t current_static_address = IJKL_VM_GLOBAL_BASE_ADDRESS;

    for (size_t i = 0; i < machine->instructions_count; ++i) {
        IjklVmInstruction* inst = &machine->instruction_stream[i];
        IjklVmOperand* operands[3] = { &inst->destination, &inst->first_operand, &inst->second_operand };
        for (int op_idx = 0; op_idx < 3; ++op_idx) {
            IjklVmOperand* operand = operands[op_idx];
            if (operand->kind == OPERAND_KIND_IMMEDIATE_STRING && operand->value.string_value) {
                const char* text = operand->value.string_value;
                size_t text_length = strlen(text) + 1;
                if (current_static_address + text_length < IJKL_VM_HEAP_BASE_ADDRESS) {
                    memcpy(&machine->memory.raw_linear_memory_buffer[current_static_address], text, text_length);
                    operand->string_virtual_address = current_static_address;
                    current_static_address += text_length;
                    /* Alinha o próximo endereço para limite de 8 bytes */
                    current_static_address = (current_static_address + 7ULL) & ~7ULL;
                }
            }
        }
    }
}

void ijkl_virtual_machine_reset(IjklVirtualMachine* machine) {
    if (!machine) return;

    memset(machine->registers, 0, sizeof(machine->registers));
    machine->instruction_pointer = 0;
    machine->call_stack_depth = 0;
    machine->execution_state = VM_STATE_READY;
    machine->last_error_code = VM_ERROR_NONE;
    machine->error_message_buffer[0] = '\0';
    machine->error_line_number = 0;
    machine->total_executed_cycles_count = 0;

    ijkl_vm_memory_reset(&machine->memory);
    ijkl_vm_io_bus_reset(&machine->io_bus);
    memset(&machine->dma_controller, 0, sizeof(IjklDmaController));
    machine->dma_controller.target_memory = &machine->memory;
    memset(&machine->file_io_controller, 0, sizeof(IjklFileIoController));
    machine->file_io_controller.machine = machine;

    /* Registradores especiais de controle */
    machine->current_frame_top = machine->memory.stack_base_address;
    machine->registers[IJKL_VM_REG_RA].type = VM_VALUE_TYPE_INT;
    machine->registers[IJKL_VM_REG_RA].as.unsigned_value = machine->memory.stack_base_address;

    machine->registers[IJKL_VM_REG_SP].type = VM_VALUE_TYPE_INT;
    machine->registers[IJKL_VM_REG_SP].as.unsigned_value = machine->memory.stack_base_address;

    populate_instruction_string_literals(machine);
}

bool ijkl_virtual_machine_load_instructions(IjklVirtualMachine* machine,
                                            IjklVmInstruction* instructions,
                                            size_t instructions_count) {
    if (!machine || !instructions || instructions_count == 0) return false;

    machine->instruction_stream = instructions;
    machine->instructions_count = instructions_count;
    machine->instruction_pointer = 0;
    machine->execution_state = VM_STATE_READY;
    populate_instruction_string_literals(machine);
    return true;
}

/*
 * EFEITO COLATERAL:
 * Registra um erro fatal de execução, interrompendo a Máquina Virtual.
 */
static void set_runtime_error(IjklVirtualMachine* machine,
                              IjklVmErrorCode error_code,
                              const char* error_message,
                              size_t source_line_number) {
    machine->execution_state = VM_STATE_ERROR;
    machine->last_error_code = error_code;
    machine->error_line_number = source_line_number;
    snprintf(machine->error_message_buffer, sizeof(machine->error_message_buffer), "%s", error_message);
}

/*
 * Avalia um operando e extrai seu valor tipado (registrador, imediato ou memória).
 */
static bool evaluate_operand(IjklVirtualMachine* machine,
                             const IjklVmOperand* operand,
                             IjklVmRegisterValue* output_value) {
    if (!operand || !output_value) return false;

    switch (operand->kind) {
        case OPERAND_KIND_REGISTER: {
            IjklVmRegister reg = operand->value.register_identifier;
            if (reg >= 0 && reg < IJKL_VM_REG_COUNT) {
                *output_value = machine->registers[reg];
                return true;
            }
            return false;
        }

        case OPERAND_KIND_IMMEDIATE_INT: {
            output_value->type = VM_VALUE_TYPE_INT;
            output_value->as.integer_value = operand->value.integer_value;
            return true;
        }

        case OPERAND_KIND_IMMEDIATE_FLOAT: {
            output_value->type = VM_VALUE_TYPE_FLOAT;
            output_value->as.float_value = operand->value.float_value;
            return true;
        }

        case OPERAND_KIND_IMMEDIATE_STRING: {
            output_value->type = VM_VALUE_TYPE_STRING;
            if (operand->string_virtual_address) {
                output_value->as.unsigned_value = operand->string_virtual_address;
            } else {
                output_value->as.string_pointer = operand->value.string_value;
            }
            return true;
        }

        case OPERAND_KIND_IMMEDIATE_BOOL: {
            output_value->type = VM_VALUE_TYPE_BOOL;
            output_value->as.integer_value = operand->value.boolean_value ? 1 : 0;
            return true;
        }

        case OPERAND_KIND_IMMEDIATE_CHAR: {
            output_value->type = VM_VALUE_TYPE_CHAR;
            output_value->as.integer_value = (int64_t)operand->value.character_value;
            return true;
        }

        case OPERAND_KIND_MEMORY_INDIRECT: {
            /* Calcula o endereço efetivo: base + deslocamento */
            uint64_t base_address = 0;
            if (operand->value.memory_reference.base_register != IJKL_VM_REG_NONE) {
                base_address = machine->registers[operand->value.memory_reference.base_register].as.unsigned_value;
            } else {
                base_address = operand->value.memory_reference.base_address;
            }

            int64_t offset_bytes = 0;
            if (operand->value.memory_reference.offset_register != IJKL_VM_REG_NONE) {
                offset_bytes = machine->registers[operand->value.memory_reference.offset_register].as.integer_value;
            } else {
                offset_bytes = operand->value.memory_reference.displacement_bytes;
            }

            uint64_t effective_address = (uint64_t)((int64_t)base_address + offset_bytes);

            if (operand->value.memory_reference.is_byte_access) {
                uint8_t byte_val = 0;
                if (!ijkl_vm_memory_read_bytes(&machine->memory, effective_address, &byte_val, 1)) {
                    set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                      "Segmentation fault: invalid memory read access (byte).", 0);
                    return false;
                }
                output_value->type = VM_VALUE_TYPE_INT;
                output_value->as.integer_value = (int64_t)byte_val;
                return true;
            }

            uint8_t tag = ijkl_vm_memory_get_tag(&machine->memory, effective_address);
            if (tag == VM_VALUE_TYPE_FLOAT) {
                double raw_float = 0.0;
                if (!ijkl_vm_memory_read_float(&machine->memory, effective_address, &raw_float)) {
                    set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                      "Segmentation fault: invalid memory read access (float).", 0);
                    return false;
                }
                output_value->type = VM_VALUE_TYPE_FLOAT;
                output_value->as.float_value = raw_float;
                return true;
            } else if (tag == VM_VALUE_TYPE_STRING) {
                int64_t raw_ptr = 0;
                if (!ijkl_vm_memory_read_integer(&machine->memory, effective_address, &raw_ptr)) {
                    set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                      "Segmentation fault: invalid memory read access (string).", 0);
                    return false;
                }
                output_value->type = VM_VALUE_TYPE_STRING;
                output_value->as.string_pointer = (const char*)(uintptr_t)raw_ptr;
                output_value->as.unsigned_value = (uint64_t)raw_ptr;
                return true;
            } else {
                int64_t raw_memory_value = 0;
                if (!ijkl_vm_memory_read_integer(&machine->memory, effective_address, &raw_memory_value)) {
                    set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                      "Segmentation fault: invalid memory read access.", 0);
                    return false;
                }
                output_value->type = (tag == VM_VALUE_TYPE_CHAR || tag == VM_VALUE_TYPE_BOOL) ?
                    (IjklVmValueType)tag : VM_VALUE_TYPE_INT;
                output_value->as.integer_value = raw_memory_value;
                return true;
            }
        }

        case OPERAND_KIND_LABEL_REFERENCE: {
            output_value->type = VM_VALUE_TYPE_INT;
            output_value->as.unsigned_value = operand->value.label_reference.target_instruction_index;
            return true;
        }

        default:
            return false;
    }
}

/*
 * EFEITO COLATERAL:
 * Escreve um valor avaliado no operando de destino (registrador ou memória indireta).
 */
static bool write_destination_operand(IjklVirtualMachine* machine,
                                      const IjklVmOperand* destination_operand,
                                      IjklVmRegisterValue value_to_write) {
    if (!destination_operand) return false;

    if (destination_operand->kind == OPERAND_KIND_REGISTER) {
        IjklVmRegister reg = destination_operand->value.register_identifier;
        if (reg >= 0 && reg < IJKL_VM_REG_COUNT) {
            machine->registers[reg] = value_to_write;

            /* Sincronização de registradores de controle com subsistema de memória */
            if (reg == IJKL_VM_REG_RA) {
                machine->memory.current_frame_pointer = value_to_write.as.unsigned_value;
            } else if (reg == IJKL_VM_REG_SP) {
                machine->memory.current_stack_pointer = value_to_write.as.unsigned_value;
            }
            return true;
        }
        return false;
    }

    if (destination_operand->kind == OPERAND_KIND_MEMORY_INDIRECT) {
        uint64_t base_address = 0;
        if (destination_operand->value.memory_reference.base_register != IJKL_VM_REG_NONE) {
            base_address = machine->registers[destination_operand->value.memory_reference.base_register].as.unsigned_value;
        } else {
            base_address = destination_operand->value.memory_reference.base_address;
        }

        int64_t offset_bytes = 0;
        if (destination_operand->value.memory_reference.offset_register != IJKL_VM_REG_NONE) {
            offset_bytes = machine->registers[destination_operand->value.memory_reference.offset_register].as.integer_value;
        } else {
            offset_bytes = destination_operand->value.memory_reference.displacement_bytes;
        }

        uint64_t effective_address = (uint64_t)((int64_t)base_address + offset_bytes);

        if (destination_operand->value.memory_reference.is_byte_access) {
            uint8_t byte_val = (uint8_t)(value_to_write.as.integer_value & 0xFF);
            if (!ijkl_vm_memory_write_bytes(&machine->memory, effective_address, &byte_val, 1)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                  "Segmentation fault: invalid memory write access (byte).", 0);
                return false;
            }
            return true;
        }

        if (value_to_write.type == VM_VALUE_TYPE_FLOAT) {
            if (!ijkl_vm_memory_write_float(&machine->memory, effective_address, value_to_write.as.float_value)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                  "Segmentation fault: invalid memory write access (float).", 0);
                return false;
            }
            ijkl_vm_memory_set_tag(&machine->memory, effective_address, VM_VALUE_TYPE_FLOAT);
        } else if (value_to_write.type == VM_VALUE_TYPE_STRING) {
            uint64_t addr_to_write = value_to_write.as.unsigned_value ?
                value_to_write.as.unsigned_value : (uint64_t)(uintptr_t)value_to_write.as.string_pointer;
            if (!ijkl_vm_memory_write_integer(&machine->memory, effective_address, (int64_t)addr_to_write)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                  "Segmentation fault: invalid memory write access (string).", 0);
                return false;
            }
            ijkl_vm_memory_set_tag(&machine->memory, effective_address, VM_VALUE_TYPE_STRING);
        } else {
            if (!ijkl_vm_memory_write_integer(&machine->memory, effective_address, value_to_write.as.integer_value)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                  "Segmentation fault: invalid memory write access (int/pointer).", 0);
                return false;
            }
            ijkl_vm_memory_set_tag(&machine->memory, effective_address, (uint8_t)value_to_write.type);
        }
        return true;
    }

    return false;
}

/*
 * Decompõe um timestamp Unix Epoch em segundos nos componentes de data civil
 * gregoriana (ano, mês, dia, hora, minuto, segundo) através de aritmética afim exata.
 */
static void decompose_gregorian_civil(int64_t timestamp_seconds,
                                      int64_t* output_year,
                                      int64_t* output_month,
                                      int64_t* output_day,
                                      int64_t* output_hour,
                                      int64_t* output_minute,
                                      int64_t* output_second) {
    int64_t seconds_of_day = timestamp_seconds % 86400;
    int64_t days_count = timestamp_seconds / 86400;
    if (seconds_of_day < 0) {
        seconds_of_day += 86400;
        days_count -= 1;
    }

    *output_hour = seconds_of_day / 3600;
    int64_t remaining_seconds = seconds_of_day % 3600;
    *output_minute = remaining_seconds / 60;
    *output_second = remaining_seconds % 60;

    int64_t days_shifted = days_count + 719468;
    int64_t era_index = (days_shifted >= 0 ? days_shifted : days_shifted - 146096) / 146097;
    int64_t day_of_era = days_shifted - (era_index * 146097);
    int64_t year_of_era = (day_of_era - (day_of_era / 1460) + (day_of_era / 36524) - (day_of_era / 146096)) / 365;
    int64_t calculated_year = year_of_era + (era_index * 400);
    int64_t day_of_year = day_of_era - ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100));
    int64_t month_prime = ((5 * day_of_year) + 2) / 153;
    int64_t calculated_day = day_of_year - (((153 * month_prime) + 2) / 5) + 1;
    int64_t calculated_month = (month_prime < 10) ? month_prime + 3 : month_prime - 9;
    if (calculated_month <= 2) {
        calculated_year += 1;
    }

    *output_year = calculated_year;
    *output_month = calculated_month;
    *output_day = calculated_day;
}

/*
 * EFEITO COLATERAL:
 * Instancia e preenche a estrutura DateTime (64 bytes) no heap da memória da VM.
 */
static uint64_t build_datetime_instance(IjklVirtualMachine* machine,
                                        int64_t timestamp_seconds,
                                        int64_t millisecond_fraction,
                                        int64_t timezone_offset_seconds) {
    int64_t civil_timestamp = timestamp_seconds + timezone_offset_seconds;
    int64_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    decompose_gregorian_civil(civil_timestamp, &year, &month, &day, &hour, &minute, &second);

    uint64_t datetime_address = ijkl_vm_memory_allocate_heap(&machine->memory, 64);
    if (datetime_address == 0) return 0;

    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 0, year);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 8, month);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 16, day);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 24, hour);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 32, minute);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 40, second);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 48, millisecond_fraction);
    ijkl_vm_memory_write_integer(&machine->memory, datetime_address + 56, timestamp_seconds);

    return datetime_address;
}

/*
 * EFEITO COLATERAL:
 * Despacha rotinas embutidas (built-ins) do sistema implementadas nativamente em C
 * quando a compilação omite os corpos em Código Intermediário (e.g. flag --native-builtins).
 */
/* LCG de 64 bits, com wrap unsigned definido pela norma C11. */
static uint64_t random_advance_state(uint64_t* state) {
    *state = *state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    return *state;
}

/*
 * Aloca uma cadeia de caracteres no heap linear da máquina virtual,
 * inicializa o conteúdo com terminação nula e define a etiqueta VM_VALUE_TYPE_STRING.
 */
static uint64_t create_vm_string(IjklVirtualMachine* machine, const char* source_bytes, size_t string_length) {
    uint64_t string_heap_address = ijkl_vm_memory_allocate_heap(&machine->memory, string_length + 1);
    if (string_heap_address == 0) return 0;
    if (string_length > 0 && source_bytes) {
        memcpy(&machine->memory.raw_linear_memory_buffer[string_heap_address], source_bytes, string_length);
    }
    machine->memory.raw_linear_memory_buffer[string_heap_address + string_length] = '\0';
    ijkl_vm_memory_set_tag(&machine->memory, string_heap_address, VM_VALUE_TYPE_STRING);
    return string_heap_address;
}

/*
 * Instancia no heap linear da VM a estrutura de lista dinâmica (ijkL.Collections.List)
 * com layout binário idêntico: capacity (offset 0), count (offset 8), items pointer (offset 16).
 */
static uint64_t create_vm_list(IjklVirtualMachine* machine, const uint64_t* items_array, size_t items_count) {
    uint64_t list_heap_address = ijkl_vm_memory_allocate_heap(&machine->memory, 24);
    if (list_heap_address == 0) return 0;

    size_t initial_capacity = items_count > 0 ? items_count : 4;
    uint64_t items_buffer_address = ijkl_vm_memory_allocate_heap(&machine->memory, initial_capacity * sizeof(int64_t));
    if (items_buffer_address == 0) {
        ijkl_vm_memory_free_heap(&machine->memory, list_heap_address);
        return 0;
    }

    ijkl_vm_memory_write_integer(&machine->memory, list_heap_address + 0, (int64_t)initial_capacity);
    ijkl_vm_memory_write_integer(&machine->memory, list_heap_address + 8, (int64_t)items_count);
    ijkl_vm_memory_write_integer(&machine->memory, list_heap_address + 16, (int64_t)items_buffer_address);

    for (size_t index = 0; index < items_count; ++index) {
        ijkl_vm_memory_write_integer(&machine->memory, items_buffer_address + index * sizeof(int64_t), (int64_t)items_array[index]);
    }
    return list_heap_address;
}

static bool execute_native_builtin(IjklVirtualMachine* machine, const char* builtin_name) {
    if (!machine || !builtin_name) return false;

    if (strcmp(builtin_name, "__builtin_console_has_key") == 0 ||
        strcmp(builtin_name, "__builtin_console_read_key_async") == 0) {
        bool query_availability = strcmp(builtin_name, "__builtin_console_has_key") == 0;
        int64_t result = 0;
        if (!ijkl_vm_io_bus_read_port(&machine->io_bus,
                query_availability ? IJKL_VM_IO_PORT_CONSOLE_KEY_AVAILABLE :
                                     IJKL_VM_IO_PORT_CONSOLE_READ_KEY_ASYNC, &result)) return false;
        /* A instrução in do caminho CI retorna uma palavra inteira. */
        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = result;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    uint64_t args_base = machine->current_frame_top;

    if (strncmp(builtin_name, "__builtin_random_", 17) == 0) {
        bool seed_explicitly = strcmp(builtin_name, "__builtin_random_seed") == 0;
        bool seed_from_time = strcmp(builtin_name, "__builtin_random_seed_with_time") == 0;
        bool generate_integer = strcmp(builtin_name, "__builtin_random_next") == 0;
        bool generate_range = strcmp(builtin_name, "__builtin_random_next_range") == 0;
        bool generate_real = strcmp(builtin_name, "__builtin_random_next_real") == 0;
        if (!seed_explicitly && !seed_from_time && !generate_integer &&
            !generate_range && !generate_real) return false;
        size_t argument_count = seed_explicitly ? 1 : (generate_range ? 2 : 0);
        int64_t low_address = 0, high_address = 0;
        int64_t state_low = 0, state_high = 0;
        if (!ijkl_vm_memory_read_integer(&machine->memory, args_base + argument_count * 8, &low_address) ||
            !ijkl_vm_memory_read_integer(&machine->memory, args_base + (argument_count + 1) * 8, &high_address) ||
            !ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)low_address, &state_low) ||
            !ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)high_address, &state_high)) return false;
        if (state_low < 0 || state_low > UINT32_MAX || state_high < 0 || state_high > UINT32_MAX) return false;
        uint64_t state = ((uint64_t)state_high << 32) | (uint64_t)state_low;
        int64_t integer_result = 0;
        double real_result = 0.0;
        if (seed_explicitly) {
            int64_t custom_seed = 0;
            if (!ijkl_vm_memory_read_integer(&machine->memory, args_base, &custom_seed)) return false;
            state = (uint64_t)custom_seed;
        } else if (seed_from_time) {
            /* Usa a mesma DateTime.now do caminho CI e libera a instância temporária. */
            if (!execute_native_builtin(machine, "__builtin_datetime_now")) return false;
            uint64_t datetime_address = machine->registers[IJKL_VM_REG_R0].as.unsigned_value;
            int64_t hour = 0, minute = 0, second = 0;
            bool valid_time = ijkl_vm_memory_read_integer(&machine->memory, datetime_address + 24, &hour) &&
                ijkl_vm_memory_read_integer(&machine->memory, datetime_address + 32, &minute) &&
                ijkl_vm_memory_read_integer(&machine->memory, datetime_address + 40, &second);
            ijkl_vm_memory_free_heap(&machine->memory, datetime_address);
            if (!valid_time) return false;
            state = (uint64_t)(hour * 3600 + minute * 60 + second);
        } else if (generate_integer) {
            integer_result = (int64_t)(random_advance_state(&state) >> 33);
        } else if (generate_real) {
            real_result = (double)(random_advance_state(&state) >> 11) / 9007199254740992.0;
        } else {
            int64_t minimum = 0, maximum = 0;
            if (!ijkl_vm_memory_read_integer(&machine->memory, args_base, &minimum) ||
                !ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &maximum)) return false;
            if (minimum > maximum) {
                int64_t original_minimum = minimum;
                minimum = maximum;
                maximum = original_minimum;
            }
            integer_result = minimum;
            if (minimum != maximum) {
                uint64_t distance = (uint64_t)maximum - (uint64_t)minimum;
                unsigned int bit_count = 0;
                for (uint64_t remaining = distance; remaining != 0; remaining >>= 1) ++bit_count;
                uint64_t candidate;
                do {
                    candidate = random_advance_state(&state) >> (64 - bit_count);
                } while (candidate > distance);
                uint64_t result_bits = (uint64_t)minimum + candidate;
                /* int64_t possui representação em complemento de dois; evita cast fora da faixa. */
                memcpy(&integer_result, &result_bits, sizeof(integer_result));
            }
        }
        if (!ijkl_vm_memory_write_integer(&machine->memory, (uint64_t)low_address,
                (int64_t)(state & UINT32_MAX)) ||
            !ijkl_vm_memory_write_integer(&machine->memory, (uint64_t)high_address,
                (int64_t)(state >> 32))) return false;
        machine->registers[IJKL_VM_REG_R0].type = generate_real ? VM_VALUE_TYPE_FLOAT : VM_VALUE_TYPE_INT;
        if (generate_real) machine->registers[IJKL_VM_REG_R0].as.float_value = real_result;
        else machine->registers[IJKL_VM_REG_R0].as.integer_value = integer_result;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }


    if (strcmp(builtin_name, "__builtin_str_len") == 0) {
        int64_t str_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);
        size_t len = str ? strlen(str) : 0;

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = (int64_t)len;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_cmp") == 0) {
        int64_t str1_addr = 0, str2_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str1_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &str2_addr);

        IjklVmRegisterValue val1, val2;
        val1.type = VM_VALUE_TYPE_STRING; val1.as.integer_value = str1_addr;
        val2.type = VM_VALUE_TYPE_STRING; val2.as.integer_value = str2_addr;
        const char* s1 = resolve_string_pointer(machine, &val1);
        const char* s2 = resolve_string_pointer(machine, &val2);

        int cmp_res = 0;
        if (s1 && s2) {
            cmp_res = strcmp(s1, s2);
        } else if (s1 && !s2) {
            cmp_res = 1;
        } else if (!s1 && s2) {
            cmp_res = -1;
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = (int64_t)cmp_res;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_concat") == 0) {
        int64_t str1_addr = 0, str2_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str1_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &str2_addr);

        IjklVmRegisterValue val1, val2;
        val1.type = VM_VALUE_TYPE_STRING; val1.as.integer_value = str1_addr;
        val2.type = VM_VALUE_TYPE_STRING; val2.as.integer_value = str2_addr;
        const char* s1 = resolve_string_pointer(machine, &val1);
        const char* s2 = resolve_string_pointer(machine, &val2);
        if (!s1) s1 = "";
        if (!s2) s2 = "";

        size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        size_t total_len = len1 + len2 + 1;

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, total_len);
        if (new_str_addr == 0) return false;

        memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], s1, len1);
        memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr + len1], s2, len2);
        machine->memory.raw_linear_memory_buffer[new_str_addr + len1 + len2] = '\0';

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_sub") == 0) {
        int64_t source_string_address = 0;
        int64_t start_index = 0;
        int64_t substring_length = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &source_string_address);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &start_index);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 16, &substring_length);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = source_string_address;
        const char* source_string = resolve_string_pointer(machine, &temp_val);

        size_t source_length = source_string ? strlen(source_string) : 0;
        if (start_index < 0) start_index = 0;
        if (substring_length < 0) substring_length = 0;

        size_t actual_start = (size_t)start_index;
        size_t actual_length = (size_t)substring_length;

        if (actual_start > source_length) {
            actual_start = source_length;
            actual_length = 0;
        } else if (actual_start + actual_length > source_length) {
            actual_length = source_length - actual_start;
        }

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, actual_length + 1);
        if (new_str_addr == 0) return false;

        if (actual_length > 0 && source_string) {
            memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], source_string + actual_start, actual_length);
        }
        machine->memory.raw_linear_memory_buffer[new_str_addr + actual_length] = '\0';

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_int_to_str") == 0) {
        int64_t int_val = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &int_val);

        char temp_buffer[64];
        snprintf(temp_buffer, sizeof(temp_buffer), "%lld", (long long)int_val);
        size_t str_len = strlen(temp_buffer) + 1;

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, str_len);
        if (new_str_addr == 0) return false;

        memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], temp_buffer, str_len);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_real_to_str") == 0) {
        double float_val = 0.0;
        ijkl_vm_memory_read_float(&machine->memory, args_base, &float_val);

        char temp_buffer[64];
        snprintf(temp_buffer, sizeof(temp_buffer), "%.6f", float_val);
        char* dot = strchr(temp_buffer, '.');
        if (dot) {
            char* end = temp_buffer + strlen(temp_buffer) - 1;
            while (end > dot + 1 && *end == '0') {
                *end = '\0';
                end--;
            }
        }
        size_t str_len = strlen(temp_buffer) + 1;

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, str_len);
        if (new_str_addr == 0) return false;

        memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], temp_buffer, str_len);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_to_int") == 0) {
        int64_t str_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);

        int64_t parsed_int = 0;
        if (str) {
            char* end_ptr = NULL;
            parsed_int = (int64_t)strtoll(str, &end_ptr, 10);
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = parsed_int;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_to_real") == 0) {
        int64_t str_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);

        double parsed_real = 0.0;
        if (str) {
            char* end_ptr = NULL;
            parsed_real = strtod(str, &end_ptr);
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_FLOAT;
        machine->registers[IJKL_VM_REG_R0].as.float_value = parsed_real;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_to_bool") == 0) {
        int64_t str_addr = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);

        bool bool_val = false;
        if (str) {
            if (strcmp(str, "1") == 0 ||
                ijkl_string_case_insensitive_equals(str, "true") ||
                ijkl_string_case_insensitive_equals(str, "verdadeiro") ||
                ijkl_string_case_insensitive_equals(str, "vrai")) {
                bool_val = true;
            }
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_BOOL;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = bool_val ? 1 : 0;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_bool_to_str") == 0) {
        int64_t bool_int = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &bool_int);

        const char* text_val = bool_int ? "verdadeiro" : "falso";
        size_t str_len = strlen(text_val) + 1;

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, str_len);
        if (new_str_addr == 0) return false;

        memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], text_val, str_len);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_char_to_str") == 0) {
        int64_t char_code = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &char_code);

        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, 2);
        if (new_str_addr == 0) return false;

        machine->memory.raw_linear_memory_buffer[new_str_addr] = (uint8_t)char_code;
        machine->memory.raw_linear_memory_buffer[new_str_addr + 1] = '\0';

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_create") == 0) {
        int64_t length = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &length);
        if (length < 0) length = 0;

        size_t total_alloc = (size_t)length + 1;
        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, total_alloc);
        if (new_str_addr == 0) return false;

        memset(&machine->memory.raw_linear_memory_buffer[new_str_addr], ' ', total_alloc - 1);
        machine->memory.raw_linear_memory_buffer[new_str_addr + length] = '\0';

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_get_char") == 0) {
        int64_t str_addr = 0;
        int64_t index = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &index);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);

        char result_char = '\0';
        if (str && index >= 0) {
            size_t str_len = strlen(str);
            if ((size_t)index < str_len) {
                result_char = str[index];
            }
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = (unsigned char)result_char;

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_set_char") == 0) {
        int64_t str_addr = 0;
        int64_t index = 0;
        int64_t new_char_val = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &str_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &index);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 16, &new_char_val);

        IjklVmRegisterValue temp_val;
        temp_val.type = VM_VALUE_TYPE_STRING;
        temp_val.as.integer_value = str_addr;
        const char* str = resolve_string_pointer(machine, &temp_val);

        /*
         * Semântica Pura e Imutável:
         * Strings são imutáveis; aloca-se uma nova cadeia no heap com a cópia dos caracteres
         * originais e a substituição pontual do caractere na posição 'index' solicitada.
         */
        size_t str_len = str ? strlen(str) : 0;
        uint64_t new_str_addr = ijkl_vm_memory_allocate_heap(&machine->memory, str_len + 1);
        if (new_str_addr == 0) return false;

        if (str && str_len > 0) {
            memcpy(&machine->memory.raw_linear_memory_buffer[new_str_addr], str, str_len);
        }
        if (str && index >= 0 && (size_t)index < str_len) {
            machine->memory.raw_linear_memory_buffer[new_str_addr + index] = (uint8_t)new_char_val;
        }
        machine->memory.raw_linear_memory_buffer[new_str_addr + str_len] = '\0';

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_str_addr;
        ijkl_vm_memory_set_tag(&machine->memory, new_str_addr, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_split") == 0) {
        int64_t string_address = 0;
        int64_t delimiter_value = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &string_address);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &delimiter_value);

        IjklVmRegisterValue temporary_value;
        temporary_value.type = VM_VALUE_TYPE_STRING;
        temporary_value.as.integer_value = string_address;
        const char* source_string = resolve_string_pointer(machine, &temporary_value);
        char delimiter_character = (char)delimiter_value;

        size_t string_length = source_string ? strlen(source_string) : 0;
        if (string_length == 0) {
            uint64_t empty_list_address = create_vm_list(machine, NULL, 0);
            machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
            machine->registers[IJKL_VM_REG_R0].as.unsigned_value = empty_list_address;
            machine->memory.current_stack_pointer = machine->current_frame_top;
            return true;
        }

        size_t items_capacity = 8;
        size_t items_count = 0;
        uint64_t* items_array = (uint64_t*)malloc(items_capacity * sizeof(uint64_t));
        if (!items_array) return false;

        size_t segment_start_index = 0;
        for (size_t scan_index = 0; scan_index < string_length; ++scan_index) {
            if (source_string[scan_index] == delimiter_character) {
                size_t segment_length = scan_index - segment_start_index;
                uint64_t segment_string_address = create_vm_string(machine, &source_string[segment_start_index], segment_length);
                if (items_count >= items_capacity) {
                    items_capacity *= 2;
                    uint64_t* reallocated_buffer = (uint64_t*)realloc(items_array, items_capacity * sizeof(uint64_t));
                    if (!reallocated_buffer) { free(items_array); return false; }
                    items_array = reallocated_buffer;
                }
                items_array[items_count++] = segment_string_address;
                segment_start_index = scan_index + 1;
            }
        }

        size_t remaining_segment_length = string_length - segment_start_index;
        uint64_t last_segment_address = create_vm_string(machine, &source_string[segment_start_index], remaining_segment_length);
        if (items_count >= items_capacity) {
            items_capacity += 1;
            uint64_t* reallocated_buffer = (uint64_t*)realloc(items_array, items_capacity * sizeof(uint64_t));
            if (!reallocated_buffer) { free(items_array); return false; }
            items_array = reallocated_buffer;
        }
        items_array[items_count++] = last_segment_address;

        uint64_t result_list_address = create_vm_list(machine, items_array, items_count);
        free(items_array);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = result_list_address;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_str_split_lines") == 0) {
        int64_t string_address = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &string_address);

        IjklVmRegisterValue temporary_value;
        temporary_value.type = VM_VALUE_TYPE_STRING;
        temporary_value.as.integer_value = string_address;
        const char* source_string = resolve_string_pointer(machine, &temporary_value);

        size_t string_length = source_string ? strlen(source_string) : 0;
        if (string_length == 0) {
            uint64_t empty_list_address = create_vm_list(machine, NULL, 0);
            machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
            machine->registers[IJKL_VM_REG_R0].as.unsigned_value = empty_list_address;
            machine->memory.current_stack_pointer = machine->current_frame_top;
            return true;
        }

        size_t items_capacity = 8;
        size_t items_count = 0;
        uint64_t* items_array = (uint64_t*)malloc(items_capacity * sizeof(uint64_t));
        if (!items_array) return false;

        size_t segment_start_index = 0;
        size_t scan_index = 0;
        while (scan_index < string_length) {
            char current_character = source_string[scan_index];
            if (current_character == '\n' || current_character == '\r') {
                size_t segment_length = scan_index - segment_start_index;
                uint64_t segment_string_address = create_vm_string(machine, &source_string[segment_start_index], segment_length);
                if (items_count >= items_capacity) {
                    items_capacity *= 2;
                    uint64_t* reallocated_buffer = (uint64_t*)realloc(items_array, items_capacity * sizeof(uint64_t));
                    if (!reallocated_buffer) { free(items_array); return false; }
                    items_array = reallocated_buffer;
                }
                items_array[items_count++] = segment_string_address;

                if (current_character == '\r' && scan_index + 1 < string_length && source_string[scan_index + 1] == '\n') {
                    scan_index++;
                }
                segment_start_index = scan_index + 1;
            }
            scan_index++;
        }

        if (segment_start_index < string_length) {
            size_t remaining_segment_length = string_length - segment_start_index;
            uint64_t last_segment_address = create_vm_string(machine, &source_string[segment_start_index], remaining_segment_length);
            if (items_count >= items_capacity) {
                items_capacity += 1;
                uint64_t* reallocated_buffer = (uint64_t*)realloc(items_array, items_capacity * sizeof(uint64_t));
                if (!reallocated_buffer) { free(items_array); return false; }
                items_array = reallocated_buffer;
            }
            items_array[items_count++] = last_segment_address;
        }

        uint64_t result_list_address = create_vm_list(machine, items_array, items_count);
        free(items_array);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = result_list_address;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_mem_copy") == 0) {
        int64_t dest_addr = 0, src_addr = 0, count = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &dest_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &src_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 16, &count);

        if (count > 0) {
            if (!ijkl_vm_memory_is_valid_address(&machine->memory, (uint64_t)dest_addr, (size_t)count) ||
                !ijkl_vm_memory_is_valid_address(&machine->memory, (uint64_t)src_addr, (size_t)count)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION, "Out of bounds in __builtin_mem_copy", 0);
                return false;
            }
            memmove(&machine->memory.raw_linear_memory_buffer[dest_addr],
                    &machine->memory.raw_linear_memory_buffer[src_addr],
                    (size_t)count);
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = dest_addr;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_mem_fill") == 0) {
        int64_t dest_addr = 0, val = 0, count = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &dest_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &val);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 16, &count);

        if (count > 0) {
            if (!ijkl_vm_memory_is_valid_address(&machine->memory, (uint64_t)dest_addr, (size_t)count)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION, "Out of bounds in __builtin_mem_fill", 0);
                return false;
            }
            memset(&machine->memory.raw_linear_memory_buffer[dest_addr],
                   (uint8_t)(val & 0xFF),
                   (size_t)count);
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = dest_addr;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_mem_cmp") == 0) {
        int64_t buf1_addr = 0, buf2_addr = 0, count = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &buf1_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 8, &buf2_addr);
        ijkl_vm_memory_read_integer(&machine->memory, args_base + 16, &count);

        int cmp_res = 0;
        if (count > 0) {
            if (!ijkl_vm_memory_is_valid_address(&machine->memory, (uint64_t)buf1_addr, (size_t)count) ||
                !ijkl_vm_memory_is_valid_address(&machine->memory, (uint64_t)buf2_addr, (size_t)count)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION, "Out of bounds in __builtin_mem_cmp", 0);
                return false;
            }
            int c = memcmp(&machine->memory.raw_linear_memory_buffer[buf1_addr],
                           &machine->memory.raw_linear_memory_buffer[buf2_addr],
                           (size_t)count);
            cmp_res = (c < 0) ? -1 : ((c > 0) ? 1 : 0);
        }

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = cmp_res;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_datetime_now") == 0) {
        int64_t epoch_seconds = (int64_t)time(NULL);
        int64_t millisecond_fraction = 0;
        int64_t timezone_offset_seconds = 0;
#ifdef _WIN32
        SYSTEMTIME system_time;
        GetSystemTime(&system_time);
        millisecond_fraction = (int64_t)system_time.wMilliseconds;

        TIME_ZONE_INFORMATION timezone_information;
        DWORD timezone_result = GetTimeZoneInformation(&timezone_information);
        long total_bias_minutes = timezone_information.Bias;
        if (timezone_result == TIME_ZONE_ID_DAYLIGHT) {
            total_bias_minutes += timezone_information.DaylightBias;
        } else if (timezone_result == TIME_ZONE_ID_STANDARD) {
            total_bias_minutes += timezone_information.StandardBias;
        }
        timezone_offset_seconds = (int64_t)(-total_bias_minutes * 60);
#else
        struct timespec time_spec;
        if (clock_gettime(CLOCK_REALTIME, &time_spec) == 0) {
            millisecond_fraction = (int64_t)(time_spec.tv_nsec / 1000000L);
        }
        struct tm local_time_info;
        time_t time_now = (time_t)epoch_seconds;
        if (!localtime_r(&time_now, &local_time_info)) return false;
            struct tm utc_time_info;
            if (!gmtime_r(&time_now, &utc_time_info)) return false;
            /* A diferença civil preserva o horário de verão sem mktime ambíguo. */
            int day_difference = local_time_info.tm_yday - utc_time_info.tm_yday;
            if (local_time_info.tm_year > utc_time_info.tm_year) day_difference = 1;
            if (local_time_info.tm_year < utc_time_info.tm_year) day_difference = -1;
            timezone_offset_seconds = (int64_t)day_difference * 86400 +
                (local_time_info.tm_hour - utc_time_info.tm_hour) * 3600 +
                (local_time_info.tm_min - utc_time_info.tm_min) * 60 +
                local_time_info.tm_sec - utc_time_info.tm_sec;
#endif
        uint64_t datetime_address = build_datetime_instance(machine, epoch_seconds, millisecond_fraction, timezone_offset_seconds);
        if (datetime_address == 0) return false;

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = datetime_address;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_datetime_utc_now") == 0) {
        int64_t epoch_seconds = (int64_t)time(NULL);
        int64_t millisecond_fraction = 0;
#ifdef _WIN32
        SYSTEMTIME system_time;
        GetSystemTime(&system_time);
        millisecond_fraction = (int64_t)system_time.wMilliseconds;
#else
        struct timespec time_spec;
        if (clock_gettime(CLOCK_REALTIME, &time_spec) == 0) {
            millisecond_fraction = (int64_t)(time_spec.tv_nsec / 1000000L);
        }
#endif
        uint64_t datetime_address = build_datetime_instance(machine, epoch_seconds, millisecond_fraction, 0);
        if (datetime_address == 0) return false;

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = datetime_address;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_datetime_from_unix_timestamp") == 0) {
        int64_t timestamp_seconds = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &timestamp_seconds);

        uint64_t datetime_address = build_datetime_instance(machine, timestamp_seconds, 0, 0);
        if (datetime_address == 0) return false;

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = datetime_address;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_datetime_elapsed_seconds_of_day") == 0) {
        int64_t datetime_address = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &datetime_address);
        if (datetime_address == 0) {
            machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
            machine->registers[IJKL_VM_REG_R0].as.integer_value = 0;
            machine->memory.current_stack_pointer = machine->current_frame_top;
            return true;
        }

        int64_t hour = 0, minute = 0, second = 0;
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 24, &hour);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 32, &minute);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 40, &second);

        int64_t elapsed_seconds = (hour * 3600) + (minute * 60) + second;

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
        machine->registers[IJKL_VM_REG_R0].as.integer_value = elapsed_seconds;
        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    if (strcmp(builtin_name, "__builtin_datetime_to_string") == 0) {
        int64_t datetime_address = 0;
        ijkl_vm_memory_read_integer(&machine->memory, args_base, &datetime_address);
        if (datetime_address == 0) {
            uint64_t empty_string_address = ijkl_vm_memory_allocate_heap(&machine->memory, 1);
            if (empty_string_address != 0) {
                machine->memory.raw_linear_memory_buffer[empty_string_address] = '\0';
                ijkl_vm_memory_set_tag(&machine->memory, empty_string_address, VM_VALUE_TYPE_STRING);
            }
            machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
            machine->registers[IJKL_VM_REG_R0].as.unsigned_value = empty_string_address;
            machine->memory.current_stack_pointer = machine->current_frame_top;
            return true;
        }

        int64_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 0, &year);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 8, &month);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 16, &day);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 24, &hour);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 32, &minute);
        ijkl_vm_memory_read_integer(&machine->memory, (uint64_t)datetime_address + 40, &second);

        char formatted_buffer[32];
        snprintf(formatted_buffer, sizeof(formatted_buffer), "%04lld-%02lld-%02lld %02lld:%02lld:%02lld",
                 (long long)year, (long long)month, (long long)day,
                 (long long)hour, (long long)minute, (long long)second);

        size_t string_length_with_null = strlen(formatted_buffer) + 1;
        uint64_t new_string_address = ijkl_vm_memory_allocate_heap(&machine->memory, string_length_with_null);
        if (new_string_address == 0) return false;

        memcpy(&machine->memory.raw_linear_memory_buffer[new_string_address], formatted_buffer, string_length_with_null);

        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
        machine->registers[IJKL_VM_REG_R0].as.unsigned_value = new_string_address;
        ijkl_vm_memory_set_tag(&machine->memory, new_string_address, VM_VALUE_TYPE_STRING);

        machine->memory.current_stack_pointer = machine->current_frame_top;
        return true;
    }

    return false;
}

/*
 * EFEITO COLATERAL:
 * Executa uma única instrução (ciclo de busca, decodificação e despacho).
 */
bool ijkl_virtual_machine_step(IjklVirtualMachine* machine) {
    if (!machine || machine->execution_state == VM_STATE_HALTED || machine->execution_state == VM_STATE_ERROR) {
        return false;
    }

    if (machine->instruction_pointer >= machine->instructions_count) {
        machine->execution_state = VM_STATE_HALTED;
        return false;
    }

    const IjklVmInstruction* current_instruction = &machine->instruction_stream[machine->instruction_pointer];
    machine->registers[IJKL_VM_REG_CI].type = VM_VALUE_TYPE_INT;
    machine->registers[IJKL_VM_REG_CI].as.unsigned_value = machine->instruction_pointer;

    size_t next_instruction_pointer = machine->instruction_pointer + 1;
    bool should_advance_pc = true;

    switch (current_instruction->opcode) {
        /* 1. Atribuição simples: dest := op1; */
        case IJKL_OPCODE_ASSIGN: {
            IjklVmRegisterValue value;
            if (!evaluate_operand(machine, &current_instruction->first_operand, &value)) {
                set_runtime_error(machine, VM_ERROR_MEMORY_ACCESS_VIOLATION,
                                  "Could not evaluate source operand in assignment.", current_instruction->source_line_number);
                return false;
            }
            write_destination_operand(machine, &current_instruction->destination, value);
            break;
        }

        /* 2. Operações Aritméticas e Lógicas */
        case IJKL_OPCODE_ADD:
        case IJKL_OPCODE_SUB:
        case IJKL_OPCODE_MUL:
        case IJKL_OPCODE_DIV:
        case IJKL_OPCODE_MOD:
        case IJKL_OPCODE_EXP:
        case IJKL_OPCODE_EQUAL:
        case IJKL_OPCODE_NOT_EQUAL:
        case IJKL_OPCODE_LESS:
        case IJKL_OPCODE_LESS_EQUAL:
        case IJKL_OPCODE_GREATER:
        case IJKL_OPCODE_GREATER_EQUAL:
        case IJKL_OPCODE_LOGICAL_AND:
        case IJKL_OPCODE_LOGICAL_OR:
        case IJKL_OPCODE_LOGICAL_XOR:
        case IJKL_OPCODE_BITWISE_AND:
        case IJKL_OPCODE_BITWISE_OR:
        case IJKL_OPCODE_SHIFT_LEFT:
        case IJKL_OPCODE_SHIFT_RIGHT: {
            IjklVmRegisterValue operand1;
            IjklVmRegisterValue operand2;
            evaluate_operand(machine, &current_instruction->first_operand, &operand1);
            evaluate_operand(machine, &current_instruction->second_operand, &operand2);

            IjklVmRegisterValue result;
            memset(&result, 0, sizeof(IjklVmRegisterValue));

            bool is_string_comparison = (operand1.type == VM_VALUE_TYPE_STRING && operand2.type == VM_VALUE_TYPE_STRING);
            bool is_floating_point = (operand1.type == VM_VALUE_TYPE_FLOAT || operand2.type == VM_VALUE_TYPE_FLOAT);

            if (is_string_comparison) {
                const char* s1 = resolve_string_pointer(machine, &operand1);
                const char* s2 = resolve_string_pointer(machine, &operand2);
                if (!s1) s1 = "";
                if (!s2) s2 = "";
                int cmp = strcmp(s1, s2);
                result.type = VM_VALUE_TYPE_INT;
                switch (current_instruction->opcode) {
                    case IJKL_OPCODE_EQUAL:         result.as.integer_value = (cmp == 0) ? 1 : 0; break;
                    case IJKL_OPCODE_NOT_EQUAL:     result.as.integer_value = (cmp != 0) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS:          result.as.integer_value = (cmp < 0) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS_EQUAL:    result.as.integer_value = (cmp <= 0) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER:       result.as.integer_value = (cmp > 0) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER_EQUAL: result.as.integer_value = (cmp >= 0) ? 1 : 0; break;
                    default: break;
                }
            } else if (is_floating_point) {
                double val1 = (operand1.type == VM_VALUE_TYPE_FLOAT) ? operand1.as.float_value : (double)operand1.as.integer_value;
                double val2 = (operand2.type == VM_VALUE_TYPE_FLOAT) ? operand2.as.float_value : (double)operand2.as.integer_value;

                switch (current_instruction->opcode) {
                    case IJKL_OPCODE_ADD: result.type = VM_VALUE_TYPE_FLOAT; result.as.float_value = val1 + val2; break;
                    case IJKL_OPCODE_SUB: result.type = VM_VALUE_TYPE_FLOAT; result.as.float_value = val1 - val2; break;
                    case IJKL_OPCODE_MUL: result.type = VM_VALUE_TYPE_FLOAT; result.as.float_value = val1 * val2; break;
                    case IJKL_OPCODE_DIV:
                        if (val2 == 0.0) {
                            set_runtime_error(machine, VM_ERROR_DIVISION_BY_ZERO, "Division by zero (float).", current_instruction->source_line_number);
                            return false;
                        }
                        result.type = VM_VALUE_TYPE_FLOAT;
                        result.as.float_value = val1 / val2;
                        break;
                    case IJKL_OPCODE_EXP: result.type = VM_VALUE_TYPE_FLOAT; result.as.float_value = pow(val1, val2); break;
                    case IJKL_OPCODE_EQUAL:         result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 == val2) ? 1 : 0; break;
                    case IJKL_OPCODE_NOT_EQUAL:     result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 != val2) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS:          result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 < val2) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS_EQUAL:    result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 <= val2) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER:       result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 > val2) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER_EQUAL: result.type = VM_VALUE_TYPE_INT; result.as.integer_value = (val1 >= val2) ? 1 : 0; break;
                    default: break;
                }
            } else {
                int64_t int1 = operand1.as.integer_value;
                int64_t int2 = operand2.as.integer_value;
                result.type = VM_VALUE_TYPE_INT;

                switch (current_instruction->opcode) {
                    case IJKL_OPCODE_ADD: result.as.integer_value = int1 + int2; break;
                    case IJKL_OPCODE_SUB: result.as.integer_value = int1 - int2; break;
                    case IJKL_OPCODE_MUL: result.as.integer_value = int1 * int2; break;
                    case IJKL_OPCODE_DIV:
                        if (int2 == 0) {
                            set_runtime_error(machine, VM_ERROR_DIVISION_BY_ZERO, "Division by zero (integer).", current_instruction->source_line_number);
                            return false;
                        }
                        result.as.integer_value = int1 / int2;
                        break;
                    case IJKL_OPCODE_MOD:
                        if (int2 == 0) {
                            set_runtime_error(machine, VM_ERROR_DIVISION_BY_ZERO, "Modulo by zero.", current_instruction->source_line_number);
                            return false;
                        }
                        result.as.integer_value = int1 % int2;
                        break;
                    case IJKL_OPCODE_EXP: result.as.integer_value = (int64_t)pow((double)int1, (double)int2); break;
                    case IJKL_OPCODE_EQUAL:         result.as.integer_value = (int1 == int2) ? 1 : 0; break;
                    case IJKL_OPCODE_NOT_EQUAL:     result.as.integer_value = (int1 != int2) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS:          result.as.integer_value = (int1 < int2) ? 1 : 0; break;
                    case IJKL_OPCODE_LESS_EQUAL:    result.as.integer_value = (int1 <= int2) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER:       result.as.integer_value = (int1 > int2) ? 1 : 0; break;
                    case IJKL_OPCODE_GREATER_EQUAL: result.as.integer_value = (int1 >= int2) ? 1 : 0; break;
                    case IJKL_OPCODE_LOGICAL_AND:   result.as.integer_value = (int1 != 0 && int2 != 0) ? 1 : 0; break;
                    case IJKL_OPCODE_LOGICAL_OR:    result.as.integer_value = (int1 != 0 || int2 != 0) ? 1 : 0; break;
                    case IJKL_OPCODE_LOGICAL_XOR:   result.as.integer_value = ((int1 != 0) ^ (int2 != 0)) ? 1 : 0; break;
                    case IJKL_OPCODE_BITWISE_AND:   result.as.integer_value = int1 & int2; break;
                    case IJKL_OPCODE_BITWISE_OR:    result.as.integer_value = int1 | int2; break;
                    case IJKL_OPCODE_SHIFT_LEFT:    result.as.integer_value = int1 << int2; break;
                    case IJKL_OPCODE_SHIFT_RIGHT:   result.as.integer_value = int1 >> int2; break;
                    default: break;
                }
            }

            write_destination_operand(machine, &current_instruction->destination, result);
            break;
        }

        /* 3. Operadores Unários */
        case IJKL_OPCODE_NEGATE: {
            IjklVmRegisterValue operand1;
            evaluate_operand(machine, &current_instruction->first_operand, &operand1);
            if (operand1.type == VM_VALUE_TYPE_FLOAT) {
                operand1.as.float_value = -operand1.as.float_value;
            } else {
                operand1.as.integer_value = -operand1.as.integer_value;
            }
            write_destination_operand(machine, &current_instruction->destination, operand1);
            break;
        }

        case IJKL_OPCODE_LOGICAL_NOT: {
            IjklVmRegisterValue operand1;
            evaluate_operand(machine, &current_instruction->first_operand, &operand1);
            operand1.type = VM_VALUE_TYPE_INT;
            operand1.as.integer_value = (operand1.as.integer_value == 0) ? 1 : 0;
            write_destination_operand(machine, &current_instruction->destination, operand1);
            break;
        }

        /* 4. Alocação e Desalocação no Heap */
        case IJKL_OPCODE_ALLOC: {
            IjklVmRegisterValue size_val;
            evaluate_operand(machine, &current_instruction->first_operand, &size_val);
            uint64_t allocated_ptr = ijkl_vm_memory_allocate_heap(&machine->memory, (size_t)size_val.as.integer_value);
            IjklVmRegisterValue ptr_val;
            ptr_val.type = VM_VALUE_TYPE_INT;
            ptr_val.as.unsigned_value = allocated_ptr;
            write_destination_operand(machine, &current_instruction->destination, ptr_val);
            break;
        }

        case IJKL_OPCODE_FREE: {
            IjklVmRegisterValue ptr_val;
            evaluate_operand(machine, &current_instruction->first_operand, &ptr_val);
            ijkl_vm_memory_free_heap(&machine->memory, ptr_val.as.unsigned_value);
            break;
        }

        /* 5. Controle de Fluxo: jump, jump_if, call, return, halt */
        case IJKL_OPCODE_JUMP: {
            next_instruction_pointer = current_instruction->destination.value.label_reference.target_instruction_index;
            should_advance_pc = false;
            break;
        }

        case IJKL_OPCODE_JUMP_IF: {
            IjklVmRegisterValue cond;
            evaluate_operand(machine, &current_instruction->first_operand, &cond);
            bool is_condition_true = (cond.type == VM_VALUE_TYPE_FLOAT) ?
                (cond.as.float_value != 0.0) : (cond.as.integer_value != 0);

            if (is_condition_true) {
                next_instruction_pointer = current_instruction->destination.value.label_reference.target_instruction_index;
                should_advance_pc = false;
            }
            break;
        }

        case IJKL_OPCODE_CALL: {
            if (current_instruction->destination.value.label_reference.target_instruction_index == (size_t)-1) {
                const char* builtin_name = current_instruction->destination.value.label_reference.label_name;
                if (!execute_native_builtin(machine, builtin_name)) {
                    set_runtime_error(machine, VM_ERROR_UNRESOLVED_LABEL,
                                      "Failed executing native built-in routine.",
                                      current_instruction->source_line_number);
                    return false;
                }
                break;
            }

            if (machine->call_stack_depth >= IJKL_VM_MAX_CALL_STACK_DEPTH) {
                set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Call stack overflow (recursion too deep).", current_instruction->source_line_number);
                return false;
            }

            IjklCallFrame* frame = &machine->call_stack[machine->call_stack_depth++];
            frame->return_instruction_index = machine->instruction_pointer + 1;
            frame->previous_frame_pointer = machine->memory.current_frame_pointer;
            frame->caller_frame_top = machine->current_frame_top;
            frame->called_routine_name = current_instruction->destination.value.label_reference.label_name;

            next_instruction_pointer = current_instruction->destination.value.label_reference.target_instruction_index;
            should_advance_pc = false;
            break;
        }

        case IJKL_OPCODE_ALLOC_FRAME: {
            IjklVmRegisterValue frame_size_val;
            evaluate_operand(machine, &current_instruction->first_operand, &frame_size_val);
            size_t frame_local_size = (size_t)frame_size_val.as.integer_value;

            uint64_t new_frame_pointer = 0;
            if (machine->call_stack_depth > 0) {
                IjklCallFrame* frame = &machine->call_stack[machine->call_stack_depth - 1];
                new_frame_pointer = frame->caller_frame_top;
            } else {
                new_frame_pointer = machine->memory.stack_base_address;
            }

            size_t total_aligned_size = align_to_8_bytes(frame_local_size);
            uint64_t new_frame_top = new_frame_pointer + total_aligned_size;

            /* Zera as variáveis locais recém-alocadas além dos argumentos já empilhados */
            size_t arguments_bytes = (machine->memory.current_stack_pointer > new_frame_pointer) ?
                (size_t)(machine->memory.current_stack_pointer - new_frame_pointer) : 0;
            if (total_aligned_size > arguments_bytes &&
                new_frame_pointer + total_aligned_size <= machine->memory.total_allocated_capacity_bytes) {
                memset(&machine->memory.raw_linear_memory_buffer[new_frame_pointer + arguments_bytes],
                       0,
                       total_aligned_size - arguments_bytes);
            }

            machine->memory.current_frame_pointer = new_frame_pointer;
            machine->current_frame_top = new_frame_top;
            machine->memory.current_stack_pointer = new_frame_top;

            /* Atualiza registradores de hardware ra e sp */
            machine->registers[IJKL_VM_REG_RA].type = VM_VALUE_TYPE_INT;
            machine->registers[IJKL_VM_REG_RA].as.unsigned_value = machine->memory.current_frame_pointer;

            machine->registers[IJKL_VM_REG_SP].type = VM_VALUE_TYPE_INT;
            machine->registers[IJKL_VM_REG_SP].as.unsigned_value = machine->memory.current_stack_pointer;
            break;
        }

        case IJKL_OPCODE_FREE_FRAME: {
            if (machine->call_stack_depth > 0) {
                IjklCallFrame* frame = &machine->call_stack[machine->call_stack_depth - 1];
                machine->memory.current_stack_pointer = frame->caller_frame_top;
                machine->current_frame_top = frame->caller_frame_top;
                machine->memory.current_frame_pointer = frame->previous_frame_pointer;
            } else {
                machine->memory.current_stack_pointer = machine->memory.stack_base_address;
                machine->current_frame_top = machine->memory.stack_base_address;
                machine->memory.current_frame_pointer = machine->memory.stack_base_address;
            }

            machine->registers[IJKL_VM_REG_RA].as.unsigned_value = machine->memory.current_frame_pointer;
            machine->registers[IJKL_VM_REG_SP].as.unsigned_value = machine->memory.current_stack_pointer;
            break;
        }

        case IJKL_OPCODE_RETURN: {
            if (current_instruction->first_operand.kind != OPERAND_KIND_NONE) {
                IjklVmRegisterValue ret_val;
                evaluate_operand(machine, &current_instruction->first_operand, &ret_val);
                machine->registers[IJKL_VM_REG_R0] = ret_val;
            }
            if (machine->call_stack_depth > 0) {
                IjklCallFrame* frame = &machine->call_stack[--machine->call_stack_depth];
                if (frame->called_routine_name) {
                    if (strstr(frame->called_routine_name, "_to_str") != NULL ||
                        strstr(frame->called_routine_name, "str_concat") != NULL ||
                        strstr(frame->called_routine_name, "str_sub") != NULL ||
                        strstr(frame->called_routine_name, "substring") != NULL ||
                        strstr(frame->called_routine_name, "str_create") != NULL ||
                        strstr(frame->called_routine_name, "create") != NULL ||
                        strstr(frame->called_routine_name, "readAllText") != NULL ||
                        strstr(frame->called_routine_name, "lerTodoTexto") != NULL ||
                        strstr(frame->called_routine_name, "getCurrent") != NULL ||
                        strstr(frame->called_routine_name, "obterDiretorioAtual") != NULL) {
                        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_STRING;
                        uint64_t addr = machine->registers[IJKL_VM_REG_R0].as.unsigned_value;
                        if (addr >= IJKL_VM_HEAP_BASE_ADDRESS &&
                            addr < machine->memory.total_allocated_capacity_bytes) {
                            ijkl_vm_memory_set_tag(&machine->memory, addr, VM_VALUE_TYPE_STRING);
                        }
                    } else if (strstr(frame->called_routine_name, "_to_real") != NULL) {
                        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_FLOAT;
                    } else if (strstr(frame->called_routine_name, "_to_bool") != NULL) {
                        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_BOOL;
                    } else if (strstr(frame->called_routine_name, "_to_int") != NULL ||
                               strstr(frame->called_routine_name, "str_len") != NULL ||
                               strstr(frame->called_routine_name, "str_cmp") != NULL) {
                        machine->registers[IJKL_VM_REG_R0].type = VM_VALUE_TYPE_INT;
                    }
                }
                next_instruction_pointer = frame->return_instruction_index;
                should_advance_pc = false;
            } else {
                machine->execution_state = VM_STATE_HALTED;
                return false;
            }
            break;
        }

        case IJKL_OPCODE_HALT: {
            machine->execution_state = VM_STATE_HALTED;
            return false;
        }

        /* 6. Pilha: push e pop */
        case IJKL_OPCODE_PUSH: {
            IjklVmRegisterValue push_val;
            evaluate_operand(machine, &current_instruction->first_operand, &push_val);
            uint64_t target_stack_address = machine->memory.current_stack_pointer;

            if (push_val.type == VM_VALUE_TYPE_FLOAT) {
                if (!ijkl_vm_memory_write_float(&machine->memory, target_stack_address, push_val.as.float_value)) {
                    set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Stack overflow on push (float).", current_instruction->source_line_number);
                    return false;
                }
                ijkl_vm_memory_set_tag(&machine->memory, target_stack_address, VM_VALUE_TYPE_FLOAT);
            } else if (push_val.type == VM_VALUE_TYPE_STRING) {
                if (!ijkl_vm_memory_write_integer(&machine->memory, target_stack_address, (int64_t)push_val.as.unsigned_value)) {
                    set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Stack overflow on push (string).", current_instruction->source_line_number);
                    return false;
                }
                ijkl_vm_memory_set_tag(&machine->memory, target_stack_address, VM_VALUE_TYPE_STRING);
            } else if (push_val.type == VM_VALUE_TYPE_CHAR) {
                if (!ijkl_vm_memory_write_integer(&machine->memory, target_stack_address, push_val.as.integer_value)) {
                    set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Stack overflow on push (char).", current_instruction->source_line_number);
                    return false;
                }
                ijkl_vm_memory_set_tag(&machine->memory, target_stack_address, VM_VALUE_TYPE_CHAR);
            } else if (push_val.type == VM_VALUE_TYPE_BOOL) {
                if (!ijkl_vm_memory_write_integer(&machine->memory, target_stack_address, push_val.as.integer_value ? 1 : 0)) {
                    set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Stack overflow on push (bool).", current_instruction->source_line_number);
                    return false;
                }
                ijkl_vm_memory_set_tag(&machine->memory, target_stack_address, VM_VALUE_TYPE_BOOL);
            } else {
                if (!ijkl_vm_memory_write_integer(&machine->memory, target_stack_address, push_val.as.integer_value)) {
                    set_runtime_error(machine, VM_ERROR_STACK_OVERFLOW, "Stack overflow on push (int).", current_instruction->source_line_number);
                    return false;
                }
                ijkl_vm_memory_set_tag(&machine->memory, target_stack_address, VM_VALUE_TYPE_INT);
            }

            machine->memory.current_stack_pointer += sizeof(int64_t);
            machine->registers[IJKL_VM_REG_SP].as.unsigned_value = machine->memory.current_stack_pointer;
            break;
        }

        case IJKL_OPCODE_POP: {
            if (machine->memory.current_stack_pointer < machine->memory.stack_base_address + sizeof(int64_t)) {
                set_runtime_error(machine, VM_ERROR_STACK_UNDERFLOW, "Stack underflow on pop.", current_instruction->source_line_number);
                return false;
            }

            machine->memory.current_stack_pointer -= sizeof(int64_t);
            uint64_t target_stack_address = machine->memory.current_stack_pointer;
            uint8_t tag = ijkl_vm_memory_get_tag(&machine->memory, target_stack_address);

            IjklVmRegisterValue pop_val;
            if (tag == VM_VALUE_TYPE_FLOAT) {
                double float_val = 0.0;
                ijkl_vm_memory_read_float(&machine->memory, target_stack_address, &float_val);
                pop_val.type = VM_VALUE_TYPE_FLOAT;
                pop_val.as.float_value = float_val;
            } else if (tag == VM_VALUE_TYPE_STRING) {
                int64_t ptr_val = 0;
                ijkl_vm_memory_read_integer(&machine->memory, target_stack_address, &ptr_val);
                pop_val.type = VM_VALUE_TYPE_STRING;
                pop_val.as.string_pointer = (const char*)(uintptr_t)ptr_val;
                pop_val.as.unsigned_value = (uint64_t)ptr_val;
            } else if (tag == VM_VALUE_TYPE_CHAR) {
                int64_t int_val = 0;
                ijkl_vm_memory_read_integer(&machine->memory, target_stack_address, &int_val);
                pop_val.type = VM_VALUE_TYPE_CHAR;
                pop_val.as.integer_value = int_val;
            } else if (tag == VM_VALUE_TYPE_BOOL) {
                int64_t int_val = 0;
                ijkl_vm_memory_read_integer(&machine->memory, target_stack_address, &int_val);
                pop_val.type = VM_VALUE_TYPE_BOOL;
                pop_val.as.integer_value = (int_val != 0) ? 1 : 0;
            } else {
                int64_t int_val = 0;
                ijkl_vm_memory_read_integer(&machine->memory, target_stack_address, &int_val);
                pop_val.type = VM_VALUE_TYPE_INT;
                pop_val.as.integer_value = int_val;
            }

            write_destination_operand(machine, &current_instruction->destination, pop_val);
            machine->registers[IJKL_VM_REG_SP].as.unsigned_value = machine->memory.current_stack_pointer;
            break;
        }

        /* 7. Barramento I/O: in e out */
        case IJKL_OPCODE_IN: {
            IjklVmRegisterValue port_val;
            evaluate_operand(machine, &current_instruction->first_operand, &port_val);
            uint16_t port_address = (uint16_t)port_val.as.integer_value;

            int64_t input_result = 0;
            if (!ijkl_vm_io_bus_read_port(&machine->io_bus, port_address, &input_result)) {
                set_runtime_error(machine, VM_ERROR_IO_PORT_FAULT, "Failed reading from I/O port.", current_instruction->source_line_number);
                return false;
            }

            IjklVmRegisterValue in_reg_val;
            if (port_address == IJKL_VM_IO_PORT_CONSOLE_REAL) {
                in_reg_val.type = VM_VALUE_TYPE_FLOAT;
                in_reg_val.as.integer_value = input_result;
            } else if (port_address == IJKL_VM_IO_PORT_FILE_READ_ALL || port_address == IJKL_VM_IO_PORT_FOLDER_GET_CURR) {
                in_reg_val.type = VM_VALUE_TYPE_STRING;
                in_reg_val.as.unsigned_value = (uint64_t)input_result;
                if ((uint64_t)input_result >= IJKL_VM_HEAP_BASE_ADDRESS &&
                    (uint64_t)input_result < machine->memory.total_allocated_capacity_bytes) {
                    ijkl_vm_memory_set_tag(&machine->memory, (uint64_t)input_result, VM_VALUE_TYPE_STRING);
                }
            } else if ((port_address >= IJKL_VM_IO_PORT_FILE_EXISTS && port_address <= IJKL_VM_IO_PORT_FILE_COPY) ||
                       (port_address >= IJKL_VM_IO_PORT_FOLDER_EXISTS && port_address <= IJKL_VM_IO_PORT_FOLDER_SET_CURR)) {
                in_reg_val.type = VM_VALUE_TYPE_BOOL;
                in_reg_val.as.integer_value = (input_result != 0) ? 1 : 0;
            } else {
                in_reg_val.type = VM_VALUE_TYPE_INT;
                in_reg_val.as.integer_value = input_result;
            }
            write_destination_operand(machine, &current_instruction->destination, in_reg_val);
            break;
        }

        case IJKL_OPCODE_OUT: {
            IjklVmRegisterValue port_val;
            IjklVmRegisterValue data_val;
            evaluate_operand(machine, &current_instruction->first_operand, &port_val);
            evaluate_operand(machine, &current_instruction->second_operand, &data_val);

            uint16_t port_address = (uint16_t)port_val.as.integer_value;
            const char* string_literal = (data_val.type == VM_VALUE_TYPE_STRING ||
                                          (port_address == IJKL_VM_IO_PORT_CONSOLE_WRITE &&
                                           data_val.as.unsigned_value >= IJKL_VM_PAGE_NULL_END_ADDRESS &&
                                           data_val.as.unsigned_value < machine->memory.total_allocated_capacity_bytes)) ?
                resolve_string_pointer(machine, &data_val) : NULL;

            /* Se a escrita for na porta de console 0x0001, formata conforme o tipo dinâmico */
            if (port_address == IJKL_VM_IO_PORT_CONSOLE_WRITE) {
                if (data_val.type == VM_VALUE_TYPE_CHAR) {
                    port_address = IJKL_VM_IO_PORT_CONSOLE_CHAR;
                } else if (data_val.type == VM_VALUE_TYPE_FLOAT) {
                    port_address = IJKL_VM_IO_PORT_CONSOLE_REAL;
                } else if (data_val.type == VM_VALUE_TYPE_BOOL) {
                    string_literal = data_val.as.integer_value ? "verdadeiro" : "falso";
                }
            }

            int64_t raw_data = 0;
            if (data_val.type == VM_VALUE_TYPE_FLOAT) {
                memcpy(&raw_data, &data_val.as.float_value, sizeof(double));
            } else {
                raw_data = data_val.as.integer_value;
            }

            if (!ijkl_vm_io_bus_write_port(&machine->io_bus, port_address, raw_data, string_literal)) {
                set_runtime_error(machine, VM_ERROR_IO_PORT_FAULT, "Failed writing to I/O port.", current_instruction->source_line_number);
                return false;
            }
            break;
        }

        case IJKL_OPCODE_ERROR: {
            IjklVmRegisterValue msg_val;
            evaluate_operand(machine, &current_instruction->first_operand, &msg_val);
            const char* msg = resolve_string_pointer(machine, &msg_val);
            if (!msg) msg = "Assertion failed";
            set_runtime_error(machine, VM_ERROR_USER_ASSERTION, msg, current_instruction->source_line_number);
            return false;
        }

        default:
            break;
    }

    if (should_advance_pc) {
        machine->instruction_pointer = next_instruction_pointer;
    } else {
        machine->instruction_pointer = next_instruction_pointer;
    }

    machine->total_executed_cycles_count++;
    return true;
}

/*
 * EFEITO COLATERAL:
 * Executa continuamente o programa até encontrar 'halt', erro ou conclusão.
 */
bool ijkl_virtual_machine_run(IjklVirtualMachine* machine) {
    if (!machine) return false;

    machine->execution_state = VM_STATE_RUNNING;

    while (machine->execution_state == VM_STATE_RUNNING) {
        if (!ijkl_virtual_machine_step(machine)) {
            break;
        }
    }

    return (machine->execution_state == VM_STATE_HALTED);
}

int64_t ijkl_virtual_machine_get_register_integer(const IjklVirtualMachine* machine,
                                                  IjklVmRegister register_identifier) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return 0;
    return machine->registers[register_identifier].as.integer_value;
}

void ijkl_virtual_machine_set_register_integer(IjklVirtualMachine* machine,
                                               IjklVmRegister register_identifier,
                                               int64_t integer_value) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return;
    machine->registers[register_identifier].type = VM_VALUE_TYPE_INT;
    machine->registers[register_identifier].as.integer_value = integer_value;
}

double ijkl_virtual_machine_get_register_float(const IjklVirtualMachine* machine,
                                               IjklVmRegister register_identifier) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return 0.0;
    return machine->registers[register_identifier].as.float_value;
}

void ijkl_virtual_machine_set_register_float(IjklVirtualMachine* machine,
                                             IjklVmRegister register_identifier,
                                             double float_value) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return;
    machine->registers[register_identifier].type = VM_VALUE_TYPE_FLOAT;
    machine->registers[register_identifier].as.float_value = float_value;
}

const char* ijkl_virtual_machine_get_register_string(const IjklVirtualMachine* machine,
                                                     IjklVmRegister register_identifier) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return NULL;
    return resolve_string_pointer(machine, &machine->registers[register_identifier]);
}

void ijkl_virtual_machine_set_register_string(IjklVirtualMachine* machine,
                                              IjklVmRegister register_identifier,
                                              const char* string_value) {
    if (!machine || register_identifier < 0 || register_identifier >= IJKL_VM_REG_COUNT) return;
    machine->registers[register_identifier].type = VM_VALUE_TYPE_STRING;
    machine->registers[register_identifier].as.string_pointer = string_value;
}

const char* ijkl_virtual_machine_get_error_message(const IjklVirtualMachine* machine) {
    if (!machine) return "Null virtual machine pointer.";
    return machine->error_message_buffer;
}

const char* ijkl_virtual_machine_error_code_to_string(IjklVmErrorCode error_code) {
    switch (error_code) {
        case VM_ERROR_NONE: return "No error";
        case VM_ERROR_DIVISION_BY_ZERO: return "Division by zero";
        case VM_ERROR_INVALID_OPCODE: return "Invalid opcode";
        case VM_ERROR_MEMORY_ACCESS_VIOLATION: return "Segmentation fault (memory access violation)";
        case VM_ERROR_STACK_OVERFLOW: return "Stack overflow";
        case VM_ERROR_STACK_UNDERFLOW: return "Stack underflow";
        case VM_ERROR_UNRESOLVED_LABEL: return "Unresolved label target";
        case VM_ERROR_IO_PORT_FAULT: return "I/O port fault";
        case VM_ERROR_USER_ASSERTION: return "User assertion error";
        case VM_ERROR_INSTRUCTION_OUT_OF_BOUNDS: return "Instruction pointer out of bounds";
        default: return "Unknown runtime error";
    }
}
