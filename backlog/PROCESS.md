# Processo Operacional e Gestão de Backlog para Agentes de IA (GNAK)

Este documento estabelece o protocolo mandatório de engenharia e gestão de tarefas para agentes autônomos de IA e colaboradores humanos no repositório **ijkLang (ijkL)**. 

O modelo adotado é o **GNAK (Git-Native Agentic Kanban)**: um sistema de Kanban baseado em arquivos locais versionados no Git, garantindo atomicidade, total rastreabilidade, independência de ferramentas externas e integridade do histórico do software.

---

## 1. Estrutura de Diretórios e Estados do Kanban

O ciclo de vida de qualquer demanda técnica percorre quatro pastas sequenciais dentro de `backlog/`:

| Diretório | Estado Kanban | Descrição e Critério de Entrada |
| :--- | :--- | :--- |
| `backlog/01_plan/` | **PLAN** | Ideias, diagnósticos iniciais, rascunhos de arquitetura e propostas em refinamento. Critérios de aceite ainda incompletos. |
| `backlog/02_todo/` | **TODO** | Tarefas prontas para execução. Possui título claro, objetivo delimitado, prioridade definida e critérios de aceite objetivos e verificáveis. |
| `backlog/03_doing/` | **DOING** | Tarefa em desenvolvimento ativo por um agente ou desenvolvedor. A branch de trabalho correspondente deve estar criada. |
| `backlog/04_done/` | **DONE** | Tarefa concluída com sucesso. Código validado pelos testes em todas as plataformas exigidas, com registros de execução e evidências anexadas ao próprio arquivo. |

> [!IMPORTANT]
> **Preservação de Pastas Vazias:** Toda pasta em `backlog/` deve conter um arquivo `.gitkeep`. Quando uma tarefa transitar de pasta, **nunca** apague o `.gitkeep` da pasta de origem. O Git não versiona diretórios vazios e apagaria a pasta se o `.gitkeep` for removido.

---

## 2. Nomenclatura de Arquivos e Branches

O desacoplamento entre a prioridade (no arquivo) e o identificador temporal (na branch) garante estabilidade no Git caso uma tarefa seja repriorizada antes de iniciar.

### 2.1. Nomenclatura do Arquivo de Tarefa
O arquivo de uma tarefa deve seguir estritamente o formato:
```
backlog/<pasta>/[PRIORIDADE]-[DATA_HORA]-[SLUG_DESCRITIVO].txt
```

* **`[PRIORIDADE]`**: 5 dígitos decimais com preenchimento de zeros à esquerda (`00050` a `00500`).
  * O menor número indica **maior prioridade/urgência**.
  * A ordenação alfabética natural do sistema de arquivos e comandos (`ls`, `dir`) coloca automaticamente as tarefas mais críticas no topo.
* **`[DATA_HORA]`**: Data e hora de criação em UTC no formato `YYYYMMDD-HHMMSS` (15 caracteres numéricos separados por hífen).
* **`[SLUG_DESCRITIVO]`**: Descrição sucinta da tarefa em `kebab-case` (apenas letras minúsculas ASCII, dígitos e hifens).
* **Exemplo:** `00300-20261102-154607-corrigir-compilacao-ubuntu.txt`

### 2.2. Nomenclatura da Branch Git
A branch associada à tarefa deve corresponder exatamente ao sufixo temporal e slug do arquivo, **sem o prefixo de prioridade**:
```
[DATA_HORA]-[SLUG_DESCRITIVO]
```
* **Exemplo:** `20261102-154607-corrigir-compilacao-ubuntu`

**Por que a prioridade não entra no nome da branch?**
Se uma tarefa for repriorizada de `00300` para `00100` durante o planejamento em `01_plan` ou `02_todo`, apenas o arquivo é renomeado. A branch Git, histórico de commits, links de pull requests e referências em discussões permanecem imutáveis e válidos. O vínculo entre a branch e a tarefa permanece 100% determinístico através do identificador `[DATA_HORA]-[SLUG_DESCRITIVO]`.

---

## 3. Escala Decimal de Prioridades

Para manter espaçamento que permita inserir tarefas intermediárias sem renumerar arquivos existentes, adota-se a seguinte escala padrão:

