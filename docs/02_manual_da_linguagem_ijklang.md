# Manual Prático da Linguagem ijkLang

A linguagem **ijkLang (`ijkL`)** foi concebida para oferecer uma sintaxe intuitiva, fortemente tipada e estruturada, servindo como ferramenta didática para o aprendizado de algoritmos, tipos de dados e controle de fluxo.

Um dos diferenciais didáticos da linguagem é o suporte multilíngue por meio de dicionários de palavras-chave (`pt-BR.words` e `en-US.words`), permitindo que os programas sejam redigidos tanto em português quanto em inglês sem necessidade de alteração do compilador. Todos os blocos estruturais são delimitados por chaves `{` e `}`.

---

## 1. Estrutura Geral de um Programa

Todo arquivo executável em ijkLang declara um programa por meio da palavra-chave `programa` (ou `program`), delimitado por chaves `{` e `}`. O ponto de entrada da aplicação é a função estritamente privada `privado principal(): inteiro` (ou `private main(): int` em inglês), que pode opcionalmente receber a contagem e o vetor de argumentos do ambiente e devolve um código de saída inteiro.

O corpo de um `programa` ou de uma `unidade` aceita **exclusivamente declarações puras** (constantes, tipos, variáveis de módulo com inicializadores constantes e rotinas). Instruções executáveis avulsas diretamente no corpo do namespace são expressamente proibidas e geram erro sintático; todo código executável deve residir no interior de funções ou procedimentos.

### Exemplo canônico em português (`pt-BR`):
```text
programa OlaMundo {
    privado principal(): inteiro {
        escrevaln("Olá, mundo do ijkLang!");
        retorne 0;
    }
}
```

### Exemplo canônico equivalente em inglês (`en-US`):
```text
program HelloWorld {
    private main(): int {
        writeln("Hello, world of ijkLang!");
        return 0;
    }
}
```

---

## 2. Tipos de Dados Fundamentais

A linguagem possui tipos primitivos de dados com representação determinística:

| Tipo (`pt-BR`) | Tipo (`en-US`) | Descrição Técnica | Exemplos de Literais |
| :--- | :--- | :--- | :--- |
| `inteiro` | `int` / `integer` | Número inteiro com sinal de 64 bits | `42`, `-10`, `0x2A` |
| `real` | `real` | Decimal exato de base 10 (cálculos financeiros) | `3.14159`, `-0.5` |
| `flutuante` | `float` | Ponto flutuante binário IEEE 754 de 32 bits | `9.81f`, `-0.01f` |
| `caractere` | `char` | Caractere Unicode UTF-8 | `'A'`, `'ç'`, `'\n'` |
| `texto` | `string` | Cadeia sequencial de caracteres | `"Algoritmos"`, `"ijkL"` |
| `logico` | `bool` / `boolean` | Valor booleano de verdade ou falsidade | `verdadeiro`, `falso` / `true`, `false` |

---

## 3. Declaração de Constantes e Variáveis

### 3.1 A Regra de Ouro da Sintaxe de Identificadores e Tipos
Toda declaração no ecossistema ijkLang segue a ordem natural e uniforme:
`<tipo> <identificador> [= <valor_inicial>];`

- **Variáveis:** `<tipo> <identificador> [= <valor>];` (ex.: `inteiro contador = 0;`, `texto mensagem = "Olá";`)
- **Constantes:** `constante <tipo> <identificador> = <valor>;` (ou com tipo inferido: `constante <identificador> = <valor>;`)
- **Parâmetros de Rotinas:** `<tipo> <identificador>` (ex.: `quadrado(inteiro valor): inteiro`, `concatenar(texto s1, texto s2): texto`)
- **Uso Exclusivo do Delimitador `:`:** O símbolo `:` é reservado **estrita e exclusivamente para especificar o tipo de retorno de funções**, espelhando a notação matemática $f(x) : \mathbb{R}$. Procedimentos não utilizam `:` nem tipo de retorno.

