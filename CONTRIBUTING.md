# Guia de Contribuição e Padrões de Engenharia — ijkLang (`ijkL`)

Agradecemos o interesse em contribuir com o ecossistema educacional **ijkLang (`ijkL`)**. Este projeto adota padrões rigorosos de engenharia de software e práticas de código limpo (*Clean Code*) com a norma **ISO C11**, visando a máxima clareza pedagógica e excelência técnica para o ensino de computação.

---

## 1. Princípios Gerais do Projeto

1. **Finalidade Educacional e Rigor Acadêmico**:
   - Todo o código-fonte deve servir como modelo de referência para estudantes de graduação. Clareza e manutenibilidade têm precedência sobre micro-otimizações obscuras.
2. **Separação de Responsabilidades e Modularidade**:
   - O projeto é organizado em módulos coesos, com interfaces públicas declaradas em `include/` e implementações isoladas em `src/`.
   - Módulos compartilhados entre o compilador e a máquina virtual residem exclusivamente em `src/common/`.
3. **Gestão Segura de Memória**:
   - Estruturas dinâmicas durante as fases de compilação devem ser alocadas prioritariamente na `IjklMemoryArena`, permitindo alocação e desalocação em tempo constante $O(1)$ ao término do ciclo.
   - Recursos alocados no heap tradicional (`malloc` / `free`) devem possuir ciclo de vida explicitamente delimitado, garantindo ausência total de vazamentos de memória.

---

## 2. Padrões de Codificação em C (ISO C11)

### 2.1 Identificadores em Inglês Descritivo
- Todos os identificadores no código-fonte (nomes de funções, estruturas, variáveis locais, parâmetros e campos) devem ser redigidos em inglês (`en-US`), com palavras completas e altamente expressivas.
- **Proibição de Abreviações Crípticas**: Evite contrações como `tok`, `stmt`, `decl`, `expr`, `cond`, `spec`, `arg`, `ret`, `ln`, `len`, `tmp`. Prefira sempre:
  - `token`
  - `statement`
  - `declaration`
  - `expression`
  - `condition_expression`
  - `type_specification`
  - `argument`
  - `return_value`
  - `line_number`
  - `buffer_length`
  - `temporary_value`
- **Siglas Técnicas Consolidadas**: Siglas como `AST`, `AFD`, `UTF8` ou `EOF` são permitidas e devem ser documentadas formalmente quando introduzidas em novos módulos.

### 2.2 Padrão Semântico de Namespace (`ijkl_modulo_acao`)
- **Funções públicas**: Devem seguir o prefixo canônico `ijkl_<modulo>_<acao>`:
  - `ijkl_memory_arena_allocate`
  - `ijkl_lexer_next_token`
  - `ijkl_parser_parse_compilation_unit`
  - `ijkl_code_generator_generate`
- **Tipos e estruturas de dados públicas**: Utilizam estilo PascalCase iniciado por `Ijkl`:
  - `IjklMemoryArena`
  - `IjklLexer`
  - `IjklParser`
  - `IjklAstNode`
  - `IjklTypeSpec`
  - `IjklSymbolTable`

### 2.3 Política de Comentários
- **Comentários Técnicos em Português (`pt-BR`)**: Os comentários explicativos que contextualizam o propósito pedagógico e o funcionamento dos módulos devem ser redigidos em português culto.
- **Foco em Efeitos Colaterais e Decisões Não Óbvias**: Documente construções que realizem mutações de estado, regras especiais de precedência gramatical, alocações de recursos ou estratégias de previsão sintática (*lookahead*).
- **Sem Comentários Redundantes**: Não inclua comentários triviais em instruções cujo propósito seja autoevidente pelo próprio código.

---

## 3. Fluxo de Trabalho e Submissão

### 3.1 Criação de Ramificação (Branch)
Crie uma ramificação descritiva a partir da branch principal (`main`):

```bash
git checkout -b feature/nome-da-melhoria
# ou:
git checkout -b fix/descricao-do-ajuste
```

### 3.2 Execução Obrigatória da Suíte de Testes
Antes de submeter qualquer contribuição, todas as suítes de testes automatizados devem passar com 100% de aprovação.

**No Windows (PowerShell):**
```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run_tests.ps1
powershell -ExecutionPolicy Bypass -File .\tests\run_integration_tests.ps1
powershell -ExecutionPolicy Bypass -File .\examples\apps\onm\tests\run_tests.ps1
```

**No Linux ou macOS (Bash / Make):**
```bash
./tests/run_tests.sh
./tests/run_integration_tests.sh
./examples/apps/onm/tests/run_tests.sh
# ou:
make test
make test-integration
```

### 3.3 Mensagens de Commit
As mensagens de commit devem ser claras, objetivas e em português ou inglês, descrevendo o que foi alterado e a motivação técnica:

```text
Adiciona suporte a especificador de tipo literal em expressoes constantes
Refatora alocador da tabela de simbolos para reutilizacao da arena
Corrige resolucao de caminhos relativos no carregador de modulos
```

---

## 4. Licença e Atribuição

Ao submeter contribuições para o repositório **ijkLang (`ijkL`)**, você concorda que suas modificações serão licenciadas sob os mesmos termos da **PolyForm Noncommercial License 1.0.0**, mantendo o projeto acessível para fins pedagógicos, acadêmicos e de estudo pessoal, e vedada a sua exploração comercial.
