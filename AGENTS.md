# Diretrizes Arquiteturais e Operacionais para Agentes de IA — ijkLang (ijkL)

Este documento estabelece as diretrizes obrigatórias de engenharia de software, padrões de codificação em C (ISO C11) e o protocolo de gestão autônoma de backlog para agentes de inteligência artificial (e colaboradores humanos) no repositório **ijkLang (ijkL)**.

---

## 1. Gestão Autônoma de Tarefas e Backlog (GNAK)

Todo trabalho desenvolvido por agentes deve ser orientado pelo protocolo **GNAK (Git-Native Agentic Kanban)** detalhado em [`backlog/PROCESS.md`](backlog/PROCESS.md):

1. **Estrutura de Pastas e Estados:**
   * `backlog/01_plan/`: Rascunhos, diagnósticos e planejamento.
   * `backlog/02_todo/`: Tarefas aprovadas e prontas para execução (com critérios de aceite fechados).
   * `backlog/03_doing/`: Tarefa em execução ativa pelo agente.
   * `backlog/04_done/`: Tarefa concluída, validada e com evidências registradas.
   * `backlog/05_blocked/`: Tarefas temporariamente impedidas (ambiente, decisão arquitetural, validação). Responsabilidade exclusiva do mantenedor; agentes autônomos nunca se autoatribuem tarefas nesta pasta.
   * *Atenção:* Manter sempre o arquivo `.gitkeep` em cada pasta.

2. **Nomenclatura do Arquivo:**
   * Formato: `backlog/<pasta>/[PRIORIDADE]-[DATA_HORA]-[SLUG].txt`
   * Exemplo: `00300-20261102-154607-corrigir-compilacao-ubuntu.txt`
   * Prioridade decimal com zeros à esquerda: menor número = maior urgência (`00050` Máxima/Arquitetural, `00100` Crítica, `00200` Alta, `00300` Normal, `00400` Baixa, `00500` Desejável).

3. **Nomenclatura da Branch Git:**
   * Formato: `[DATA_HORA]-[SLUG]` (a prioridade **NÃO** faz parte da branch).
   * Exemplo: `20261102-154607-corrigir-compilacao-ubuntu`

4. **Ciclo Iterativo do Agente, Regras de Dependência e Lock por Branch Remota:**
   * Ao ser solicitado a verificar o backlog: inspecionar as pastas e relatar tarefas, estados e prioridades.
   * **Verificação de Dependências (Definition of Ready - DoR):** Inspecionar o campo `Dependências:` de cada tarefa candidata em `02_todo/`. A tarefa só pode ser assumida se **todas as suas dependências estiverem concluídas em `04_done/` na branch `main`**. Se alguma dependência estiver em `01_plan/`, `02_todo/`, `03_doing/` ou `05_blocked/`, a tarefa deve ser **pulada**.
   * **Lock por Branch Remota:** Consultar as branches remotas (`git fetch --prune origin && git branch -r`). Pular tarefas elegíveis cuja branch remota `origin/[DATA_HORA]-[SLUG]` já exista.
   * **Isolamento Estrito de Branch:** O agente deve criar sua branch **exclusivamente a partir da `origin/main` atualizada** (`git checkout main && git pull origin main && git checkout -b <branch>`). É estritamente proibido criar branches a partir de branches ativas (`doing`) de outros agentes (*stacked branches*).
   * Publicar a branch imediatamente (`git push -u origin <branch>`) para registrar o lock atômico no Git. Se o push for aceito, mover o arquivo para `03_doing/` (`git mv`), atualizar cabeçalho e comitar o claim na branch. Se o push for rejeitado por colisão, descartar a branch e avaliar a próxima tarefa.
   * **Tratamento de Bloqueios:** Se uma tarefa for impedida durante o desenvolvimento, o agente opera **exclusivamente dentro de sua branch** (nunca comita na `main`), move o arquivo de `03_doing/` para `05_blocked/`, anota os metadados de bloqueio (motivo, sugestão de resolução, responsável pelo desbloqueio: mantenedor), comita, envia push e mantém o PR em Draft `[BLOCKED]`.
   * **Ao concluir:** Anexar comandos, saídas e evidências no próprio arquivo, mover para `04_done/` (`git mv`), comitar e abrir PR / mergear na `main`.
   * Consulte o protocolo completo em [`backlog/PROCESS.md`](backlog/PROCESS.md).

---

## 2. Padrões de Nomenclatura e Clean Code em C (ISO C11)

