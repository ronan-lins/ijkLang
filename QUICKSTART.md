# Guia de Início Rápido (Quickstart) — ijkLang (`ijkL`)

Este guia prático orienta a configuração do ambiente de desenvolvimento, a compilação das ferramentas do ecossistema e a execução de um programa completo, demonstrando as etapas de análise, inspeção sintática, geração de código intermediário e execução na máquina virtual.

---

## 1. Obtenção do Código-Fonte

Clone o repositório oficial e acesse o diretório principal:

```bash
git clone https://github.com/ronan-lins/ijkLang
cd ijkLang
```

---

## 2. Preparação do Ambiente e Compilação

O projeto é escrito em **C (ISO C11)** e não requer dependências de bibliotecas externas complexas.

### 2.1 Ambiente Windows

#### Pré-requisitos:

- **Visual Studio 2019 ou 2022** (com a carga de trabalho _"Desenvolvimento para Desktop com C++"_) ou **Build Tools for Visual Studio**.
- **PowerShell 5.1** ou superior.

#### Compilação via MSBuild:

Abra o _Developer Command Prompt for VS_ (ou um terminal PowerShell configurado com as ferramentas de compilação) e execute:

```powershell
MSBuild.exe ijkL.sln -p:Configuration=Debug -p:Platform=x64
```

Os binários compilados serão gerados em `bin\Debug\`:

- `bin\Debug\ijkl.exe` (Compilador)
- `bin\Debug\ijkl-m.exe` (Máquina Virtual e Depurador)
- `bin\Debug\ijkl.ini` (Configuração padrão de idioma e palavras-chave)

---

### 2.2 Ambiente Linux ou macOS

#### Pré-requisitos:

- Compilador **GCC** (versão 9.0 ou superior) ou **Clang** (versão 10.0 ou superior).
- Utilitário **GNU Make**.
- No Ubuntu/Debian: `sudo apt update && sudo apt install build-essential git`
- No Fedora/RHEL: `sudo dnf groupinstall "Development Tools"`
- No Arch Linux: `sudo pacman -S base-devel git`

#### Compilação via Make:

No diretório raiz do projeto, execute:

```bash
make all
```

Os binários compilados serão gerados no diretório `bin/`:

- `bin/ijkl` (Compilador)
- `bin/ijkl-m` (Máquina Virtual e Depurador)
- `bin/ijkl.ini` e `bin/keywords/` (Ativos copiados automaticamente)

---

## 3. Código de Demonstração: `experimento.ijk`

Para experimentar os recursos de linguagem e arquitetura, crie um arquivo chamado `experimento.ijk` na raiz do projeto (ou utilize o arquivo de exemplo disponibilizado em `examples/pt-BR/quickstart_demo.ijk`).

Este programa demonstra:

1. Declaração de funções com parâmetros e retorno tipado (`real`).
2. Declaração de procedimentos com mutação de dados em memória por meio de ponteiros (`ponteiro inteiro`, `&` e `@`).
3. Variáveis de múltiplos tipos (`inteiro`, `real`).
4. Laço de repetição com variável de controle (`para (inteiro i = 1 ate limite passo 1)`).
5. Estrutura condicional de desvio (`se / senao`).
6. Primitivas de saída formatada no console (`escrevaln`).

```text
// experimento.ijk - Demonstração Prática do ijkLang

programa ProcessamentoNumericoApp {
    // 1. Função com cálculo aritmético e retorno tipado
    privado calcularTermo(inteiro indice, real escala): real {
        retorne (indice * 10) * escala;
    }

    // 2. Procedimento que altera variável de memória via ponteiro
    privado duplicarValor(ponteiro inteiro ptr_item) {
        @ptr_item = @ptr_item * 2;
    }

    // 3. Ponto de entrada obrigatório da aplicação (privado principal(): inteiro)
    privado principal(): inteiro {
        // Declaração de variáveis locais
        real fator_escala = 1.5;
        inteiro limite = 5;
        real acumulador = 0.0;
        inteiro valor_referencia = 10;

        escrevaln("=== Iniciando Processamento ijkLang ===");

        // Chamada do procedimento passando o endereço da variável (&)
        duplicarValor(&valor_referencia);
        escrevaln("Valor de referencia duplicado: ", valor_referencia);

        // 4. Laço de repetição iterando de 1 até o limite
        para (inteiro i = 1 ate limite passo 1) {
            real termo = calcularTermo(i, fator_escala);
            acumulador = acumulador + termo;
            escrevaln("Passo ", i, " | Termo: ", termo, " | Acumulado: ", acumulador);
        }

        // 5. Estrutura condicional baseada no valor acumulado
        se (acumulador >= 100.0) {
            escrevaln("Status: Meta atingida com sucesso (>= 100.0).");
        } senao {
            escrevaln("Status: Meta nao atingida (< 100.0).");
        }

        escrevaln("=== Processamento Concluido ===");
        retorne 0;
    }
}
```

---

## 4. O Ciclo Completo

Com os executáveis gerados, acompanhe o fluxo passo a passo de como o compilador processa o arquivo até a execução pela máquina virtual.

> **Nota para execução**: No Windows utilize os binários em `bin\Debug\` (e.g. `.\bin\Debug\ijkl.exe`); no Linux/macOS utilize os binários em `bin/` (e.g. `./bin/ijkl`). O idioma padrão do vocabulário é definido no arquivo `ijkl.ini` (`language = pt-BR`) e pode ser sobrescrito diretamente via linha de comando com a flag `--lang` (e.g. `--lang pt-BR` ou `--lang en-US`).

### Passo 4.1: Inspeção Léxica (Tokens)

Visualize a sequência de tokens gerada pelo Autômato Finito Determinístico:

```bash
# Windows
.\bin\Debug\ijkl.exe --tokens experimento.ijk

