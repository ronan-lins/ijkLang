# Apêndice B: Gramática Sintática Formal (EBNF)

Este apêndice apresenta a especificação sintática formal e completa da linguagem **ijkLang** na notação de **Backus-Naur Estendida (EBNF)** (ISO/IEC 14977).

---

## 1. Estrutura Global do Programa e Módulos

```ebnf
UnidadeCompilacao       = { DeclaracaoImportacao } , ( DeclaracaoPrograma | DeclaracaoUnidade ) ;

DeclaracaoImportacao    = ( 'importe' | 'import' ) , NomeQualificado , ';' ;

NomeQualificado         = Identificador , { '.' , Identificador } ;

DeclaracaoPrograma      = ( 'programa' | 'program' ) , Identificador , '{' ,
                          { DeclaracaoItem } ,
                          '}' ;

DeclaracaoUnidade       = ( 'unidade' | 'unit' ) , NomeQualificado , '{' ,
                          { DeclaracaoItem } ,
                          '}' ;

DeclaracaoItem          = DeclaracaoConstante
                        | DefinicaoTipo
                        | DeclaracaoVariavel
                        | DeclaracaoRotina ;
```

---

## 2. Seções de Declarações

### 2.1 Declaração de Constantes
```ebnf
DeclaracaoConstante     = ( 'constante' | 'const' ) , [ EspecificacaoTipo ] , Identificador , '=' , ExpressaoConstante , ';' ;
ExpressaoConstante      = LiteralInteiro | LiteralReal | LiteralFlutuante | LiteralCaractere | LiteralTexto | LiteralBooleano ;
```

### 2.2 Declaração de Tipos Personalizados
```ebnf
DefinicaoTipo           = ( 'tipo' | 'type' ) , Identificador , '=' , EspecificacaoTipoCompleta , ';' ;

EspecificacaoTipoCompleta = EspecificacaoTipoPrimitivo
                          | TipoArranjo
                          | TipoRegistro
                          | TipoPonteiro ;

TipoArranjo             = ( 'arranjo' | 'array' ) , '[' , [ LiteralInteiro ] , ']' , [ ( 'de' | 'of' ) ] , EspecificacaoTipo ;
TipoRegistro            = ( 'registro' | 'struct' ) , '{' , { CampoRegistro } , '}' ;
CampoRegistro           = EspecificacaoTipo , Identificador , ';' ;
TipoPonteiro            = ( 'ponteiro' | 'pointer' ) , [ ( 'de' | 'of' ) ] , EspecificacaoTipo ;
```

### 2.3 Declaração de Variáveis
```ebnf
DeclaracaoVariavel      = EspecificacaoTipo , Identificador , [ '=' , Expressao ] , ';' ;

EspecificacaoTipoPrimitivo = 'inteiro'   | 'int'   | 'integer'
                           | 'real'      | 'real'
                           | 'flutuante' | 'float'
                           | 'caractere' | 'char'
                           | 'texto'     | 'string'
                           | 'logico'    | 'bool'  | 'boolean' ;

EspecificacaoTipo       = EspecificacaoTipoPrimitivo
                        | Identificador
                        | TipoArranjo
                        | TipoPonteiro ;
```

---

## 3. Declaração de Rotinas (Funções e Procedimentos) e Regras de Visibilidade

Em ijkLang, rotinas com especificação de retorno (`: Tipo`) são funções; rotinas sem especificação de retorno são procedimentos. O delimitador `:` é reservado **estrita e exclusivamente** para o tipo de retorno de funções (espelhando a notação matemática $f(x) : \mathbb{R}$). Todos os parâmetros seguem uniformemente a ordem `<tipo> <identificador>`.

