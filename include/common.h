#ifndef IJKL_COMMON_H
#define IJKL_COMMON_H

#if defined(_MSC_VER)
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "version.h"

/* Portability helpers */
#if defined(_WIN32) || defined(_WIN64)
    #define IJKL_PATH_SEPARATOR '\\'
    #define IJKL_PATH_SEPARATOR_STR "\\"
#else
    #define IJKL_PATH_SEPARATOR '/'
    #define IJKL_PATH_SEPARATOR_STR "/"
#endif

/* Memory allocation wrappers with null-check */
static inline void* ijkl_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "Erro fatal: falha de alocacao de memoria (%zu bytes).\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline void* ijkl_calloc(size_t num, size_t size) {
    void* ptr = calloc(num, size);
    if (!ptr && num > 0 && size > 0) {
        fprintf(stderr, "Erro fatal: falha de alocacao zerada de memoria.\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline void* ijkl_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "Erro fatal: falha de realocacao de memoria (%zu bytes).\n", size);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

static inline void ijkl_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* String duplicate portable helper */
static inline char* ijkl_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)ijkl_malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

/* Comparação case-insensitive portátil em conformidade estrita com ISO C11 */
static inline int ijkl_string_case_insensitive_compare(const char* first_string, const char* second_string) {
    if (first_string == second_string) return 0;
    if (!first_string) return -1;
    if (!second_string) return 1;

    while (*first_string && *second_string) {
        int char_difference = tolower((unsigned char)*first_string) - tolower((unsigned char)*second_string);
        if (char_difference != 0) {
            return char_difference;
        }
        first_string++;
        second_string++;
    }
    return tolower((unsigned char)*first_string) - tolower((unsigned char)*second_string);
}

static inline bool ijkl_string_case_insensitive_equals(const char* first_string, const char* second_string) {
    return ijkl_string_case_insensitive_compare(first_string, second_string) == 0;
}

#endif /* IJKL_COMMON_H */
