# Preparação para a DEFESA — Programa SIMPLEX (TDE 2)

> A defesa cobra **duas coisas**: (1) você sabe explicar o código em detalhe e
> (2) você consegue fazer **mudanças pontuais** ao vivo. Além disso, são feitas
> ~2 perguntas para verificar autoria. Este documento mapeia o código ao método
> e lista os ensaios mais prováveis. Estude com o código aberto ao lado.

---

## 1. Visão geral em uma frase

O programa resolve PL pelo **SIMPLEX tabular** usando o **método das duas fases**.
Internamente tudo é tratado como **maximização** (minimizar `c·x` = maximizar
`-c·x`); a Fase 1 constrói uma base factível inicial com variáveis artificiais
quando há restrições `>=` ou `=`.

Fluxo: `main.cpp` lê o problema → `Simplex::solve()` resolve → `reporter` imprime.

---

## 2. Mapa CÓDIGO ↔ MÉTODO (onde está cada coisa)

| Etapa do método / requisito | Função | Arquivo:linha |
|---|---|---|
| Ler problema (arquivo / console) | `loadProblemFromFile`, `readProblemFromConsole` | `src/lp_problem.cpp` |
| Forma padrão (folga/excesso/artificial, `b>=0`) | `buildStandardForm` | `src/simplex.cpp:33` |
| Deixar a linha-Z coerente com a base ("price out") | `priceOutObjectiveRow` | `src/simplex.cpp:107` |
| Escolher variável que ENTRA | `chooseEnteringVariable` | `src/simplex.cpp:119` |
| Teste da razão (variável que SAI) / detectar SEM FRONTEIRA | `ratioTest` | `src/simplex.cpp:136` |
| Operação de pivô (Gauss-Jordan) | `pivot` | `src/simplex.cpp:168` |
| Fase 1 (artificiais) / detectar INVIÁVEL | `runPhaseOne` | `src/simplex.cpp:183` |
| Transição Fase 1 → Fase 2 (objetivo real + price out) | `setObjectiveForPhaseTwo` | `src/simplex.cpp:239` |
| Fase 2 (otimização do objetivo real) | `runPhaseTwo` | `src/simplex.cpp:252` |
| Detectar DEGENERAÇÃO | `detectDegeneracy` | `src/simplex.cpp:288` |
| Detectar ÓTIMOS ALTERNADOS (plus) | `detectAlternateOptima` | `src/simplex.cpp:314` |
| Recuperar x e o valor de Z | `extractSolution` | `src/simplex.cpp:300` |
| Imprimir tabela / resultado | `reporter::printTableau`, `printResult` | `src/reporter.cpp` |

### Requisitos obrigatórios → onde são cumpridos
1. **Maximização** → núcleo (`runPhaseTwo`); base de testes/depuração.
2. **Interface + tutorial** → `main.cpp` (console e arquivo) + `TUTORIAL.txt`.
3. **Número de iterações + tabela a cada passo** → contador `iterations` + `printIfVerbose`.
4. **Z ótimo / variáveis, final vs. intermediário** → `extractSolution` + `printResult`
   (mesmo bloco para TODOS os status, marcando se é final ou intermediário).
5. **Degeneração** → `detectDegeneracy` (variável básica = 0 OU empate no teste da razão).
6. **Inviabilidade** → `runPhaseOne`: soma das artificiais > 0 ao fim da Fase 1.
7. **Sem fronteira** → `ratioTest`: coluna de entrada sem entrada positiva.

### Pluses
- **Minimização (duas fases)** → `min` vira `max(-c)` em `setObjectiveForPhaseTwo`;
  Fase 1 em `runPhaseOne`.
- **Ótimos alternados** → `detectAlternateOptima` (custo reduzido = 0 em variável
  não-básica) e mostra o vértice alternativo pivotando-a para dentro.

---

## 3. Convenção da tabela (DECORE isto — é a pergunta mais provável)

- A **linha Z** guarda `z_j - c_j` (custo reduzido) de cada coluna; o RHS dela é o
  valor atual do objetivo (interno, de maximização).
- **Maximização**: a base é **ótima** quando **todos** os `z_j - c_j >= 0`.
- A **variável que entra** é a coluna com `z_j - c_j` **mais negativo**
  (regra de Dantzig). Empate → menor índice (anti-ciclagem).
- A **variável que sai** vem do **teste da razão**: menor `RHS_i / a_ik` entre
  `a_ik > 0`. Sem nenhum `a_ik > 0` → **ilimitado**.

> Por que `min` vira `max(-c)`? Porque o motor de iteração só sabe maximizar.
> O **valor reportado** é recalculado de `c` original em `extractSolution`, então
> nunca há confusão de sinal na resposta.

---

## 4. Os "acidentes" — como cada um é detectado

- **Inviável**: terminei a Fase 1 e a soma das artificiais ainda é > 0
  (`artificialSum() > FEAS_EPS`). Significa que não dá para zerar as artificiais,
  logo não há solução factível. (`runPhaseOne` retorna `false`.)