### 3.2 Semântica de Mutabilidade: Tipos Primitivos vs. Estruturados
- **Primitivos (Valores) — Imutáveis:** `texto`, `inteiro`, `real`, `flutuante`, `caractere`, `logico` possuem semântica de valor. Funções nunca alteram o operando de entrada; operações geram e retornam um **novo valor independente** (ex.: `texto novo = Texto.alterar(str, 1, 'a');`).
- **Estruturados (Entidades) — Mutáveis:** `Lista` (`struct`), registros e tipos de usuário possuem semântica de referência via ponteiro (`ponteiro`), permitindo mutações *in-place* no estado interno (ex.: `Colecoes.adicionar(lista, item);`).

```text
programa Declaracoes {
    privado principal(): inteiro {
        constante real PI = 3.141592;
        constante inteiro LIMITE = 100;

        inteiro idade = 18;
        real taxa = 12.5;
        logico ativo = verdadeiro;
        caractere letra = 'A';
        texto nome = "Estudante";

        escreva("Idade inicial: ");
        escrevaln(idade);
        retorne 0;
    }
}
```

---

## 4. Operadores e Expressões

### 4.1 Operadores Aritméticos
- Adição: `+`
- Subtração: `-`
- Multiplicação: `*`
- Divisão: `/`
- Resto da Divisão Inteira (Módulo): `%` ou `mod`
- Potenciação: `^`

### 4.2 Operadores Relacionais
- Igualdade: `==`
- Diferença: `!=` ou `<>`
- Menor que: `<`
- Menor ou igual: `<=`
- Maior que: `>`
- Maior ou igual: `>=`

### 4.3 Operadores Lógicos
- Conjunção (*E* lógico): `e` (ou `and`, `&&`)
- Disjunção (*OU* lógico): `ou` (ou `or`, `||`)
- Negação (*NÃO* lógico): `nao` (ou `not`, `!`)

### 4.4 Operador de Custo de Memória
- Custo em bytes: `$tipo` ou `$variavel` (retorna o tamanho em memória de 64 bits).

---

## 5. Estruturas de Controle de Fluxo

### 5.1 Estrutura Condicional (`se / senao`)
Permite a bifurcação de execução baseada em uma expressão relacional ou lógica.

```text
se (idade >= 18) {
    escrevaln("Maior de idade.");
} senao {
    escrevaln("Menor de idade.");
}
```

Na sintaxe em inglês:
```text
if (age >= 18) {
    writeln("Adult.");
} else {
    writeln("Minor.");
}
```

### 5.2 Estrutura de Escolha Múltipla (`escolha / caso`)
Avalia uma expressão discreta e seleciona o ramo correspondente.

```text
escolha (opcao) {
    caso 1:
        escrevaln("Opção de Cadastro.");
    caso 2:
        escrevaln("Opção de Consulta.");
    padrao:
        escrevaln("Opção inválida.");
}
```

---

## 6. Estruturas de Repetição (Laços)

O ijkLang oferece três formas clássicas de laços de repetição, todas delimitadas por `{` e `}`:

### 6.1 Laço Pré-Testado (`enquanto`)
Executa o bloco enquanto a condição permanecer verdadeira.

```text
inteiro i = 0;
enquanto (i < 5) {
    escrevaln(i);
    i = i + 1;
}
```

### 6.2 Laço com Variável de Controle (`para`)
Executa um número determinado de iterações, incrementando automaticamente a variável de controle:

```text
para (inteiro i = 1 ate 10 passo 1) {
    escrevaln(i);
}
```

Na sintaxe em inglês:
```text
for (int i = 1 to 10 step 1) {
    writeln(i);
}
```

### 6.3 Laço Pós-Testado (`repita / ate`) com Aborto (`aborte`)
Executa o bloco pelo menos uma vez, interrompendo quando a condição for satisfeita. Permite o comando `aborte` (`break`) para saída prematura:

```text
inteiro contador = 0;
repita {
    contador = contador + 1;
    se (contador == 3) {
        aborte;
    }
} ate (contador >= 10);
```

---

## 7. Modularização: Funções, Procedimentos e Encapsulamento

Em ijkLang, rotinas com especificação de tipo de retorno (`: Tipo`) são funções; rotinas sem especificação de retorno são procedimentos. A linguagem não utiliza palavras-chave redundantes como `funcao` ou `procedimento`.

