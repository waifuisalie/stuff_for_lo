# Preparação para a DEFESA — Programa SIMPLEX (TDE 2) — VERSÃO PYTHON

> A defesa cobra **três coisas**: (1) você sabe explicar o código em detalhe,
> (2) você consegue fazer **mudanças pontuais** ao vivo, e (3) as funcionalidades
> serão testadas com problemas que o professor escolher. Além disso, ~2 perguntas
> para verificar autoria. Estude com `simplex.py` aberto ao lado.
>
> Todas as referências abaixo são para os arquivos `.py` desta pasta (`TDE2-python/`).

---

## 1. Visão geral em uma frase

O programa resolve PL pelo **SIMPLEX tabular** com o **método das duas fases**.
Internamente tudo é **maximização** (minimizar `c·x` = maximizar `-c·x`); a Fase 1
constrói uma base factível inicial com variáveis artificiais quando há restrições
`>=` ou `=`.

Fluxo: `main.py` lê → `Simplex.solve()` resolve → `reporter` imprime.

---

## 2. Mapa CÓDIGO ↔ MÉTODO (onde está cada coisa)

| Etapa do método / requisito | Função (Python) | Arquivo:linha |
|---|---|---|
| Ler problema (arquivo / console) | `load_problem_from_file`, `read_problem_from_console` | `lp_problem.py:66`, `:139` |
| Forma padrão (folga/excesso/artificial, `b>=0`) | `build_standard_form` | `simplex.py:79` |
| Deixar a linha-Z coerente com a base ("price out") | `price_out_objective_row` | `simplex.py:149` |
| Escolher variável que ENTRA | `choose_entering_variable` | `simplex.py:162` |
| Teste da razão (variável que SAI) / detectar SEM FRONTEIRA | `ratio_test` | `simplex.py:177` |
| Operação de pivô (Gauss-Jordan) | `pivot` | `simplex.py:204` |
| Fase 1 (artificiais) / detectar INVIÁVEL | `run_phase_one` | `simplex.py:222` |
| Transição Fase 1 → Fase 2 (objetivo real + price out) | `set_objective_for_phase_two` | `simplex.py:275` |
| Fase 2 (otimização do objetivo real) | `run_phase_two` | `simplex.py:290` |
| Detectar DEGENERAÇÃO | `detect_degeneracy` | `simplex.py:324` |
| Detectar ÓTIMOS ALTERNADOS (plus) | `detect_alternate_optima` | `simplex.py:346` |
| Recuperar x e o valor de Z | `extract_solution` | `simplex.py:332` |
| Imprimir tabela / resultado | `print_tableau`, `print_result` | `reporter.py:43`, `:66` |

### Requisitos obrigatórios → onde são cumpridos
1. **Maximização** → núcleo (`run_phase_two`); base de testes/depuração.
2. **Interface + tutorial** → `main.py` (console e arquivo) + `TUTORIAL.txt`.
3. **Número de iterações + tabela a cada passo** → contador `self.iterations` + `_print_if_verbose` (`simplex.py:382`).
4. **Z ótimo / variáveis, final vs. intermediário** → `extract_solution` + `print_result`
   (mesmo bloco para TODOS os status, marcando se é final ou intermediário).
5. **Degeneração** → `detect_degeneracy` (variável básica = 0 OU empate no teste da razão).
6. **Inviabilidade** → `run_phase_one`: soma das artificiais > 0 ao fim da Fase 1 (`simplex.py:246`).
7. **Sem fronteira** → `ratio_test`: coluna de entrada sem entrada positiva (`simplex.py:193`).

### Pluses
- **Minimização (duas fases)** → `min` vira `max(-c)` em `set_objective_for_phase_two` (`simplex.py:282`).
- **Ótimos alternados** → `detect_alternate_optima` (custo reduzido = 0 numa não-básica)
  e mostra o vértice alternativo pivotando-a para dentro.

---

## 3. Convenção da tabela (DECORE isto — é a pergunta mais provável)

- A **linha Z** guarda `z_j - c_j` (custo reduzido) de cada coluna; o RHS dela é o
  valor atual do objetivo (interno, de maximização).
- **Maximização**: a base é **ótima** quando **todos** os `z_j - c_j >= 0`.
- A **variável que entra** é a coluna com `z_j - c_j` **mais negativo**
  (regra de Dantzig — `choose_entering_variable`, `simplex.py:162`). Empate → menor índice.
- A **variável que sai** vem do **teste da razão**: menor `RHS_i / a_ik` entre
  `a_ik > 0` (`ratio_test`). Sem nenhum `a_ik > 0` → **ilimitado**.

> Por que `min` vira `max(-c)`? Porque o motor de iteração só sabe maximizar.
> O **valor reportado** é recalculado de `c` original em `extract_solution`, então
> nunca há confusão de sinal na resposta.

---

## 4. Os "acidentes" — como cada um é detectado

- **Inviável** (`run_phase_one`, linha 246): terminei a Fase 1 e a soma das
  artificiais ainda é > 0 → não dá para zerar as artificiais → não há solução factível.
- **Sem fronteira / ilimitado** (`ratio_test`, linha 193): na coluna que vai entrar,
  **nenhum** elemento é positivo → a variável cresce sem limite → Z cresce sem limite.
- **Degeneração** (`detect_degeneracy`, linha 324): alguma variável **básica vale 0**
  (vértice degenerado) ou houve **empate** no teste da razão (`self.tie_seen`).
