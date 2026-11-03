# Barramento de Entrada e Saída e Plugins Didáticos

O estudo da interação entre a Unidade Central de Processamento (CPU) e o ambiente externo constitui um dos temas centrais da disciplina de Arquitetura de Computadores. O ecossistema **ijkLang** introduz esse conceito de forma limpa e modular por meio de um **Barramento de I/O baseado em portas** e uma **API de Plugins Dinâmicos em C**.

---

## 1. Fundamentos de Entrada e Saída: I/O Mapeado em Portas

Na arquitetura de computadores, existem duas abordagens clássicas para comunicação com dispositivos periféricos:
1. **I/O Mapeado em Memória (*Memory-Mapped I/O*)**: As portas dos dispositivos compartilham o mesmo espaço de endereçamento da memória principal RAM.
2. **I/O Mapeado em Portas (*Port-Mapped I/O*)**: A CPU possui um espaço de endereçamento separado exclusivo para entrada e saída, acessado por instruções especializadas (`in` e `out`).

O ijkLang adota o modelo **Port-Mapped I/O**, implementando um barramento com espaço de endereçamento de **16 bits** (`0x0000` a `0xFFFF`), totalizando 65.536 portas lógicas acessíveis:

```mermaid
flowchart LR
    subgraph CPU ["Máquina Virtual (ijkl-m)"]
        REG["Registradores (r0..r4)"]
        EXEC["Instruções: in / out"]
    end

    subgraph Barramento ["Barramento de I/O (16-bit)"]
        BUS["Espaço de Portas (0x0000 a 0xFFFF)"]
    end

    subgraph Dispositivos ["Plugins Didáticos Carregados (.dll / .so)"]
        DEV1["Porta 0x0010: Semáforo de Trânsito"]
        DEV2["Porta 0x0020: Mostrador 7 Segmentos"]
        DEV3["Porta 0x0030: Painel de Chaves e LEDs"]
    end

    EXEC <--> BUS
    BUS <--> DEV1
    BUS <--> DEV2
    BUS <--> DEV3
```

---

## 2. Instruções de Barramento no Código Intermediário

A comunicação entre a máquina virtual e os periféricos realiza-se por meio de duas instruções atômicas:

### 2.1 Instrução de Saída (`out`)
Escreve o valor armazenado em um registrador de 64 bits em uma porta de 16 bits do barramento:
```text
out 0x0010, r0;   ; Envia o conteúdo de r0 para o dispositivo na porta 0x0010
```

### 2.2 Instrução de Entrada (`in`)
Lê o estado lógico atual da porta indicada e armazena o valor recebido no registrador de destino:
```text
in r1, 0x0030;    ; Lê o valor dos sensores na porta 0x0030 para o registrador r1
```

---

## 3. Especificação da API de Plugins em C (`ijkl_device_api.h`)

Para viabilizar a criação de periféricos personalizados por estudantes e pesquisadores, o sistema disponibiliza uma interface formal em C padrão (`include/ijkl_device_api.h`):

```c
typedef struct IjklDeviceInterface {
    const char* (*get_device_name)(void);
    uint16_t    (*get_base_port)(void);
    uint16_t    (*get_port_count)(void);
    bool        (*initialize)(void);
    int64_t     (*read_port)(uint16_t port_address);
    void        (*write_port)(uint16_t port_address, int64_t value);
    void        (*step_cycle)(void);
    void        (*shutdown)(void);
} IjklDeviceInterface;
```

### Ciclo de Vida do Plugin:
1. **Carregamento Dinâmico**: A máquina virtual carrega a biblioteca compartilhada (`.dll` no Windows ou `.so` no Linux) e localiza o símbolo de exportação `ijkl_plugin_register`.
2. **Inicialização**: Executa `initialize()`, alocando recursos de hardware simulado e registrando o intervalo de portas atribuído ao dispositivo.
3. **Ciclo Operacional**: A cada instrução `in` ou `out` direcionada ao intervalo do periférico, os métodos `read_port()` ou `write_port()` são acionados de maneira síncrona.
4. **Sincronismo de Clock (`step_cycle`)**: Permite a simulação de temporizadores, atrasos físicos de transição e decaimento de sinais.
5. **Encerramento (`shutdown`)**: Libera janelas gráficas, arquivos de log ou conexões associadas ao término da execução da máquina virtual.

