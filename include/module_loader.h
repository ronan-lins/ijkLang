#ifndef IJKL_MODULE_LOADER_H
#define IJKL_MODULE_LOADER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "vocabulary.h"
#include "ast.h"

/*
 * Estrutura que representa uma unidade compilada carregada em memória.
 */
typedef struct IjklLoadedModule {
    const char* logical_name;              /* Nome qualificado (ex.: "GeometriaApp.Calculos") */
    const char* source_file_path;          /* Caminho absoluto ou relativo resolvido no disco */
    char* source_code_buffer;              /* Conteúdo bruto lido do arquivo fonte */
    size_t source_length_in_bytes;
    IjklAstNode* compilation_unit_ast;     /* Nó raiz AST (AST_NODE_MODULE ou AST_NODE_PROGRAM) */
    bool is_currently_loading;             /* Flag para detecção de ciclo / recursão mútua */
    bool is_loading_completed;             /* Flag indicando conclusão do carregamento */
    struct IjklLoadedModule* next_module;
} IjklLoadedModule;

/*
 * Estrutura principal do Carregador de Módulos e Gerenciador de Dependências.
 */
typedef struct IjklModuleLoader {
    IjklMemoryArena* memory_arena;
    IjklVocabulary* active_vocabulary;
    const char* base_search_directory;
    const char* framework_search_directory;
    IjklVocabulary* framework_vocabulary;
    IjklLoadedModule* first_loaded_module;
    IjklLoadedModule* last_loaded_module;
    size_t total_loaded_modules_count;
    size_t total_errors_count;
} IjklModuleLoader;

/* --- Criação e Destruição --- */

IjklModuleLoader* ijkl_module_loader_create(IjklMemoryArena* memory_arena,
                                           IjklVocabulary* active_vocabulary,
                                           const char* base_search_directory,
                                           const char* framework_search_directory);

void ijkl_module_loader_set_framework_vocabulary(IjklModuleLoader* module_loader,
                                                IjklVocabulary* framework_vocabulary);

void ijkl_module_loader_destroy(IjklModuleLoader* module_loader);

/* --- Resolução de Caminhos no Sistema de Arquivos --- */

/*
 * EFEITO COLATERAL:
 * Localiza um arquivo de módulo no sistema de arquivos a partir de um nome qualificado
 * (ex.: 'GeometriaApp.Calculos') testando as três variantes obrigatórias:
 * 1. Nome.Qualificado.ijk
 * 2. Nome/Qualificado.ijk (apenas último ponto como separador)
 * 3. Nome/Do/Modulo/Qualificado.ijk (todos os pontos como separadores)
 *
 * Realiza busca insensível a maiúsculas e minúsculas em Windows, Linux e macOS.
 * Emite erro se mais de uma variante coincidir (ambiguidade) ou se nenhuma for localizada.
 */
bool ijkl_module_loader_resolve_module_path(const IjklModuleLoader* module_loader,
                                            const char* qualified_module_name,
                                            char* output_resolved_path,
                                            size_t output_buffer_capacity);

/* --- Carregamento do Projeto e Grafo de Dependências --- */

/*
 * EFEITO COLATERAL:
 * Carrega a unidade raiz indicada e resolve recursivamente todas as suas dependências
 * declaradas através de 'import' / 'importe', construindo o grafo de dependências consolidado.
 * Suporta referências circulares e recursão mútua sem loop infinito.
 * Retorna a lista encadeada de raízes AST de todas as unidades do projeto.
 */
IjklAstNode* ijkl_module_loader_load_project(IjklModuleLoader* module_loader,
                                             const char* root_file_path);

/* Retorna a contagem total de erros acumulados durante o carregamento de módulos */
size_t ijkl_module_loader_get_error_count(const IjklModuleLoader* module_loader);

#endif /* IJKL_MODULE_LOADER_H */
