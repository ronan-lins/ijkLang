# ==============================================================================
# ijkLang (ijkL) - Makefile para GCC / Clang / MinGW
# ==============================================================================

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude -D_POSIX_C_SOURCE=200809L -O2
LDFLAGS ?=
BIN_DIR = bin
BUILD_DIR = build

TARGET_COMPILER = $(BIN_DIR)/ijkl
TARGET_VM = $(BIN_DIR)/ijkl-m

# Bibliotecas de linkagem para a Máquina Virtual (math e dynamic loading)
LDLIBS_VM ?= -lm
ifeq ($(OS),Windows_NT)
    # No Windows (MinGW), funcoes de carregamento dinamico estao na kernel32
else
    UNAME_S := $(shell uname -s 2>/dev/null || echo "Unknown")
    ifeq ($(UNAME_S),Linux)
        LDLIBS_VM += -ldl
    endif
endif

SRCS_COMPILER = src/compiler/main.c \
               src/compiler/config.c \
               src/compiler/lexer.c \
               src/compiler/token.c \
               src/compiler/vocabulary.c \
               src/compiler/ast.c \
               src/compiler/parser.c \
               src/compiler/symtab.c \
               src/compiler/semantic.c \
               src/compiler/codegen.c \
               src/compiler/module_loader.c \
               src/common/memory.c \
               src/common/utf8.c

SRCS_VM = src/vm/main.c \
          src/vm/ci_instruction.c \
          src/vm/ci_loader.c \
          src/vm/vm_core.c \
          src/vm/vm_debugger.c \
          src/vm/vm_io.c \
          src/vm/vm_memory.c \
          src/vm/vm_plugins.c \
          src/common/memory.c \
          src/common/utf8.c

OBJS_COMPILER = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS_COMPILER))
OBJS_VM = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS_VM))

.PHONY: all clean test test-integration test-console publish copy_assets

all: $(TARGET_COMPILER) $(TARGET_VM) copy_assets

$(TARGET_COMPILER): $(OBJS_COMPILER) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJS_COMPILER) -o $@

$(TARGET_VM): $(OBJS_VM) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJS_VM) -o $@ $(LDLIBS_VM)

$(BUILD_DIR)/compiler/%.o: src/compiler/%.c | $(BUILD_DIR)/compiler
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vm/%.o: src/vm/%.c | $(BUILD_DIR)/vm
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common/%.o: src/common/%.c | $(BUILD_DIR)/common
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/compiler:
	mkdir -p $(BUILD_DIR)/compiler

$(BUILD_DIR)/vm:
	mkdir -p $(BUILD_DIR)/vm

$(BUILD_DIR)/common:
	mkdir -p $(BUILD_DIR)/common

copy_assets: | $(BIN_DIR)
	@cp -f ijkl.ini $(BIN_DIR)/ 2>/dev/null || copy /Y ijkl.ini $(BIN_DIR)\ >nul 2>&1 || true
	@mkdir -p $(BIN_DIR)/keywords 2>/dev/null || mkdir $(BIN_DIR)\keywords >nul 2>&1 || true
	@cp -rf keywords/* $(BIN_DIR)/keywords/ 2>/dev/null || xcopy /E /Y /I keywords $(BIN_DIR)\keywords >nul 2>&1 || true
	@mkdir -p $(BIN_DIR)/lib 2>/dev/null || mkdir $(BIN_DIR)\lib >nul 2>&1 || true
	@cp -rf lib/* $(BIN_DIR)/lib/ 2>/dev/null || xcopy /E /Y /I lib $(BIN_DIR)\lib >nul 2>&1 || true

test: all
	@echo "Executando testes da suite ijkLang..."
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File tests/run_tests.ps1
else
	@bash tests/run_tests.sh
endif

test-console: all
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File tests/run_console_tests.ps1
else
	@python3 tests/run_console_tests.py
endif

test-integration: all
	@echo "Executando testes de integracao modular do ijkLang..."
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File tests/run_integration_tests.ps1
else
	@bash tests/run_integration_tests.sh
endif

# Suporte a argumento posicional direto: make publish <destino> ou make publish DEST=<destino>
ifeq (publish,$(firstword $(MAKECMDGOALS)))
  PUBLISH_TARGET_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(PUBLISH_TARGET_ARGS):;@:)
  DEST ?= $(firstword $(PUBLISH_TARGET_ARGS))
endif

ifeq ($(filter %bin %bin/,$(DEST)),)
  PUBLISH_BIN_DIR = $(DEST)/bin
else
  PUBLISH_BIN_DIR = $(DEST)
endif

publish: clean all
ifeq ($(strip $(DEST)),)
	$(error Erro: Destino nao informado. Uso: make publish <diretorio_destino> (ex: make publish ../ijkDemo) ou make publish DEST=<diretorio_destino>)
endif
	@echo "======================================================================"
	@echo "Publicando ijkLang em: $(DEST)"
	@echo "======================================================================"
ifeq ($(OS),Windows_NT)
	@if not exist "$(PUBLISH_BIN_DIR)" mkdir "$(PUBLISH_BIN_DIR)"
	@if not exist "$(PUBLISH_BIN_DIR)\keywords" mkdir "$(PUBLISH_BIN_DIR)\keywords"
	@if not exist "$(PUBLISH_BIN_DIR)\lib" mkdir "$(PUBLISH_BIN_DIR)\lib"
	@copy /Y "$(BIN_DIR)\ijkl.exe" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || copy /Y "$(BIN_DIR)\ijkl" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || true
	@copy /Y "$(BIN_DIR)\ijkl-m.exe" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || copy /Y "$(BIN_DIR)\ijkl-m" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || true
	@copy /Y "$(BIN_DIR)\ijkl.ini" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || copy /Y "ijkl.ini" "$(PUBLISH_BIN_DIR)\" >nul 2>&1 || true
	@xcopy /E /Y /I "keywords" "$(PUBLISH_BIN_DIR)\keywords" >nul 2>&1 || true
	@xcopy /E /Y /I "lib" "$(PUBLISH_BIN_DIR)\lib" >nul 2>&1 || true
	@if not exist "$(DEST)\lib" mkdir "$(DEST)\lib"
	@xcopy /E /Y /I "lib" "$(DEST)\lib" >nul 2>&1 || true
	@if not exist "$(DEST)\examples" mkdir "$(DEST)\examples"
	@xcopy /E /Y /I "examples" "$(DEST)\examples" >nul 2>&1 || true
	@echo Publicacao concluida com sucesso em: $(DEST)
else
	@mkdir -p "$(PUBLISH_BIN_DIR)/keywords" "$(PUBLISH_BIN_DIR)/lib" "$(DEST)/lib"
	@cp -f $(BIN_DIR)/ijkl $(BIN_DIR)/ijkl-m "$(PUBLISH_BIN_DIR)/"
	@cp -f ijkl.ini "$(PUBLISH_BIN_DIR)/"
	@cp -rf keywords/* "$(PUBLISH_BIN_DIR)/keywords/"
	@cp -rf lib/* "$(PUBLISH_BIN_DIR)/lib/"
	@cp -rf lib/* "$(DEST)/lib/"
	@chmod +x "$(PUBLISH_BIN_DIR)/ijkl" "$(PUBLISH_BIN_DIR)/ijkl-m" 2>/dev/null || true
	@mkdir -p "$(DEST)/examples"
	@cp -rf examples/* "$(DEST)/examples/"
	@echo "Publicacao concluida com sucesso em: $(DEST)"
endif

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