---

## 4. Exemplos de Dispositivos Didáticos

### 4.1 Semáforo de Trânsito Educacional (Porta `0x0010`)
Controla o acionamento de lâmpadas de tráfego a partir de uma máscara de bits:
- Bit 0: LED Vermelho (ligado/desligado).
- Bit 1: LED Amarelo (ligado/desligado).
- Bit 2: LED Verde (ligado/desligado).

Um programa em ijkLang pode alternar as fases de um cruzamento através de escritas sequenciais com atrasos calculados:
```text
programa ControleSemaforo {
    privado principal(): inteiro {
        escreva_porta(0x0010, 4); // Bit 2 ativo: Luz Verde
        // Aguarda temporização...
        escreva_porta(0x0010, 2); // Bit 1 ativo: Luz Amarela
        // Aguarda temporização...
        escreva_porta(0x0010, 1); // Bit 0 ativo: Luz Vermelha
        retorne 0;
    }
}
```

### 4.2 Mostrador de 7 Segmentos Multiplexado (Porta `0x0020`)
Recebe dígitos decimais codificados (ou padrões de catodo) e renderiza na interface textual ou gráfica a representação dos mostradores numéricos.

### 4.3 Sensores e Chaves de Entrada (Porta `0x0030`)
Permite ao estudante ler o estado de chaves liga/desliga simuladas pelo usuário no teclado, condicionando laços de controle industrial e atuadores virtuais.


## Teclado sem bloqueio

`ijkL.IO.Console.hasKey(): bool` consulta se existe um byte de entrada sem
consumi-lo logicamente. `readKeyAsync(): char` consome esse byte ou retorna zero
se não houver entrada. EOF não é tecla. Para distinguir um byte NUL de ausência
de dados, consulte `hasKey()` antes da leitura. As funções trabalham com bytes,
como a porta de caracteres existente; não decodificam sequências UTF-8 ou teclas
especiais do terminal. ESC isolado corresponde ao byte 27.

O modo padrão emite as rotinas em CI: a porta `0x0005` consulta disponibilidade
e a porta `0x0006` faz uma leitura assíncrona, evitando uma leitura bloqueante
após a sondagem. Com `--native-builtins`, o compilador emite chamadas para
`__builtin_console_has_key` e `__builtin_console_read_key_async`, despachadas
em C pela VM usando o mesmo backend de E/S. A instrução `in` retorna uma palavra
inteira; o despacho nativo preserva essa representação dinâmica, mantendo as
assinaturas públicas bool/char e o comportamento idêntico nos dois modos.

No POSIX, `poll` consulta stdin; terminais são colocados temporariamente em modo
não canônico e sem eco, restaurando os atributos antes de cada retorno. Pipes e
arquivos redirecionados também são aceitos. No Windows, `_kbhit`/`_getch` atendem
ao console e `PeekNamedPipe` consulta pipes redirecionados. A VM desativa o buffer
de stdin ao registrar o console, para a sondagem não ignorar bytes antecipados
por stdio. Um byte antecipado fica na máquina e é devolvido a stdin se a próxima
operação for uma leitura bloqueante. O console pressupõe um único consumidor de
stdin, como o restante da VM.

Os aliases `ijkL.ES.Console.temTecla()` e `leiaTeclaAssincrona()` usam o mecanismo
existente de `keywords/pt-BR.words`, sem duplicar a unidade física em `lib/ijkL/ES`.

Validação: `make test-console` ou `python3 tests/run_console_tests.py` no Ubuntu;
no Windows, `powershell -ExecutionPolicy Bypass -File tests/run_console_tests.ps1`.
As suítes gerais incluem esses testes. Os testes POSIX usam um pseudo-terminal
para enviar ESC sem Enter e verificar a restauração de atributos; no Windows,
validar também `tests/console/poll.ijk` em console real e pressionar ESC sem Enter.
O runner Ubuntu usa Python 3 (somente biblioteca padrão).
