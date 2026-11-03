#ifndef IJKL_VM_IO_H
#define IJKL_VM_IO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * ==============================================================================
 * MÓDULO: BARRAMENTO DE ENTRADA E SAÍDA (I/O) DA VM (IJKLANG VM)
 * ==============================================================================
 * Implementa o barramento simulado de portas de 16 bits (0x0000 a 0xFFFF)
 * para integração das instruções 'in' e 'out' com o console padrão do terminal
 * e dispositivos periféricos externos (plugins .dll / .so).
 *
 * Mapeamento canônico das portas do sistema:
 * - 0x0000: Console Terminal - Saída / Entrada de Caractere Bruto UTF-8.
 * - 0x0001: Console Terminal - Saída de Dados Formatados (String / Número).
 * - 0x0002: Console Terminal - Entrada / Saída de Ponto Flutuante (Real / Float).
 * - 0x0003: Console Terminal - Entrada de Linha e Números Inteiros (Stdin).
 * - 0x0010 - 0x0013: Semáforo Didático (plugins/dev_semaforo.dll).
 * - 0x0020 - 0x0027: Display de 7 Segmentos (plugins/dev_display7.dll).
 * - 0x0030 - 0x0037: Robô Simulado em Malha 2D (plugins/dev_robo.dll).
 * ==============================================================================
 */

#define IJKL_VM_IO_PORT_CONSOLE_CHAR        0x0000
#define IJKL_VM_IO_PORT_CONSOLE_WRITE       0x0001
#define IJKL_VM_IO_PORT_CONSOLE_REAL        0x0002
#define IJKL_VM_IO_PORT_CONSOLE_READ        0x0003
#define IJKL_VM_IO_PORT_CONSOLE_READ_LINE   0x0004

/* Portas de controle de relógio e data/hora do sistema operacional */
#define IJKL_VM_IO_PORT_SYSTEM_TIME_SECONDS 0x0008
#define IJKL_VM_IO_PORT_SYSTEM_TIME_MILLIS  0x0009
#define IJKL_VM_IO_PORT_SYSTEM_TIME_TIMEZONE 0x000A

/* Portas de controle de transferência direta de memória (DMA) - Basys 3 FPGA */
#define IJKL_VM_IO_PORT_DMA_SOURCE        0x0020
#define IJKL_VM_IO_PORT_DMA_DESTINATION   0x0021
#define IJKL_VM_IO_PORT_DMA_COUNT         0x0022
#define IJKL_VM_IO_PORT_DMA_CONTROL       0x0023

#define IJKL_VM_DMA_CMD_COPY              1
#define IJKL_VM_DMA_CMD_FILL              2
#define IJKL_VM_DMA_CMD_COMPARE           3

/* Portas de operações em arquivos e sistema de diretórios */
#define IJKL_VM_IO_PORT_FILE_EXISTS       0x0100
#define IJKL_VM_IO_PORT_FILE_READ_ALL     0x0101
#define IJKL_VM_IO_PORT_FILE_WRITE_ALL    0x0102
#define IJKL_VM_IO_PORT_FILE_APPEND       0x0103
#define IJKL_VM_IO_PORT_FILE_DELETE       0x0104
#define IJKL_VM_IO_PORT_FILE_RENAME       0x0105
#define IJKL_VM_IO_PORT_FILE_COPY         0x0106

#define IJKL_VM_IO_PORT_FOLDER_EXISTS     0x0110
#define IJKL_VM_IO_PORT_FOLDER_CREATE     0x0111
#define IJKL_VM_IO_PORT_FOLDER_DELETE     0x0112
#define IJKL_VM_IO_PORT_FOLDER_GET_CURR   0x0113
#define IJKL_VM_IO_PORT_FOLDER_SET_CURR   0x0114

#define IJKL_VM_MAX_REGISTERED_IO_DEVICES 64

struct IjklVirtualMemory;
struct IjklVirtualMachine;