### 7.1 Regras de Visibilidade e Interface de Unidades e Programas
Para promover as melhores práticas de engenharia de software e ocultamento de informação (*information hiding*), as unidades modulares e executáveis seguem regras estritas de visibilidade e integridade:

1. **(Regra 0) Constantes e Tipos são Públicos por Padrão**:
   - Constantes (`constante` / `const`) e tipos definidos pelo usuário (`tipo` / `type`) integram a interface pública da unidade.
   - São acessíveis diretamente por qualquer unidade importadora sem necessidade (nem permissão) de palavras-chave adicionais como `publico` ou `privado`.

2. **(Regra 1) Variáveis de Módulo são Privadas por Padrão**:
   - Todas as variáveis declaradas no escopo de uma unidade são estritamente privadas, protegendo o estado interno contra mutações externas indevidas.
   - Não exigem nem aceitam palavras-chave `publico` ou `privado`. O único meio de acesso ou mutação a partir de outra unidade é por meio de rotinas públicas que encapsulam a leitura e escrita.

3. **(Regra 2) Rotinas Exigem Modificador Explícito (`publico` ou `privado`)**:
   - Toda função e procedimento declarado dentro de uma `unidade` (`unit`) deve ser precedido obrigatoriamente por `publico` (`public`) ou `privado` (`privado` / `private` / `prive`), determinando se a rotina integra a API pública do módulo ou se é um detalhe de implementação interno.
   - **Exclusividade Privada em Programas (`program`)**: Em unidades do tipo `programa` (`program`), **todas as rotinas devem ser obrigatoriamente `privado` (`private`)**. O compilador emite um **erro semântico** se qualquer rotina (incluindo `principal` ou funções auxiliares) for declarada como `publico` (`public`). A justificativa conceitual e pedagógica é direta: programas são pontos de entrada executáveis autônomos que **não podem ser importados** por nenhuma outra unidade; permitir rotinas públicas em um `programa` seria inútil, incoerente com a arquitetura de módulos e geraria a falsa ilusão de que a rotina poderia ser reutilizada externamente.
   - O método de inicialização da aplicação (`principal` ou `main`) em um `programa` deve ser **obrigatoriamente `privado` (`private`)**. O compilador rejeita com erro semântico qualquer declaração de `principal` como `publico`.
   - **Restrição de Importação de Programas**: Unidades do tipo `programa` (`program`) não podem ser importadas por outras unidades. Apenas bibliotecas do tipo `unidade` (`unit`) podem ser importadas via `importe`.
   - **Restrição de Ponto de Entrada**: Unidades do tipo `unidade` (`unit`) não podem conter rotina com o identificador de ponto de entrada (`principal` ou `main`).

4. **(Regra 3) Corpo Puro de Declarações no Escopo de Unidade e Programa**:
   - É expressamente proibida a presença de instruções imperativas soltas (como atribuições fora de declarações, condicionais, laços ou chamadas avulsas a procedimentos) diretamente no corpo de `unidade` ou `programa`.
   - O corpo do namespace aceita **exclusivamente**:
     - Declarações de constantes (`constante` / `const`);
     - Definições de novos tipos (`tipo` / `type`);
     - Declarações de variáveis de módulo com inicializadores que sejam estritamente expressões constantes;
     - Declarações de rotinas (funções e procedimentos).
   - Qualquer comando executável deve residir obrigatoriamente dentro do corpo de uma rotina.