# Linux
./bin/ijkl --tokens experimento.ijk
```

### Passo 4.2: Visualização da Árvore Sintática Abstrata (AST)

Exiba a hierarquia estrutural construída pelo analisador descendente recursivo:

```bash
# Windows
.\bin\Debug\ijkl.exe --ast experimento.ijk

# Linux
./bin/ijkl --ast experimento.ijk
```

### Passo 4.3: Inspeção da Tabela de Símbolos e Escopos

Verifique a resolução de escopos e os deslocamentos (_offsets_) de memória calculados para cada variável:

```bash
# Windows
.\bin\Debug\ijkl.exe --symtab experimento.ijk

# Linux
./bin/ijkl --symtab experimento.ijk
```

### Passo 4.4: Geração do Código Intermediário Linear (`.ci`)

Compile o código-fonte emitindo o arquivo intermediário textual (tanto a extensão `.ijk` na entrada quanto a opção de saída `-o` são opcionais; ao omitir a extensão na entrada, `.ijk` é assumido, e ao omitir `-o`, o compilador assume automaticamente o nome do arquivo-fonte com a extensão `.ci`):

```bash
# Windows (infere 'experimento.ijk' e gera 'experimento.ci' automaticamente)
.\bin\Debug\ijkl.exe experimento

# Ou especificando a extensão e/ou o arquivo de saída via -o:
.\bin\Debug\ijkl.exe experimento.ijk
.\bin\Debug\ijkl.exe -o experimento.ci experimento.ijk

# Linux / macOS (infere 'experimento.ijk' e gera 'experimento.ci' automaticamente)
./bin/ijkl experimento

# Ou especificando a extensão e/ou o arquivo de saída via -o:
./bin/ijkl experimento.ijk
./bin/ijkl -o experimento.ci experimento.ijk
```

O arquivo `experimento.ci` gerado conterá instruções textuais legíveis baseadas em registradores e saltos:

```text
#calcularTermo:
    alloc_frame 16;
    r0 := [ra, 0];
    ...
#ProcessamentoNumericoApp:
    alloc_frame 48;
    ...
    call #duplicarValor;
    ...
    jump_if r0, #se_verdadeiro_3;
    ...
```

### Passo 4.5: Execução do Programa na Máquina Virtual

Execute o programa na máquina virtual `ijkl-m` (a extensão `.ci` é opcional; quando omitida, o interpretador infere automaticamente `.ci`):

```bash
# Windows (infere 'experimento.ci' automaticamente)
.\bin\Debug\ijkl-m.exe experimento

# Ou informando explicitamente a extensão:
.\bin\Debug\ijkl-m.exe experimento.ci

# Linux / macOS (infere 'experimento.ci' automaticamente)
./bin/ijkl-m experimento

# Ou informando explicitamente a extensão:
./bin/ijkl-m experimento.ci
```

#### Saída Obtida no Console:

```text
=== Iniciando Processamento ijkLang ===
Valor de referencia duplicado: 20
Passo 1 | Termo: 15.000000 | Acumulado: 15.000000
Passo 2 | Termo: 30.000000 | Acumulado: 45.000000
Passo 3 | Termo: 45.000000 | Acumulado: 90.000000
Passo 4 | Termo: 60.000000 | Acumulado: 150.000000
Passo 5 | Termo: 75.000000 | Acumulado: 225.000000
Status: Meta atingida com sucesso (>= 100.0).
=== Processamento Concluido ===
```

### Passo 4.6: Execução com Métricas de Ciclos (`--stats`)

Para analisar o custo computacional e os recursos alocados pelo programa:

```bash
# Windows
.\bin\Debug\ijkl-m.exe --stats experimento.ci

# Linux
./bin/ijkl-m --stats experimento.ci
```

A máquina virtual imprimirá o relatório ao término da execução:

```text
--- ijkLang VM Execution Statistics ---
  Executed Instructions: 287 cycles
  Peak Stack Allocated:  0 bytes
  Total Heap Allocated:  0 bytes
