#ifndef IJKL_VM_CI_LOADER_H
#define IJKL_VM_CI_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "memory.h"
#include "vm/ci_instruction.h"

/*
 * ==============================================================================
 * MÓDULO: CARREGADOR DE CÓDIGO INTERMEDIÁRIO (IJKLANG VM)
 * ==============================================================================
 * Analisa arquivos textuais de Código Intermediário (*.ci), constrói o vetor
 * de instruções decodificadas, mapeia rótulos simbólicos (#rotulo:) e resolve
 * os endereços de saltos e chamadas para execução na Máquina Virtual.
 *
 * Siglas técnicas empregadas:
 * - VM: Virtual Machine (Máquina Virtual educacional).
 * - CI: Código Intermediário Linear textual (*.ci).
 * ==============================================================================
 */

/* Programa completo compilado em memória pronto para execução */
typedef struct IjklCiProgram {
    IjklVmInstruction* instructions;
    size_t instructions_count;
    size_t entry_point_instruction_index;
    const char* program_name;
} IjklCiProgram;

/*
 * EFEITO COLATERAL:
 * Lê o arquivo *.ci em disco, decodifica suas instruções e resolve os rótulos de salto.
 * Todas as estruturas e strings alocadas utilizam o IjklMemoryArena fornecido.
 */
bool ijkl_ci_loader_load_from_file(IjklMemoryArena* memory_arena,
                                   const char* file_path,
                                   IjklCiProgram* output_program);

/*
 * EFEITO COLATERAL:
 * Decodifica o código intermediário linear a partir de um buffer de texto em memória.
 */
bool ijkl_ci_loader_load_from_string(IjklMemoryArena* memory_arena,
                                     const char* source_code_text,
                                     const char* source_name_for_diagnostics,
                                     IjklCiProgram* output_program);

#endif /* IJKL_VM_CI_LOADER_H */