5. **(Regra 4) Procedimentos Reservados de Ciclo de Vida (`inicializar` e `finalizar`)**:
   - O compilador e a máquina virtual fornecem suporte nativo a dois métodos especiais para gerenciamento de recursos e ciclo de vida: `inicializar` (`initialize`) e `finalizar` (`finalize`).
   - **Assinatura e Visibilidade**:
     - Devem ser procedimentos sem retorno (não podem ser funções) e sem parâmetros (`()`).
     - Devem ser declarados obrigatoriamente com o modificador `privado` (`private`).
     - Cada unidade ou programa pode declarar no máximo um procedimento de inicialização e um de finalização.
   - **Ordem de Execução no Bootstrap**:
     1. As variáveis estáticas de todos os módulos importados e do programa são alocadas e inicializadas com seus literais e constantes no segmento global.
     2. Os procedimentos `inicializar()` dos módulos importados são executados na primeira vez em que a unidade for carregada (garantia de idempotência).
     3. O procedimento `inicializar()` do programa principal é executado (se definido).
     4. A função principal `privado principal(): inteiro` é chamada.
     5. Ao término bem-sucedido de `principal`, o procedimento `finalizar()` do programa é disparado.
     6. Os procedimentos `finalizar()` dos módulos importados são executados na **ordem inversa** de sua inicialização para liberação graciosa e ordenada de recursos.
   - **Semântica Fail-Fast**: Caso ocorra uma falha em tempo de execução (*runtime error*), a execução da máquina virtual é imediatamente interrompida sem acionar os procedimentos de finalização, preservando a integridade do estado da máquina para fins de diagnóstico e depuração.

### 7.2 Exemplo de Encapsulamento e Ciclo de Vida em Unidade
```text
unidade Geometria.Retangulo {
    // Regra 0: Constante pública na interface
    constante real FATOR_ESCALA = 1.0;

    // Regra 1 e Regra 3: Variável privada com inicializador constante
    inteiro total_calculos = 0;

    // Regra 4: Procedimento de inicialização do módulo
    privado inicializar() {
        total_calculos = 0;
    }

    // Regra 4: Procedimento de finalização do módulo
    privado finalizar() {
        // Encerramento ordenado de recursos
    }

    // Regra 2: Procedimento auxiliar estritamente interno
    privado registrarUso() {
        total_calculos = total_calculos + 1;
    }

    // Regra 2: Funções públicas expostas para unidades importadoras
    publico calcularArea(real base, real altura): real {
        registrarUso();
        retorne base * altura * FATOR_ESCALA;
    }

    publico obterTotalCalculos(): inteiro {
        retorne total_calculos;
    }
}
```

### 7.3 Procedimento com Passagem por Referência (`ref` ou `&`)
```text
privado dobrarValor(ref inteiro numero) {
    numero = numero * 2;
}
```

### 7.4 Função com Retorno de Valor
Funções com tipo de retorno exigem obrigatoriamente o comando `retorne`:
```text
publico calcularMedia(real nota1, real nota2): real {
    retorne (nota1 + nota2) / 2.0;
}
```

---

## 8. Estruturas de Dados Compostas

### 8.1 Arranjos (Vetores)
Coleções sequenciais e homogêneas de elementos indexados numericamente a partir de zero:
```text
arranjo[5] de inteiro numeros;
numeros[0] = 10;
numeros[1] = 20;

para (inteiro k = 0 ate 4 passo 1) {
    escrevaln("Elemento [", k, "] = ", numeros[k]);
}
```

### 8.2 Registros (`structs`)
Tipos de dados heterogêneos definidos pelo usuário que agrupam múltiplos campos:
```text
tipo Ponto = registro {
    real x;
    real y;
};

Ponto origem;
origem.x = 0.0;
origem.y = 0.0;
```

### 8.3 Apontadores e Manipulação de Endereços
Para o estudo prático de alocação de memória e referências indiretas, a linguagem oferece tipos apontadores (`ponteiro`), operador de endereço (`&`) e operador de desreferenciação (`*` ou `@`):
```text
inteiro valor = 42;
ponteiro de inteiro ptr = &valor;
*ptr = 100;
escrevaln(valor); // Imprime 100
```

---

## 9. Primitivas de Entrada e Saída

A interação básica com o usuário no terminal dá-se através de três comandos nativos:
- `leia(identificador)`: Lê um valor do teclado e armazena na variável.
- `escreva(expressao1, expressao2, ...)`: Emite os valores no console sem quebra de linha.
- `escrevaln(expressao1, expressao2, ...)`: Emite os valores no console e posiciona o cursor na linha seguinte.xpressão no console sem quebra de linha.
- `escrevaln(expressao)`: Emite o valor da expressão no console e posiciona o cursor na linha seguinte.