- **Sem fronteira (ilimitado)**: na coluna que vai entrar, **nenhum** elemento é
  positivo → não há limite para o crescimento da variável → Z cresce sem limite.
- **Degeneração**: alguma variável **básica vale 0** (vértice degenerado) ou houve
  **empate** no teste da razão (risco de ciclagem).
- **Ótimos alternados**: no ótimo, uma variável **não-básica** tem custo reduzido
  **= 0** → dá para movê-la sem mudar Z → existe outra solução ótima.

Detalhe técnico importante (saiba explicar): no fim da Fase 1, artificiais que
ficam na base em valor 0 são **retiradas** por `evictArtificialsFromBasis`; e na
Fase 2 as colunas artificiais ficam **proibidas** de reentrar (`forbidden`).

---

## 5. Ensaios de "mudança pontual" (pratique cada um ANTES da defesa)

O professor pode pedir uma alteração ao vivo. Saiba EXATAMENTE onde mexer:

1. **"Troque de maximização para minimização (ou vice-versa)."**
   → No arquivo de entrada, mude `max`/`min`. (Se pedir no código, o tratamento
   está em `setObjectiveForPhaseTwo`, linha do `orig.objective == Objective::MAX`.)

2. **"Adicione/remova uma restrição."**
   → No arquivo: ajuste o `m` (número de restrições) e a linha correspondente.
   Mostre que o programa recria folgas/artificiais sozinho.

3. **"Mude um coeficiente."**
   → Edite o número no arquivo e rode de novo; explique como ele entra na tabela
   em `buildStandardForm`.

4. **"Onde você detecta o caso ilimitado / degenerado / inviável / alternados?"**
   → Aponte direto para `ratioTest`, `detectDegeneracy`, `runPhaseOne`,
   `detectAlternateOptima` (use a tabela da seção 2).

5. **"Mostre o teste da razão / a regra de entrada / o pivô."**
   → `ratioTest`, `chooseEnteringVariable`, `pivot`.

6. **"Mude a regra de entrada para a menor coluna (regra de Bland)."**
   → Em `chooseEnteringVariable`, em vez de "mais negativo", pegue a **primeira**
   coluna com `z_j - c_j < -EPS`. (Saiba fazer essa troca de cabeça.)

7. **"Imprima também as folgas no resultado."**
   → Em `extractSolution`/`printResult`, hoje só mostramos `x1..xn`; explique como
   estenderia para as colunas de folga usando `colName` e `basis`.

---

## 6. Perguntas de autoria prováveis (e respostas curtas)

- **"Por que duas fases e não o M-grande?"** O enunciado desaconselha o M-grande;
  as duas fases evitam um "M" gigante que causa erro numérico — a Fase 1 só
  minimiza a soma das artificiais para achar uma base factível.
- **"O que é o `priceOutObjectiveRow` e por que é chamado na transição?"** Ele
  zera, na linha Z, as colunas que estão na base. Sem isso, os custos reduzidos da
  Fase 2 ficariam errados logo na primeira escolha — é o bug clássico das duas fases.
- **"Para que serve o `EPS`?"** Comparar `double` com 0 exato é perigoso; uso uma
  tolerância pequena para "é zero?", para empates e para a viabilidade.
- **"Como garante que não entra em loop infinito?"** Empate no teste da razão usa
  o menor índice (anti-ciclagem) e há um **limite de iterações** que devolve uma
  resposta intermediária marcada como INTERROMPIDO.
- **"Por que recalcula Z a partir de `c` original?"** Porque internamente
  maximizo `-c` no `min`; recalcular de `c` evita erro de sinal na resposta.
- **"E se o problema for degenerado E tiver custo reduzido zero numa não-básica?"**
  (pergunta capciosa) Custo reduzido zero numa não-básica só é **ótimos
  alternados** se eu conseguir de fato **chegar a outro vértice ótimo** com ela.
  Em `detectAlternateOptima` eu faço o teste da razão nessa coluna: se o pivô moveria
  para um vértice diferente (razão > 0), reporto ótimos alternados e mostro o vértice;
  se o único pivô é **degenerado** (razão 0, mesmo vértice), isso é **degeneração**,
  não ótimos alternados — e o programa reporta como degeneração. Assim os dois
  acidentes não se confundem.

---

## 7. Roteiro de demonstração (5 min)

1. `make` → mostra que compila limpo.
2. `./simplex tests/01_max_textbook.txt --step` → percorre a tabela iteração a
   iteração (mostra entra/sai, contagem de iterações, Z=36).
3. `./simplex tests/06_min_twophase.txt` → minimização com duas fases (Z=9).
4. `./simplex tests/03_unbounded.txt` e `04_infeasible.txt` → acidentes.
5. `./simplex tests/05_alternate_optima.txt` → ótimos alternados (vértice alt.).
6. `./simplex` (sem arquivo) → digita um problema na hora pelo console.