1. **Identificadores em Inglês Descritivo**:
   * Todo identificador em código C (funções, estruturas, tipos enumerados, variáveis locais, parâmetros e campos) deve ser redigido em inglês (`en-US`), com palavras completas e altamente descritivas.
   * **Proibição de Abreviações e Siglas Crípticas**: Evite abreviações como `tok`, `stmt`, `decl`, `expr`, `cond`, `spec`, `arg`, `ret`, `ln`, `len`, `tmp`. Prefira sempre: `token`, `statement`, `declaration`, `expression`, `condition_expression`, `type_specification`, `argument`, `return_value`, `line_number`, `buffer_length`, `temporary_value`.
   * **Uso Excepcional de Siglas**: Quando uma sigla técnica consolidada for inevitável (e.g., `AST`, `AFD`, `UTF8`, `CI`, `EOF`), seu significado formal deve ser documentado em comentário de cabeçalho no respectivo arquivo.

2. **Padrão Semântico de Namespace (`ijkl_modulo_acao`)**:
   * Funções públicas de módulos devem seguir o padrão:
     `ijkl_<modulo>_<acao>` (e.g., `ijkl_memory_arena_allocate`, `ijkl_lexer_next_token`, `ijkl_parser_parse_compilation_unit`, `ijkl_ast_print`).
   * Estruturas de dados públicas: `Ijkl<NomeEntidade>` (e.g., `IjklMemoryArena`, `IjklLexer`, `IjklParser`, `IjklAstNode`, `IjklTypeSpec`).

3. **Política Rigorosa de Comentários**:
   * **Comentários Explicativos em Português (`pt-BR`)**: Os comentários devem contextualizar o propósito pedagógico e o funcionamento dos módulos em português culto.
   * **Apenas Efeitos Colaterais e Propósitos Não Óbvios**: Documente construções que realizem mutações de estado, regras de precedência gramatical especiais, alocações de memória ou estratégias preditivas de lookahead.
   * **Evitar Comentários Redundantes**: Não escreva comentários em construções cujo propósito já seja autoevidente e explícito pelo próprio código.

4. **Distribuição de Responsabilidades e Modularidade**:
   * Módulos coesos e bem delimitados com interfaces limpas em `include/`:
     * `memory.h` / `memory.c`: Gerenciamento de memória baseado em regiões (Arenas).
     * `utf8.h` / `utf8.c`: Decodificação e manipulação de fluxos Unicode UTF-8.
     * `token.h` / `token.c`: Definição e classificação de tokens léxicos.
     * `vocabulary.h` / `vocabulary.c`: Gestão do vocabulário multilingue e alfabeto complementar.
     * `config.h` / `config.c`: Carregamento do arquivo de configuração `ijkl.ini`.
     * `lexer.h` / `lexer.c`: Analisador léxico determinístico por tabela de transição.
     * `ast.h` / `ast.c`: Árvore Sintática Abstrata e formatador de visualização hierárquica ASCII.
     * `parser.h` / `parser.c`: Analisador sintático descendente recursivo com lookahead preditivo.
     * `symtab.h` / `symtab.c`: Tabela de símbolos encadeada por escopos.
   * Referência de excelência: observe a estrutura e clareza dos arquivos [src/compiler/main.c](src/compiler/main.c) e [src/compiler/lexer.c](src/compiler/lexer.c).

5. **Gerenciamento de Memória Sem Vazamentos**:
   * Todo nó sintático, tipo e string intermediária gerada durante a compilação deve ser alocado na `IjklMemoryArena`, permitindo alocação e destruição em tempo $O(1)$ ao final do ciclo de compilação.

---

## 3. Diretriz de Paridade de Builtins (CI vs. `--native-builtins`)

Para assegurar consistência semântica e portabilidade pedagógica do ecossistema ijkLang, toda rotina ou biblioteca que integre o mecanismo de builtins deve observar rigorosamente a paridade funcional entre seus modos de compilação e execução:

1. **Distinção Conceitual Obrigatória**:
   * **Algoritmo em Código Intermediário (CI padrão)**: Código emitido pelo compilador quando executado no modo padrão (sem flags de builtins nativos). Contém instruções assembly da VM (`.ci`) que implementam a lógica na própria máquina virtual ou acionam o barramento de E/S.
   * **Despacho Nativo de Builtin (`--native-builtins`)**: O compilador emite a instrução `call #__builtin_<nome>`, interceptada pela VM em `execute_native_builtin()` (`src/vm/vm_core.c`), executando a rotina implementada diretamente em C para máxima performance.
   * **Backend de E/S do Barramento da VM**: Instruções de E/S (`in` / `out`) lendo ou escrevendo em portas registradas (ex.: `0x0005` teclado, `0x0008`/`0x0009` relógio). O código em C da porta comunica com o sistema operacional; isso é infraestrutura de periférico da VM e **não** substitui nem constitui, isoladamente, a implementação do builtin nativo.

2. **Regra Mandatória de Paridade Semântica 100%**:
   * Nenhuma funcionalidade de biblioteca elegível para builtin pode ser disponibilizada apenas em CI ou apenas em C nativo.
   * Ambos os caminhos devem preservar estritamente as mesmas assinaturas, tipos de parâmetros e retorno, formato de estruturas na memória da VM, tratamento de erros e efeitos colaterais.

