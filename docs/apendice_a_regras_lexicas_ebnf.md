# Apêndice A: Regras Léxicas Formais (EBNF)

Este apêndice especifica formalmente as regras léxicas da linguagem **ijkLang** utilizando a notação de **Backus-Naur Estendida (EBNF)** conforme a norma ISO/IEC 14977.

---

## 1. Convenções de Notação EBNF Utilizadas

- `definicao = elemento ;` : Regra de produção gramatical.
- `A , B` : Concatenação sequencial.
- `A | B` : Alternativa (escolha disjuntiva).
- `[ A ]` : Opcionalidade (zero ou uma ocorrência).
- `{ A }` : Repetição (zero ou mais ocorrências).
- `'literal'` ou `"literal"` : Símbolo terminal explícito.
- `( ... )` : Agrupamento de elementos.
- `A - B` : Exceção sintática (A exceto B).

---

## 2. Conjunto de Caracteres e Elementos Básicos

```ebnf
CaractereASCII          = ? Qualquer código Unicode entre U+0020 e U+007E ? ;
CaractereDiacritico     = ? Codepoints Unicode estendidos para língua portuguesa
                            (á, à, â, ã, é, ê, í, ó, ô, õ, ú, ü, ç,
                             Á, À, Â, Ã, É, Ê, Í, Ó, Ô, Õ, Ú, Ü, Ç) ? ;
CaractereUnicode        = CaractereASCII | CaractereDiacritico ;

LetraMaiuscula          = 'A' | 'B' | 'C' | 'D' | 'E' | 'F' | 'G' | 'H' | 'I'
                        | 'J' | 'K' | 'L' | 'M' | 'N' | 'O' | 'P' | 'Q' | 'R'
                        | 'S' | 'T' | 'U' | 'V' | 'W' | 'X' | 'Y' | 'Z' ;

LetraMinuscula          = 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i'
                        | 'j' | 'k' | 'l' | 'm' | 'n' | 'o' | 'p' | 'q' | 'r'
                        | 's' | 't' | 'u' | 'v' | 'w' | 'x' | 'y' | 'z' ;

Letra                   = LetraMaiuscula | LetraMinuscula | CaractereDiacritico ;
DigitoDecimal           = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' ;
DigitoHexadecimal       = DigitoDecimal
                        | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'
                        | 'a' | 'b' | 'c' | 'd' | 'e' | 'f' ;

Sublinhado              = '_' ;
```

---

## 3. Espaços em Branco e Comentários

Os espaços em branco e comentários atuam como separadores léxicos e são descartados na formação dos tokens sintáticos:

```ebnf
EspacoEmBranco          = ' ' | '\t' | '\r' | '\n' ;

ComentarioLinha         = '//' , { ? Qualquer caractere exceto quebra de linha ? } , ( '\n' | ? FimDeArquivo ? ) ;
ComentarioBloco         = '/*' , { ? Qualquer caractere ? - '*/' } , '*/' ;

Separador               = EspacoEmBranco | ComentarioLinha | ComentarioBloco ;
```

---

## 4. Identificadores

Um identificador inicia-se obrigatoriamente por uma letra ou pelo caractere sublinhado, seguido por zero ou mais letras, dígitos ou sublinhados:

```ebnf
Identificador           = ( Letra | Sublinhado ) , { Letra | DigitoDecimal | Sublinhado } ;
```

---

## 5. Literais Numéricos e Constantes

### 5.1 Literais Inteiros
Suportam representação decimal com ou sem sinal, e representação hexadecimal com prefixo `0x` ou `0X`:

```ebnf
LiteralInteiroDecimal   = [ '+' | '-' ] , DigitoDecimal , { DigitoDecimal } ;
LiteralInteiroHexa      = ( '0x' | '0X' ) , DigitoHexadecimal , { DigitoHexadecimal } ;
LiteralInteiro          = LiteralInteiroHexa | LiteralInteiroDecimal ;
```

### 5.2 Literais em Ponto Flutuante (Reais)
Números reais com parte inteira, ponto fracionário e expoente opcional (notação científica):

```ebnf
ParteExpoente           = ( 'e' | 'E' ) , [ '+' | '-' ] , DigitoDecimal , { DigitoDecimal } ;
LiteralReal             = [ '+' | '-' ] , DigitoDecimal , { DigitoDecimal } , '.' , DigitoDecimal , { DigitoDecimal } , [ ParteExpoente ] ;
```

### 5.3 Literais de Caractere e Cadeias de Texto
Cadeias de caracteres delimitadas por aspas duplas e caracteres individuais delimitados por aspas simples, aceitando sequências de escape padrão:

