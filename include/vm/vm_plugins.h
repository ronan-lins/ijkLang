#ifndef IJKL_VM_PLUGINS_H
#define IJKL_VM_PLUGINS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ijkl_device_api.h"
#include "vm/vm_io.h"

/*
 * ==============================================================================
 * MÓDULO: GERENCIADOR DE PLUGINS DINÂMICOS DE DISPOSITIVOS (IJKLANG VM)
 * ==============================================================================
 * Gerencia a carga em tempo de execução de bibliotecas compartilhadas (.dll / .so)
 * que implementam a API normativa de dispositivos periféricos (ijkl_device_api.h)
 * e conecta-as ao barramento de portas de 16 bits da Máquina Virtual.
 *
 * Siglas técnicas empregadas:
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * - DLL: Dynamic-Link Library (Windows).
 * - SO: Shared Object (Linux/POSIX).
 * - API: Application Programming Interface.
 * ==============================================================================
 */

#define IJKL_VM_MAX_LOADED_PLUGINS 32

/* Registro de um plugin de periférico dinamicamente carregado */
typedef struct IjklLoadedPluginRecord {
    void* shared_library_handle;
    const IjklDeviceInterface* device_interface;
    IjklDeviceInstance* device_instance;
    bool is_initialized;
    char library_file_path[260];
} IjklLoadedPluginRecord;

/* Gerenciador central de plugins da Máquina Virtual */
typedef struct IjklVmPluginsManager {
    IjklIoBus* target_io_bus;
    IjklLoadedPluginRecord loaded_plugins[IJKL_VM_MAX_LOADED_PLUGINS];
    size_t loaded_plugins_count;
} IjklVmPluginsManager;

/* Inicialização e finalização do gerenciador de plugins */
bool ijkl_vm_plugins_manager_initialize(IjklVmPluginsManager* plugins_manager, IjklIoBus* target_io_bus);
void ijkl_vm_plugins_manager_destroy(IjklVmPluginsManager* plugins_manager);

/*
 * EFEITO COLATERAL:
 * Carrega a biblioteca dinâmica indicada em disco, consulta seu ponto de entrada
 * canônico 'ijkl_device_get_interface', inicializa o periférico e conecta-o
 * automaticamente à sua faixa de portas no barramento I/O.
 */
bool ijkl_vm_plugins_manager_load_plugin(IjklVmPluginsManager* plugins_manager,
                                         const char* library_path,
                                         const char* configuration_parameters);

/*
 * EFEITO COLATERAL:
 * Propaga a passagem de tempo do relógio da CPU virtual para todos os periféricos ativos.
 */
void ijkl_vm_plugins_manager_update(IjklVmPluginsManager* plugins_manager,
                                    uint64_t elapsed_time_microseconds);

#endif /* IJKL_VM_PLUGINS_H */