/* Estrutura representativa do controlador de DMA periférico */
typedef struct IjklDmaController {
    uint64_t source_address;
    uint64_t destination_address;
    uint64_t byte_count;
    int64_t status_or_result;
    struct IjklVirtualMemory* target_memory;
} IjklDmaController;

/* Estrutura representativa do controlador de sistema de arquivos periférico */
typedef struct IjklFileIoController {
    struct IjklVirtualMachine* machine;
    int64_t last_operation_status;
} IjklFileIoController;

/* Assinaturas dos métodos de leitura e escrita em portas do barramento */
typedef bool (*IjklIoPortReadFunction)(void* device_context,
                                       uint16_t port_address,
                                       int64_t* output_value);

typedef bool (*IjklIoPortWriteFunction)(void* device_context,
                                        uint16_t port_address,
                                        int64_t input_value,
                                        const char* optional_string_literal);

/* Mapeamento de uma faixa contígua de portas atribuída a um dispositivo */
typedef struct IjklIoDeviceBinding {
    uint16_t starting_port_address;
    uint16_t total_ports_count;
    const char* device_display_name;

    IjklIoPortReadFunction read_handler;
    IjklIoPortWriteFunction write_handler;
    void* device_context;
    bool is_active;
} IjklIoDeviceBinding;

/* Estrutura central do barramento de I/O da Máquina Virtual */
typedef struct IjklIoBus {
    IjklIoDeviceBinding registered_devices[IJKL_VM_MAX_REGISTERED_IO_DEVICES];
    size_t registered_devices_count;

    /* Estatísticas diagnósticas */
    uint64_t total_in_operations_count;
    uint64_t total_out_operations_count;
} IjklIoBus;

/* Inicialização e destruição do barramento de E/S */
bool ijkl_vm_io_bus_initialize(IjklIoBus* io_bus);
void ijkl_vm_io_bus_destroy(IjklIoBus* io_bus);
void ijkl_vm_io_bus_reset(IjklIoBus* io_bus);

/*
 * EFEITO COLATERAL:
 * Registra um manipulador de porta ou periférico para responder a uma faixa de portas.
 */
bool ijkl_vm_io_bus_register_device(IjklIoBus* io_bus,
                                    uint16_t starting_port_address,
                                    uint16_t total_ports_count,
                                    const char* device_display_name,
                                    IjklIoPortReadFunction read_handler,
                                    IjklIoPortWriteFunction write_handler,
                                    void* device_context);

/*
 * EFEITOS COLATERAIS:
 * Executa as instruções elementares 'in' e 'out' despachando para os manipuladores registrados.
 */
bool ijkl_vm_io_bus_read_port(IjklIoBus* io_bus,
                              uint16_t port_address,
                              int64_t* output_value);

bool ijkl_vm_io_bus_write_port(IjklIoBus* io_bus,
                               uint16_t port_address,
                               int64_t input_value,
                               const char* optional_string_literal);

/* Registra os dispositivos embutidos padrão do console terminal (0x0000 a 0x0004) */
bool ijkl_vm_io_bus_register_standard_console(IjklIoBus* io_bus, struct IjklVirtualMachine* machine);

/* Registra o controlador de transferência de bloco DMA (0x0020 a 0x0023) */
bool ijkl_vm_io_bus_register_dma_controller(IjklIoBus* io_bus,
                                            IjklDmaController* dma_controller,
                                            struct IjklVirtualMemory* target_memory);

/* Registra o dispositivo do relógio de tempo real do sistema (0x0008 a 0x000A) */
bool ijkl_vm_io_bus_register_system_clock(IjklIoBus* io_bus);

/* Registra o controlador de sistema de arquivos e pastas (0x0100 a 0x0114) */
bool ijkl_vm_io_bus_register_file_system_controller(IjklIoBus* io_bus,
                                                    IjklFileIoController* controller,
                                                    struct IjklVirtualMachine* machine);

#endif /* IJKL_VM_IO_H */