```ebnf
SequenciaDeEscape       = '\\' , ( 'n' | 't' | 'r' | '\\' | '\'' | '\"' | '0' ) ;

ElementoCaractere       = ( CaractereUnicode - ( '\'' | '\\' ) ) | SequenciaDeEscape ;
LiteralCaractere        = '\'' , ElementoCaractere , '\'' ;

ElementoTexto           = ( CaractereUnicode - ( '\"' | '\\' ) ) | SequenciaDeEscape ;
LiteralTexto            = '\"' , { ElementoTexto } , '\"' ;
```

### 5.4 Literais Booleanos (Lógicos)
```ebnf
LiteralBooleanoPT       = 'verdadeiro' | 'falso' ;
LiteralBooleanoEN       = 'true' | 'false' ;
LiteralBooleano         = LiteralBooleanoPT | LiteralBooleanoEN ;
```

---

## 6. Operadores e Delimitadores

### 6.1 Operadores Aritméticos
```ebnf
OpAdicao                = '+' ;
OpSubtracao             = '-' ;
OpMultiplicacao         = '*' ;
OpDivisao               = '/' ;
OpModulo                = '%' ;
```

### 6.2 Operadores Relacionais
```ebnf
OpIgual                 = '==' ;
OpDiferente             = '!=' ;
OpMenor                 = '<' ;
OpMenorIgual            = '<=' ;
OpMaior                 = '>' ;
OpMaiorIgual            = '>=' ;
```

### 6.3 Operadores de Atribuição e Ponteiros
```ebnf
OpAtribuicao            = ':=' ;
OpEndereco              = '&' ;
OpDesreferenciacao      = '*' ;
```

### 6.4 Delimitadores e Pontuação
```ebnf
DelimPontoEVirgula      = ';' ;
DelimDoisPontos         = ':' ;
DelimVirgula            = ',' ;
DelimPonto              = '.' ;
AbreParenteses          = '(' ;
FechaParenteses         = ')' ;
AbreColchetes           = '[' ;
FechaColchetes          = ']' ;
AbreChaves              = '{' ;
FechaChaves             = '}' ;
```

---

## 7. Mapeamento Multilíngue de Palavras-Chave

O vocabulário da linguagem é parametrizado através dos dicionários de palavras-chave:

| Token Interno | Palavra-Chave em Português (`pt-BR`) | Palavra-Chave em Inglês (`en-US`) |
| :--- | :--- | :--- |
| `KW_UNIT` | `unidade` | `unit` |
| `KW_PROGRAM` | `programa` | `program` |
| `KW_IMPORT` | `importe` | `import` |
| `KW_MAIN` | `principal` | `main` |
| `KW_IF` | `se` | `if` |
| `KW_ELSE` | `senao` | `else` |
| `KW_WHILE` | `enquanto` | `while` |
| `KW_FOR` | `para` | `for` |
| `KW_FROM` | `de` | `from` |
| `KW_TO` | `ate` | `to` |
| `KW_STEP` | `passo` | `step` |
| `KW_REPEAT` | `repita` | `repeat` |
| `KW_BREAK` | `aborte` | `break` |
| `KW_RETURN` | `retorne` | `return` |
| `KW_SWITCH` | `escolha` | `switch` |
| `KW_CASE` | `caso` | `case` |
| `KW_DEFAULT` | `padrao` | `default` |
| `KW_INT` | `inteiro` | `int` / `integer` |
| `KW_REAL` | `real` | `real` |
| `KW_FLOAT` | `flutuante` | `float` |
| `KW_CHAR` | `caractere` | `char` |
| `KW_STRING` | `texto` | `string` |
| `KW_BOOL` | `logico` | `bool` / `boolean` |
| `KW_ARRAY` | `arranjo` | `array` |
| `KW_OF` | `de` | `of` |
| `KW_STRUCT` | `registro` | `struct` |
| `KW_POINTER` | `ponteiro` | `pointer` |
| `KW_CONST` | `constante` | `const` |
| `KW_TYPE` | `tipo` | `type` |
| `KW_ALLOC` | `reserve` | `alloc` |
| `KW_FREE` | `libere` | `free` |
| `KW_READ` | `leia` | `read` |
| `KW_WRITE` | `escreva` | `write` |
| `KW_WRITELN` | `escrevaln` | `writeln` |
| `KW_TRUE` | `verdadeiro` | `true` |
| `KW_FALSE` | `falso` | `false` |
| `KW_NULL` | `nulo` | `null` |
| `KW_AND` | `e` | `and` |
| `KW_OR` | `ou` | `or` |
| `KW_NOT` | `nao` | `not` |
| `KW_DIV` | `div` | `div` |
| `KW_MOD` | `mod` | `mod` |
