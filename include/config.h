#ifndef IJKL_CONFIG_H
#define IJKL_CONFIG_H

#include "common.h"

/*
 * ==============================================================================
 * MÓDULO: CONFIGURAÇÃO DO COMPILADOR IJKLANG
 * ==============================================================================
 * Responsável pelo carregamento e validação das opções de inicialização do
 * compilador presentes no arquivo ijkl.ini.
 * ==============================================================================
 */

typedef struct IjklCompilerConfig {
    char active_language[32];        /* Código do idioma ativo (ex.: "en-US", "pt-BR") */
    char keywords_directory[256];    /* Diretório relativo ou absoluto contendo os arquivos .words */
    char framework_directory[256];   /* Diretório relativo ou absoluto contendo os módulos do framework padrão */
    bool is_strict_mode;             /* Modo estrito: true para exigir conformidade rígida */
} IjklCompilerConfig;

/* Inicializa as opções de configuração com seus valores padrão recomendados */
void ijkl_config_init_defaults(IjklCompilerConfig* compiler_config);

/*
 * Carrega e processa as opções presentes no arquivo ijkl.ini.
 *
 * Busca primeiro no caminho indicado por 'ini_file_path'. Caso não encontre,
 * busca no diretório onde reside o executável do compilador ('executable_directory').
 * Retorna true se o arquivo for localizado e lido com sucesso, ou false caso contrário.
 */
bool ijkl_config_load_from_file(IjklCompilerConfig* compiler_config,
                               const char* ini_file_path,
                               const char* executable_directory);

#endif /* IJKL_CONFIG_H */