- **Ótimos alternados** (`detect_alternate_optima`, linha 346): no ótimo, uma variável
  **não-básica** tem custo reduzido **= 0** E dá pra chegar a outro vértice → existe
  outra solução ótima.

Detalhe técnico (saiba explicar): no fim da Fase 1, artificiais que ficam na base em
valor 0 são retiradas por `evict_artificials_from_basis` (`simplex.py:258`); e na
Fase 2 as colunas artificiais ficam **proibidas** de reentrar (`self.forbidden`,
marcadas em `set_objective_for_phase_two`).

---

## 5. Ensaios de "mudança pontual" (pratique cada um ANTES da defesa)

1. **"Troque max ↔ min."**
   → No arquivo de entrada, mude a 1ª linha `max`/`min`.
   No código, o tratamento está em `set_objective_for_phase_two`, **`simplex.py:282`**:
   `cj = self.orig.c[j] if self.orig.objective == Objective.MAX else -self.orig.c[j]`.

2. **"Adicione/remova uma restrição."**
   → No arquivo: ajuste `m` (nº de restrições) e a linha correspondente. O programa
   recria folgas/artificiais sozinho em `build_standard_form`.

3. **"Mude um coeficiente."**
   → Edite o número no arquivo e rode de novo.

4. **"Onde você detecta ilimitado / degenerado / inviável / alternados?"**
   → Aponte direto: `ratio_test:193`, `detect_degeneracy:324`, `run_phase_one:246`,
   `detect_alternate_optima:346`.

5. **"Mostre o teste da razão / a regra de entrada / o pivô."**
   → `ratio_test:177`, `choose_entering_variable:162`, `pivot:204`.

6. **"Troque a regra de entrada para a de Bland (menor índice)."**
   → Em `choose_entering_variable` (`simplex.py:162`), troque o "mais negativo" por
   "a primeira coluna com custo reduzido negativo":
   ```python
   def choose_entering_variable(self) -> int:
       obj = self.T[self.obj_row]
       for j in range(self.total_vars):
           if self.forbidden[j]:
               continue
           if obj[j] < -EPS:      # primeira (menor indice) coluna elegivel
               return j
       return -1
   ```

7. **"Mostre também as folgas no resultado."**
   → As folgas JÁ aparecem na última tabela impressa (coluna RHS de cada linha cuja
   base é `s...`). Para colocá-las no bloco RESULTADO, em `extract_solution`
   (`simplex.py:332`) hoje só guardo as colunas de decisão (`bc < self.n`); bastaria
   guardar também as básicas de folga (percorrendo `self.basis`/`self.col_name`) e
   imprimi-las em `print_result`.

8. **"Mude a tolerância / o limite de iterações."**
   → `EPS` em `simplex.py:21`; limite de iterações em `simplex.py:231` e `:291`.

---

## 6. Perguntas de autoria prováveis (e respostas curtas)

- **"Por que duas fases e não o M-grande?"** O enunciado desaconselha o M-grande; as
  duas fases evitam um "M" gigante que causa erro numérico — a Fase 1 só minimiza a
  soma das artificiais para achar uma base factível.
- **"O que é o `price_out_objective_row` e por que é chamado na transição?"** Ele zera,
  na linha Z, as colunas que estão na base. Sem isso, os custos reduzidos da Fase 2
  ficariam errados logo na primeira escolha — é o bug clássico das duas fases.
- **"Para que serve o `EPS`?"** Comparar `float` com 0 exato é perigoso; uso uma
  tolerância pequena para "é zero?", para empates e para a viabilidade.
- **"Como garante que não entra em loop infinito?"** Empate no teste da razão favorece
  o menor índice (anti-ciclagem) e há um **limite de iterações** que devolve uma
  resposta intermediária marcada como INTERROMPIDO.
- **"Por que recalcula Z a partir de `c` original?"** Porque internamente maximizo `-c`
  no `min`; recalcular de `c` evita erro de sinal na resposta.
- **"E se o problema for degenerado E tiver custo reduzido zero numa não-básica?"**
  Custo reduzido zero só é **ótimos alternados** se eu conseguir chegar a OUTRO vértice
  ótimo. Em `detect_alternate_optima` faço o teste da razão nessa coluna: se o pivô
  moveria para um vértice diferente (razão > 0), reporto ótimos alternados; se o único
  pivô é degenerado (razão 0, mesmo vértice), isso é **degeneração**, não alternados.

---

## 7. Roteiro de demonstração (5 min)

1. `python3 main.py tests/01_max_textbook.txt --step` → percorre a tabela iteração a
   iteração (mostra entra/sai, contagem, Z=36).
2. `python3 main.py tests/06_min_twophase.txt` → minimização com duas fases (Z=9).
3. `python3 main.py tests/03_unbounded.txt` e `tests/04_infeasible.txt` → acidentes.
4. `python3 main.py tests/05_alternate_optima.txt` → ótimos alternados (vértice alt.).
5. `python3 main.py` (sem arquivo) → digita um problema na hora pelo console.

> Se o professor der um problema novo, escreva um arquivo `.txt` no formato do
> TUTORIAL (mais seguro que digitar no console) e rode. Saiba escrever o `0` para
> variáveis ausentes numa restrição (ex.: `x1 <= 4` vira `1 0 <= 4`).
