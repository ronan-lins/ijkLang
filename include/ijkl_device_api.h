#ifndef IJKL_DEVICE_API_H
#define IJKL_DEVICE_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * ==============================================================================
 * MÓDULO: API NORMATIVA DE PLUGINS DE DISPOSITIVOS PERIFÉRICOS (IJKLANG VM)
 * ==============================================================================
 * Define o contrato binário e a interface em C11 para desenvolvimento de
 * periféricos didáticos externos (módulos compartilhados .dll / .so).
 *
 * A Máquina Virtual (ijkl-m) conecta-se a estes dispositivos por intermédio
 * de um barramento simulado de portas de entrada e saída (I/O ports), mapeadas
 * no espaço de endereçamento de 16 bits (portas de 0x0000 a 0xFFFF).
 *
 * Siglas técnicas empregadas:
 * - API: Application Programming Interface (Interface de Programação de Aplicação).
 * - VM: Virtual Machine (Máquina Virtual de Execução do Código Intermediário).
 * - ABI: Application Binary Interface (Interface Binária de Aplicação).
 * ==============================================================================
 */

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #if defined(IJKL_DEVICE_BUILD_LIBRARY)
        #define IJKL_DEVICE_EXPORT __declspec(dllexport)
    #else
        #define IJKL_DEVICE_EXPORT __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define IJKL_DEVICE_EXPORT __attribute__((visibility("default")))
#else
    #define IJKL_DEVICE_EXPORT
#endif

/* Versão da especificação da API de Dispositivos (SemVer 1.0) */
#define IJKL_DEVICE_API_VERSION_MAJOR 1
#define IJKL_DEVICE_API_VERSION_MINOR 0

/* Códigos de status retornados pelas operações do dispositivo */
typedef enum IjklDeviceStatus {
    DEVICE_STATUS_SUCCESS = 0,
    DEVICE_STATUS_ERROR_INVALID_PORT = 1,
    DEVICE_STATUS_ERROR_READ_ONLY = 2,
    DEVICE_STATUS_ERROR_WRITE_ONLY = 3,
    DEVICE_STATUS_ERROR_NOT_INITIALIZED = 4,
    DEVICE_STATUS_ERROR_INTERNAL_FAILURE = 5
} IjklDeviceStatus;

/* Tipos de capacidades e modos operacionais suportados pelo periférico */
typedef enum IjklDeviceCapabilityFlags {
    DEVICE_CAPABILITY_READABLE   = (1 << 0),
    DEVICE_CAPABILITY_WRITABLE   = (1 << 1),
    DEVICE_CAPABILITY_INTERRUPT  = (1 << 2),
    DEVICE_CAPABILITY_GUI_WINDOW = (1 << 3)
} IjklDeviceCapabilityFlags;

/* Metadados descritivos sobre o periférico */
typedef struct IjklDeviceInformation {
    const char* device_identifier_name;
    const char* display_name;
    const char* author_name;
    const char* description_text;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t capability_flags;

    uint16_t starting_port_address;
    uint16_t total_ports_count;
} IjklDeviceInformation;

/* Contexto opaco manipulado internamente pelo plugin do dispositivo */
typedef void IjklDeviceInstance;

/* Assinaturas dos métodos fundamentais do ciclo de vida do dispositivo */
typedef IjklDeviceStatus (*IjklDeviceInitializeFunction)(IjklDeviceInstance** output_instance,
                                                         const char* configuration_string);

typedef IjklDeviceStatus (*IjklDeviceShutdownFunction)(IjklDeviceInstance* device_instance);

typedef IjklDeviceStatus (*IjklDeviceResetFunction)(IjklDeviceInstance* device_instance);

/*
 * EFEITO COLATERAL:
 * Atualiza o estado temporal e a lógica interna do periférico em função da passagem
 * de tempo no relógio do processador (microssegundos decorridos).
 */
typedef IjklDeviceStatus (*IjklDeviceUpdateFunction)(IjklDeviceInstance* device_instance,
                                                     uint64_t elapsed_time_microseconds);

/*
 * EFEITO COLATERAL:
 * Realiza a leitura de um valor de 32 bits a partir de uma porta de entrada do barramento.
 */
typedef IjklDeviceStatus (*IjklDeviceReadPortFunction)(IjklDeviceInstance* device_instance,
                                                       uint16_t port_address,
                                                       uint32_t* output_value);

/*
 * EFEITO COLATERAL:
 * Realiza a escrita de um valor de 32 bits em uma porta de saída do barramento.
 */
typedef IjklDeviceStatus (*IjklDeviceWritePortFunction)(IjklDeviceInstance* device_instance,
                                                        uint16_t port_address,
                                                        uint32_t input_value);

/*
 * Retorna uma representação textual formatada do estado interno do dispositivo
 * para exibição interativa na IDE, TUI ou logs do depurador.
 */
typedef const char* (*IjklDeviceInspectStateFunction)(IjklDeviceInstance* device_instance);

/* Tabela de funções que materializa a interface de um periférico */
typedef struct IjklDeviceInterface {
    uint32_t api_version_major;
    uint32_t api_version_minor;

    IjklDeviceInformation information;

    IjklDeviceInitializeFunction initialize_device;
    IjklDeviceShutdownFunction shutdown_device;
    IjklDeviceResetFunction reset_device;
    IjklDeviceUpdateFunction update_device;
    IjklDeviceReadPortFunction read_device_port;
    IjklDeviceWritePortFunction write_device_port;
    IjklDeviceInspectStateFunction inspect_device_state;
} IjklDeviceInterface;

/*
 * PONTO DE ENTRADA EXPORTADO DO PLUGIN:
 * Todo plugin dinâmico (.dll ou .so) de periférico da ijkLang deve obrigatoriamente
 * exportar a função canônica denominada 'ijkl_device_get_interface'.
 */
typedef const IjklDeviceInterface* (*IjklDeviceGetInterfaceFunction)(void);

#ifdef __cplusplus
extern "C" {
#endif

IJKL_DEVICE_EXPORT const IjklDeviceInterface* ijkl_device_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* IJKL_DEVICE_API_H */