3. **Estratégia para Fontes Não-Determinísticas**:
   * Testes automatizados de paridade entre modo padrão e modo nativo não devem comparar leituras de tempo ou teclado realizadas em momentos distintos.
   * **Relógio/DataHora**: A validação de paridade deve testar funções determinísticas (ex.: decomposição de timestamp fixo `fromUnixTimestamp(1700000000)`, `elapsedSecondsOfDay`, `toString`).
   * **Teclado**: Validar com buffers controlados (stream vazia ou pré-injetada), assegurando que ambos os modos reportam idêntica disponibilidade (`hasKey`) e leitura (`readKeyAsync`).
   * **Gerador Aleatório (PRNG)**: Para a mesma semente inicial fixa (`seed(s)`), a sequência produzida por `next()`, `nextRange()` e `nextReal()` deve ser 100% idêntica bit a bit entre o modo CI e o modo `--native-builtins`.

4. **Validação Cruzada de Aplicações**:
   * Aplicações de usuário (ex.: `spell`, `wordguess`, `prime`) devem ter suítes de teste que as compilem e executem em ambos os modos (`app.ci` e `app_native.ci`), garantindo saídas e códigos de retorno estritamente equivalentes em Windows e Linux.

---

## 4. Acordos de colaboração e operação deste ambiente

- Responder em português, de forma sucinta, descrevendo o resultado, o que foi
  alterado, a validação efetiva e as pendências. Este ambiente Ubuntu Server é
  usado pelo mantenedor somente via console/teclado; fornecer comandos completos
  de terminal e tornar scripts executáveis quando houver execução direta.
- Preservar as diretrizes existentes deste arquivo. Registrar aqui acordos
  duradouros; manter o histórico de execução nos arquivos de tarefa do backlog.
  Não copiar a transcrição inteira do chat nem transformar uma autorização
  pontual (por exemplo, excluir uma branch) em permissão permanente.
- Seguir o fluxo Git/GNAK descrito acima, mantendo trabalhos independentes em
  branches próprias originadas de origin/main. Preservar branches/PRs em revisão
  por outros agentes. Não misturar correções sem relação na mesma entrega.
- Toda tarefa deve ser autossuficiente: contexto, ambiente, versões, reprodução,
  resultado esperado/observado, escopo, dependências, aceite, decisões, comandos,
  códigos de saída, evidências, riscos e pendências no mesmo arquivo .txt.
  Incorporar logs relevantes nesse arquivo; preservar evidências originais.
  O timestamp é de criação, em UTC, e permanece estável ao mudar o estado.
- Distinguir testes executados de inspeção de código. Validar CI e nativo e as
  plataformas exigidas. Sem Windows disponível, registrar o impedimento conforme
  PROCESS.md, mantendo PR Draft [BLOCKED]; não afirmar validação ou aprovação.
- Manter os PRs integrados como histórico. Excluir branches somente quando
  autorizado e após confirmar a integração; atualizar main por fast-forward e
  preservar alterações locais. Merge não comprova aprovação formal de revisão.
- Não registrar senhas, tokens, conteúdo de chaves privadas ou credenciais em
  scripts, arquivos de instruções, backlog, logs ou commits. Usar autenticação
  SSH já configurada e permissões adequadas (.ssh: 700; chave privada: 600;
  chave pública: 644), sem exibir o conteúdo da chave privada.
- Não ampliar sudo/sandbox nem conceder sudo sem senha como solução automática.
  Usar sudo apenas nos comandos que precisarem dele; deixar o sudo solicitar a
  senha no terminal quando necessário. Não iniciar o agente inteiro como root.
- Para operações em discos, verificar identidade, sistema de arquivos e montagem
  atuais. Os nomes /dev/sdX podem mudar; preferir UUID. Distinguir montagem rw
  de permissão de escrita do usuário (em exFAT, uid/gid/umask). Não formatar,
  apagar ou forçar desmontagem para resolver somente permissões.
- `doit.sh` é uma conveniência específica deste Ubuntu: remonta o pendrive
  exFAT UUID 2A81-1677 em /mnt/usb para devops. Exige exatamente a tecla y
  minúscula; qualquer outra cancela. Não executá-lo automaticamente em build,
  teste ou inicialização. Em outra máquina, confirmar/ajustar sua configuração.
- Mudanças de data para testes são temporárias e exigem instrução explícita.
  Verificar o Chrony e distinguir relógio do sistema de RTC; se solicitado manter
  NTP parado, aguardar o pedido de reativação. Ao restaurar, verificar a fonte
  NTP e o relógio; não declarar sincronização sem evidência.
- Contexto local conhecido, sujeito a nova verificação: usuário devops;
  repositório ~/repos/ijkLang; dados/modelos Ollama em /mnt/data/ollama/models
  no NVMe; pendrive em /mnt/usb. Confirmar serviços SSH/Chrony, IPs e dispositivos
  ao consultá-los, pois esses estados não são garantias permanentes.