| Prefixo | Nível de Prioridade | Descrição / Diretriz |
| :---: | :--- | :--- |
| `00050` | **Máxima / Governança / Bloqueante** | Governança arquitetural, contratos de interface e diretrizes de paridade que bloqueiam o refinamento ou execução de outras tarefas. |
| `00100` | **Crítica / Blocker** | Falha de compilação na branch principal, quebra de CI, regressão grave em produção ou vulnerabilidade crítica. |
| `00200` | **Alta** | Defeito relevante de comportamento, requisito bloqueante para release ou suporte multiplataforma urgente. |
| `00300` | **Normal / Média** | Desenvolvimento de nova funcionalidade planejada, refatorações padrão de módulos ou expansões de suíte de testes (prioridade padrão). |
| `00400` | **Baixa** | Melhorias secundárias, ajustes de formatação, documentação acessória ou polimento visual de logs. |
| `00500` | **Desejável / Futuro** | Ideias para versões futuras, protótipos exploratórios ou débitos técnicos de baixíssimo impacto. |

*Intervalos intermediários (ex.: `00150`, `00250`, `00350`) podem ser utilizados para posicionar uma tarefa entre dois níveis sem quebrar a ordem alfabética.*

---

## 4. Protocolo Operacional para Agentes de IA

Todo agente de IA (Codex, Antigravity, Claude, Cursor, etc.) operando neste repositório deve seguir rigorosamente as quatro fases descritas abaixo:

### Fase 1: Varredura e Resposta sobre o Estado do Backlog
Quando solicitado a verificar as tarefas do projeto, o agente deve:
1. Listar o conteúdo de `backlog/01_plan/`, `backlog/02_todo/`, `backlog/03_doing/` e `backlog/04_done/`.
2. Para cada arquivo encontrado, inspecionar as primeiras linhas para extrair:
   * **Título**
   * **ID / Arquivo**
   * **Prioridade** (extraída do prefixo numérico e do campo `Prioridade:`)
   * **Status** (correspondente à pasta onde reside)
   * **Responsável** e **Branch de trabalho**
3. Apresentar uma tabela organizada por estado e ordenada por prioridade (menor número primeiro).

### Fase 2: Seleção e Assunção de Tarefa
1. Identificar em `backlog/02_todo/` a tarefa com maior prioridade (menor prefixo numérico). Caso haja empate, escolher a de menor timestamp (mais antiga).
2. Mover o arquivo da tarefa de `02_todo/` para `03_doing/` usando `git mv`:
   ```bash
   git mv backlog/02_todo/<arquivo>.txt backlog/03_doing/<arquivo>.txt
   ```
3. Atualizar o cabeçalho do arquivo:
   * `Status: DOING (em andamento)`
   * `Responsável: <Nome_do_Agente>`
   * `Início UTC: <Timestamp_ISO8601>`
   * `Branch de trabalho: <DATA_HORA>-<SLUG>`
4. Criar e alternar para a branch a partir da `main`:
   ```bash
   git checkout -b <DATA_HORA>-<SLUG>
   ```

### Fase 3: Execução, Testes e Registro Contínuo
1. Implementar o código em estrita conformidade com as diretrizes de Clean Code em `AGENTS.md` (C11, nomes em inglês descritivo, namespace `ijkl_`, gerenciamento por Arena).
2. **Diretriz de Paridade de Builtins (CI vs. `--native-builtins`)**:
   * Para tarefas de **biblioteca**: sempre implementar o algoritmo emitido em Código Intermediário (`.ci`) no modo padrão e o despacho nativo em C quando compilado com `--native-builtins`. Assegurar paridade semântica 100% entre ambos os modos.
   * Para tarefas de **aplicação**: sempre validar a compilação e execução tanto no modo padrão (`app.ci`) quanto no modo nativo (`app_native.ci`), confirmando equivalência estrita de comportamento e saída.
   * Para **fontes não-determinísticas** (relógio, teclado, PRNG): usar cenários determinísticos em testes automatizados (semente fixa para PRNG, timestamp fixo para DateTime, streams vazias/controladas para teclado).
3. Executar a compilação e as suítes de teste exigidas pelos critérios de aceite:
   * **Linux/Ubuntu:** `make -B -k -j1 all`, `make test`, `make test-integration`.
   * **Windows:** `MSBuild.exe ijkL.sln`, `tests/run_tests.ps1`, `tests/run_integration_tests.ps1`.