---------------------------------------
```

### Passo 4.7: Depuração Interativa Passo a Passo (`--debug`)

Inicie a máquina virtual em modo de depuração para inspecionar registradores e memória durante a execução:

```bash
# Windows
.\bin\Debug\ijkl-m.exe --debug experimento.ci

# Linux
./bin/ijkl-m --debug experimento.ci
```

#### Comandos Úteis do Depurador:

- `step` (ou `s`): Executa a próxima instrução (_step into_).
- `regs` (ou `r`): Exibe o conteúdo de `r0` a `r4`, `ci`, `sp` e `ra`.
- `stack 5` (ou `k 5`): Inspeciona as últimas 5 células da pilha de chamadas.
- `break #duplicarValor` (ou `b`): Define um ponto de parada no rótulo da sub-rotina.
- `continue` (ou `c`): Prossegue a execução contínua até o próximo ponto de parada.
- `quit` (ou `q`): Encerra o depurador.

---

## 5. Execução da Suíte Completa de Testes

O repositório conta com uma suíte de testes automatizados com 100% de cobertura nos exemplos em português (`pt-BR`) e inglês (`en-US`), cobrindo as cinco fases da compilação e execução (163 testes de estágios) além da validação rigorosa da arquitetura modular multunidades (17 testes de integração).

### 5.1 No Windows (PowerShell)

Execute os scripts de automação PowerShell a partir da raiz do projeto:

```powershell
# Suíte completa de estágios do compilador e VM (163 testes)
powershell -ExecutionPolicy Bypass -File .\tests\run_tests.ps1

# Suíte de integração da arquitetura modular (17 testes)
powershell -ExecutionPolicy Bypass -File .\tests\run_integration_tests.ps1

# Suíte de testes da aplicação ONM (12 testes)
powershell -ExecutionPolicy Bypass -File .\examples\apps\onm\tests\run_tests.ps1
```

### 5.2 No Linux ou macOS (Bash / Make)

Em distribuições Linux ou macOS, você pode executar os testes diretamente através dos scripts Bash correspondentes ou através do utilitário `make`:

```bash
# Opção A: Execução direta via scripts Bash
chmod +x tests/run_tests.sh tests/run_integration_tests.sh examples/apps/onm/tests/run_tests.sh
./tests/run_tests.sh
./tests/run_integration_tests.sh
./examples/apps/onm/tests/run_tests.sh

# Opção B: Execução integrada via Makefile
make test
make test-integration
```

---

## 6. Publicação e Distribuição para Projetos Consumidores (`publish`)

Para distribuir os artefatos compilados do `ijkLang` diretamente para outro diretório ou repositório consumidor (como `../ijkDemo`), utilize os comandos de publicação automatizada. A ação realiza uma compilação limpa (`clean` seguido de `all` / `Rebuild`) e sincroniza todos os arquivos essenciais para execução: binários (`bin/ijkl`, `bin/ijkl-m`), configuração (`ijkl.ini`), dicionários de palavras-chave (`keywords/*.words`) e a biblioteca de exemplos (`examples/`).

### 6.1 No Linux ou macOS (Make)

```bash
# Sintaxe posicional direta:
make publish ../ijkDemo

# Ou especificando a variável DEST:
make publish DEST=../ijkDemo
```

> **Nota de flexibilidade**: Se o caminho informado terminar em `bin` (ex.: `make publish ../ijkDemo/bin`), os binários e dicionários serão copiados diretamente nele. Caso aponte para a raiz do projeto (ex.: `make publish ../ijkDemo`), a subpasta `bin/` será preservada automaticamente.

### 6.2 No Windows (PowerShell / MSBuild)

```powershell
# Rebuild completo via MSBuild e cópia automatizada:
.\publish.ps1 ..\ijkDemo

# Ou com configuração de Release:
.\publish.ps1 ..\ijkDemo -Configuration Release
```

---

## 7. Próximos Passos

Para aprofundar-se nos aspectos teóricos e construtivos do projeto, consulte a documentação detalhada na pasta `docs/`:

- [00. Sumário Geral e Guia de Leitura](docs/00_sumario_e_guia_de_leitura.md)
- [01. Filosofia e Visão Geral](docs/01_filosofia_e_visao_geral.md)
- [02. Manual Prático da Linguagem](docs/02_manual_da_linguagem_ijklang.md)
- [03. Arquitetura do Compilador](docs/03_arquitetura_do_compilador.md)
- [04. Código Intermediário e Máquina Virtual](docs/04_codigo_intermediario_e_maquina_virtual.md)
- [05. Barramento de I/O e Plugins Didáticos](docs/05_barramento_io_e_plugins_didaticos.md)
- [06. Hardware Digital e Síntese em FPGA](docs/06_hardware_digital_e_sintese_fpga.md)
- [07. Padrões de Projeto e Clean Code em C11](docs/07_padroes_de_projeto_e_clean_code.md)
- [Apêndices A, B e C (Regras Léxicas, Gramática Sintática e Guia de Ensino)](docs/00_sumario_e_guia_de_leitura.md#relação-de-documentos)