### 3.1 Regras de Visibilidade e Interface
- **(Regra 0) Constantes e Tipos**: Públicos por padrão, integram a interface pública da unidade e não usam `publico` ou `privado`.
- **(Regra 1) Variáveis de Unidade**: Privadas por padrão, garantem o encapsulamento estrito do estado interno e não usam `publico` ou `privado`.
- **(Regra 2) Rotinas em Unidades e Programas**: É **mandatório** classificar funções e procedimentos com `publico` ou `privado` (`public` ou `private`). Em módulos de biblioteca (`unidade` / `unit`), isso controla expressamente a exposição da operação a quem importa a unidade. Em unidades executáveis (`programa` / `program`), **todas as rotinas (incluindo `principal` ou `main` e rotinas auxiliares) devem ser obrigatoriamente `privado` (`private`)**; qualquer declaração de `publico` em um programa é rejeitada com erro semântico. Unidades do tipo `programa` não podem ser importadas, e unidades `unidade` não podem conter `principal`/`main`.

```ebnf
DeclaracaoRotina        = ModificadorVisibilidade , Identificador , '(' , [ ListaParametros ] , ')' ,
                          [ ':' , EspecificacaoTipo ] ,
                          InstrucaoBloco ;

ModificadorVisibilidade = 'publico' | 'privado' | 'public' | 'private' | 'prive' ;

ListaParametros         = Parametro , { ',' , Parametro } ;
Parametro               = [ 'ref' | '&' ] , EspecificacaoTipo , Identificador ;
```

### 3.2 Ponto de Entrada Obrigatório da Aplicação
- Em unidades executáveis (`programa` / `program`), é obrigatória a existência de uma função de entrada:
  `privado principal(): inteiro { ... retorne 0; }` (ou `private main(): int { ... return 0; }`).
- A função de entrada deve ser declarada como `privado` (`private`), retornar `inteiro` (`int`), e não aceitar parâmetros ou aceitar exatamente 2 parâmetros: `(inteiro quantidade, arranjo[] de texto args)` (ou em inglês: `(int argc, string[] argv)`).

### 3.3 Procedimentos Reservados de Ciclo de Vida
- Tanto em módulos (`unidade` / `unit`) quanto em programas (`programa` / `program`), podem ser definidos procedimentos opcionais de ciclo de vida:
  - `privado inicializar()` (`private initialize()` / `privé initialiser()`): invocado na carga/importação da unidade (executado exatamente uma vez na primeira importação).
  - `privado finalizar()` (`private finalize()` / `privé finaliser()`): invocado na finalização da aplicação.
- Devem ser declarados obrigatoriamente com visibilidade `privado` (`private`), sem retorno (`procedimento`), e sem parâmetros.
- Na sequência de bootstrap, os inicializadores dos módulos importados são executados antes do inicializador do programa; o método `principal()` é então invocado; ao término com sucesso, o finalizador do programa executa primeiro, seguido pelos finalizadores dos módulos importados na ordem inversa. Em caso de interrupção ou falha em tempo de execução, a execução é interrompida imediatamente sem acionar os finalizadores (semântica fail-fast).

---

## 4. Instruções e Controle de Fluxo

```ebnf
ListaInstrucoes         = { Instrucao } ;

Instrucao               = InstrucaoAtribuicao
                        | InstrucaoChamadaRotina
                        | InstrucaoSe
                        | InstrucaoEscolha
                        | InstrucaoEnquanto
                        | InstrucaoPara
                        | InstrucaoRepita
                        | InstrucaoAborte
                        | InstrucaoRetorne
                        | InstrucaoEntradaSaida
                        | DeclaracaoVariavel
                        | InstrucaoBloco ;

InstrucaoBloco          = '{' , ListaInstrucoes , '}' ;
```

### 4.1 Atribuição e Chamada
```ebnf
InstrucaoAtribuicao     = ElementoLValue , ( '=' | ':=' ) , Expressao , ';' ;
ElementoLValue          = [ '*' | '@' ] , Identificador , { AcessoIndice | AcessoCampo } ;
AcessoIndice            = '[' , Expressao , ']' ;
AcessoCampo             = '.' , Identificador ;

InstrucaoChamadaRotina  = NomeQualificado , '(' , [ ListaArgumentos ] , ')' , ';' ;
ListaArgumentos         = Expressao , { ',' , Expressao } ;
```

