#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include "vm/vm_plugins.h"

/*
 * ==============================================================================
 * MÓDULO: IMPLEMENTAÇÃO DO GERENCIADOR DE PLUGINS DINÂMICOS (IJKLANG VM)
 * ==============================================================================
 * Carrega bibliotecas externas (.dll no Windows / .so no Linux) que implementam
 * o contrato normativo 'ijkl_device_api.h' e integra-as ao barramento de portas.
 * ==============================================================================
 */

/* Adaptadores de leitura e escrita do barramento I/O para a API de dispositivos */
static bool device_bus_read_adapter(void* device_context,
                                    uint16_t port_address,
                                    int64_t* output_value) {
    IjklLoadedPluginRecord* plugin_record = (IjklLoadedPluginRecord*)device_context;
    if (!plugin_record || !plugin_record->device_interface || !plugin_record->device_interface->read_device_port) {
        return false;
    }

    uint32_t raw_device_value = 0;
    IjklDeviceStatus status = plugin_record->device_interface->read_device_port(
        plugin_record->device_instance, port_address, &raw_device_value);

    if (status == DEVICE_STATUS_SUCCESS) {
        *output_value = (int64_t)raw_device_value;
        return true;
    }
    return false;
}

static bool device_bus_write_adapter(void* device_context,
                                     uint16_t port_address,
                                     int64_t input_value,
                                     const char* optional_string_literal) {
    (void)optional_string_literal;
    IjklLoadedPluginRecord* plugin_record = (IjklLoadedPluginRecord*)device_context;
    if (!plugin_record || !plugin_record->device_interface || !plugin_record->device_interface->write_device_port) {
        return false;
    }

    IjklDeviceStatus status = plugin_record->device_interface->write_device_port(
        plugin_record->device_instance, port_address, (uint32_t)(input_value & 0xFFFFFFFF));

    return (status == DEVICE_STATUS_SUCCESS);
}

bool ijkl_vm_plugins_manager_initialize(IjklVmPluginsManager* plugins_manager, IjklIoBus* target_io_bus) {
    if (!plugins_manager || !target_io_bus) return false;

    memset(plugins_manager, 0, sizeof(IjklVmPluginsManager));
    plugins_manager->target_io_bus = target_io_bus;
    return true;
}

void ijkl_vm_plugins_manager_destroy(IjklVmPluginsManager* plugins_manager) {
    if (!plugins_manager) return;

    for (size_t i = 0; i < plugins_manager->loaded_plugins_count; ++i) {
        IjklLoadedPluginRecord* plugin = &plugins_manager->loaded_plugins[i];
        if (plugin->is_initialized && plugin->device_interface && plugin->device_interface->shutdown_device) {
            plugin->device_interface->shutdown_device(plugin->device_instance);
            plugin->is_initialized = false;
        }

        if (plugin->shared_library_handle) {
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
            FreeLibrary((HMODULE)plugin->shared_library_handle);
#else
            dlclose(plugin->shared_library_handle);
#endif
            plugin->shared_library_handle = NULL;
        }
    }

    plugins_manager->loaded_plugins_count = 0;
}

bool ijkl_vm_plugins_manager_load_plugin(IjklVmPluginsManager* plugins_manager,
                                         const char* library_path,
                                         const char* configuration_parameters) {
    if (!plugins_manager || !library_path) return false;

    if (plugins_manager->loaded_plugins_count >= IJKL_VM_MAX_LOADED_PLUGINS) {
        fprintf(stderr, "[Error] Plugin manager reached maximum capacity of %d plugins.\n", IJKL_VM_MAX_LOADED_PLUGINS);
        return false;
    }

    void* library_handle = NULL;
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    library_handle = (void*)LoadLibraryA(library_path);
#else
    library_handle = dlopen(library_path, RTLD_NOW | RTLD_LOCAL);
#endif

    if (!library_handle) {
        fprintf(stderr, "[Error] Could not load dynamic library '%s'.\n", library_path);
        return false;
    }

    /* Consulta do ponto de entrada normativo: ijkl_device_get_interface */
    IjklDeviceGetInterfaceFunction get_interface_function = NULL;
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    union {
        FARPROC win_proc;
        IjklDeviceGetInterfaceFunction func_ptr;
    } sym_converter;
    sym_converter.win_proc = GetProcAddress((HMODULE)library_handle, "ijkl_device_get_interface");
    get_interface_function = sym_converter.func_ptr;
#else
    union {
        void* obj_ptr;
        IjklDeviceGetInterfaceFunction func_ptr;
    } sym_converter;
    sym_converter.obj_ptr = dlsym(library_handle, "ijkl_device_get_interface");
    get_interface_function = sym_converter.func_ptr;
#endif

    if (!get_interface_function) {
        fprintf(stderr, "[Error] Dynamic library '%s' does not export entry point 'ijkl_device_get_interface'.\n", library_path);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
        FreeLibrary((HMODULE)library_handle);
#else
        dlclose(library_handle);
#endif
        return false;
    }

    const IjklDeviceInterface* device_interface = get_interface_function();
    if (!device_interface || device_interface->api_version_major != IJKL_DEVICE_API_VERSION_MAJOR) {
        fprintf(stderr, "[Error] Plugin '%s' has incompatible API version.\n", library_path);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
        FreeLibrary((HMODULE)library_handle);
#else
        dlclose(library_handle);
#endif
        return false;
    }

    IjklDeviceInstance* device_instance = NULL;
    if (device_interface->initialize_device) {
        IjklDeviceStatus status = device_interface->initialize_device(&device_instance, configuration_parameters);
        if (status != DEVICE_STATUS_SUCCESS) {
            fprintf(stderr, "[Error] Failed to initialize device plugin '%s' (Status code: %d).\n", library_path, status);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
            FreeLibrary((HMODULE)library_handle);
#else
            dlclose(library_handle);
#endif
            return false;
        }
    }

    /* Registra o registro do plugin carregado */
    IjklLoadedPluginRecord* record = &plugins_manager->loaded_plugins[plugins_manager->loaded_plugins_count++];
    record->shared_library_handle = library_handle;
    record->device_interface = device_interface;
    record->device_instance = device_instance;
    record->is_initialized = true;
    strncpy(record->library_file_path, library_path, sizeof(record->library_file_path) - 1);

    /* Registra as portas do dispositivo no barramento I/O da VM */
    if (plugins_manager->target_io_bus) {
        ijkl_vm_io_bus_register_device(plugins_manager->target_io_bus,
                                       device_interface->information.starting_port_address,
                                       device_interface->information.total_ports_count,
                                       device_interface->information.display_name,
                                       device_bus_read_adapter,
                                       device_bus_write_adapter,
                                       record);
    }

    return true;
}

void ijkl_vm_plugins_manager_update(IjklVmPluginsManager* plugins_manager,
                                    uint64_t elapsed_time_microseconds) {
    if (!plugins_manager) return;

    for (size_t i = 0; i < plugins_manager->loaded_plugins_count; ++i) {
        IjklLoadedPluginRecord* record = &plugins_manager->loaded_plugins[i];
        if (record->is_initialized && record->device_interface && record->device_interface->update_device) {
            record->device_interface->update_device(record->device_instance, elapsed_time_microseconds);
        }
    }
}