4. **Registro Vivo no Arquivo:** Atualizar a seção `RESULTADOS DA EXECUÇÃO` dentro do próprio arquivo da tarefa em `03_doing/`. Registrar comandos, códigos de saída (`exit_code`), contagem de testes e limitações encontradas.

### Fase 4: Conclusão e Entrega
1. Confirmar que 100% dos critérios de aceite da tarefa foram cumpridos.
2. Mover o arquivo da tarefa de `03_doing/` para `04_done/` usando `git mv`:
   ```bash
   git mv backlog/03_doing/<arquivo>.txt backlog/04_done/<arquivo>.txt
   ```
3. Atualizar o cabeçalho final do arquivo:
   * `Status: DONE (concluída)`
   * `Fim UTC: <Timestamp_ISO8601>`
4. Realizar o commit das alterações de código juntamente com o arquivo da tarefa em `04_done/`.
5. Publicar a branch e criar o Pull Request para revisão.

---

## 5. Template Padrão de Tarefa

Ao criar uma nova tarefa em `backlog/01_plan/` ou `backlog/02_todo/`, copie e preencha a estrutura padrão abaixo:

```text
TÍTULO: [Descrição imperativa clara do objetivo da tarefa]
ID: [PRIORIDADE]-[DATA_HORA]-[SLUG_DESCRITIVO]
Tipo: [feature | bugfix | refactor | docs | test | chore]
Status: [PLAN | TODO | DOING | DONE]
Prioridade: [00050 - Máxima/Governança | 00100 - Crítica | 00200 - Alta | 00300 - Normal | 00400 - Baixa | 00500 - Desejável]
Severidade: [bloqueante | alta | moderada | baixa | nenhuma]
Criado em UTC: YYYY-MM-DDTHH:MM:SS.000000+00:00
Solicitante: [Nome ou papel do solicitante]
Responsável: [Nome do agente ou desenvolvedor assumindo a tarefa]
Início UTC: [Data e hora de início ou 'Pendente']
Fim UTC: [Data e hora de conclusão ou 'Pendente']
Repositório local: [Caminho absoluto do workspace]
Remoto: [URL do repositório remoto]
Branch de trabalho: [DATA_HORA]-[SLUG_DESCRITIVO]

OBJETIVO E CONTEXTO
[Explique com clareza o problema técnico a ser resolvido, o contexto de negócio ou acadêmico e as motivações da mudança.]

RESULTADO ESPERADO E OBSERVADO
Esperado: [O que deve ocorrer após a entrega, sob quais condições.]
Observado: [O comportamento atual com defeito ou a ausência da funcionalidade.]
Impacto: [Qual parte do ecossistema é afetada.]

AMBIENTE E REPRODUÇÃO
[Sistemas operacionais, compiladores testados (GCC, Clang, MSVC) e comandos para reproduzir a situação.]

ESCOPO E LIMITES
[O que está explicitamente dentro do escopo e o que NÃO deve ser alterado nesta tarefa (evitar escopo aberto).]

CRITÉRIOS DE ACEITE
[ ] Implementação emitida em CI no modo padrão
[ ] Implementação nativa em C invocada com --native-builtins
[ ] Validação de paridade semântica (100% idêntica nos modos padrão e nativo)
[ ] Testes automatizados executados com sucesso em Ubuntu e Windows em ambos os modos
[ ] Atualizar este próprio arquivo com as evidências de teste e fechar o status para DONE

RESULTADOS DA EXECUÇÃO — [NOME DO AGENTE]
Início UTC: YYYY-MM-DDTHH:MM:SSZ.
Fim UTC: YYYY-MM-DDTHH:MM:SSZ.
Responsável: [Nome do Agente].
Branch: [DATA_HORA]-[SLUG_DESCRITIVO].

Causas raiz e alterações implementadas:
- [Arquivo/Função 1]: [Explicação técnica das mudanças]
- [Arquivo/Função 2]: [Explicação técnica das mudanças]

Comandos, códigos de saída e evidências:
- [Comando executado]: código de saída X, [detalhes de aprovação]

Compatibilidade e Riscos:
[Sistemas validados na prática vs. sistemas apenas inspecionados. Riscos e advertências residuais.]

Conclusão:
[Parecer final da execução confirmando o cumprimento de todos os critérios.]

ANEXO A — REGISTROS BRUTOS E LOGS DE EXECUÇÃO
[Cole aqui saídas relevantes de build, compilação ou falhas para comprovação histórica.]
```