### 4.2 Instruções Condicionais
```ebnf
InstrucaoSe             = ( 'se' | 'if' ) , '(' , Expressao , ')' ,
                          ( InstrucaoBloco | Instrucao ) ,
                          [ ( 'senao' | 'else' ) , ( InstrucaoBloco | Instrucao ) ] ;

InstrucaoEscolha        = ( 'escolha' | 'switch' ) , '(' , Expressao , ')' ,
                          '{' , { RamoCaso } , [ RamoPadrao ] , '}' ;

RamoCaso                = ( 'caso' | 'case' ) , ExpressaoConstante , ':' , ListaInstrucoes ;
RamoPadrao              = ( 'padrao' | 'default' ) , ':' , ListaInstrucoes ;
```

### 4.3 Instruções de Repetição
```ebnf
InstrucaoEnquanto       = ( 'enquanto' | 'while' ) , '(' , Expressao , ')' ,
                          ( InstrucaoBloco | Instrucao ) ;

InstrucaoPara           = ( 'para' | 'for' ) , '(' ,
                          [ EspecificacaoTipo ] , Identificador , '=' , Expressao ,
                          ( 'ate' | 'to' ) , Expressao ,
                          [ ( 'passo' | 'step' ) , Expressao ] , ')' ,
                          ( InstrucaoBloco | Instrucao ) ;

InstrucaoRepita         = ( 'repita' | 'repeat' ) ,
                          InstrucaoBloco ,
                          ( 'ate' | 'until' ) , '(' , Expressao , ')' , ';' ;

InstrucaoAborte         = ( 'aborte' | 'break' ) , ';' ;
InstrucaoRetorne        = ( 'retorne' | 'return' ) , [ Expressao ] , ';' ;
```

### 4.4 Primitivas de Entrada e Saída
```ebnf
InstrucaoEntradaSaida   = InstrucaoLeia
                        | InstrucaoEscreva
                        | InstrucaoEscrevaLn ;

InstrucaoLeia           = ( 'leia' | 'read' ) , '(' , ElementoLValue , ')' , ';' ;
InstrucaoEscreva        = ( 'escreva' | 'write' ) , '(' , ListaArgumentos , ')' , ';' ;
InstrucaoEscrevaLn      = ( 'escrevaln' | 'writeln' ) , '(' , [ ListaArgumentos ] , ')' , ';' ;
```

---

## 5. Hierarquia de Expressões e Precedência de Operadores

A gramática de expressões organiza-se em camadas descendentes que garantem a precedência clássica de operadores matemáticos e lógicos sem ambiguidades:

```ebnf
Expressao               = ExpressaoLogicaOu ;

ExpressaoLogicaOu       = ExpressaoLogicaE , { ( 'ou' | 'or' ) , ExpressaoLogicaE } ;
ExpressaoLogicaE        = ExpressaoRelacional , { ( 'e' | 'and' ) , ExpressaoRelacional } ;

ExpressaoRelacional     = ExpressaoAditiva , [ OpRelacional , ExpressaoAditiva ] ;
OpRelacional            = '==' | '!=' | '<' | '<=' | '>' | '>=' ;

ExpressaoAditiva        = ExpressaoMultiplicativa , { ( '+' | '-' ) , ExpressaoMultiplicativa } ;
ExpressaoMultiplicativa = ExpressaoUnaria , { ( '*' | '/' | '%' ) , ExpressaoUnaria } ;

ExpressaoUnaria         = [ '+' | '-' | 'nao' | 'not' | '&' | '*' ] , FatorPrimario ;

FatorPrimario           = LiteralInteiro
                        | LiteralReal
                        | LiteralCaractere
                        | LiteralTexto
                        | LiteralBooleano
                        | ChamadaFuncaoOuVariavel
                        | '(' , Expressao , ')' ;

ChamadaFuncaoOuVariavel = Identificador , [ '(' , [ ListaArgumentos ] , ')' ] , { AcessoIndice | AcessoCampo } ;
```
