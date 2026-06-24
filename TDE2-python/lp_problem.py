"""lp_problem.py -- the LP model plus the two input paths (file and console).

This is just the "model": objective sense, the objective coefficients and the
constraint rows. All SIMPLEX machinery lives in simplex.py so this file stays
easy to read and explain.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import List


class Objective(Enum):
    MAX = "max"
    MIN = "min"


class ConstraintType(Enum):
    LE = "<="
    GE = ">="
    EQ = "="


@dataclass
class LPProblem:
    """An LP in original form:

        max / min   Z = c[0]*x1 + ... + c[n-1]*xn
        subject to  A[i] . x  (rel[i])  b[i]      for i = 0..m-1
                    x >= 0
    """
    objective: Objective = Objective.MAX
    n: int = 0                                       # decision variables
    m: int = 0                                       # constraints
    c: List[float] = field(default_factory=list)     # objective coefficients (n)
    A: List[List[float]] = field(default_factory=list)  # constraint matrix (m x n)
    rel: List[ConstraintType] = field(default_factory=list)  # relations (m)
    b: List[float] = field(default_factory=list)     # right-hand sides (m)


class ParseError(Exception):
    """Raised when the input cannot be understood."""


# Forgiving spellings, like the C++ version.
_REL = {"<=": ConstraintType.LE, "<": ConstraintType.LE,
        ">=": ConstraintType.GE, ">": ConstraintType.GE,
        "=": ConstraintType.EQ, "==": ConstraintType.EQ}

_OBJ = {"max": Objective.MAX, "maximize": Objective.MAX,
        "min": Objective.MIN, "minimize": Objective.MIN}


def _tokenize(text: str) -> List[str]:
    """Flatten text into whitespace-separated tokens, dropping '#' comments."""
    toks: List[str] = []
    for line in text.splitlines():
        hash_i = line.find("#")
        if hash_i != -1:
            line = line[:hash_i]
        toks.extend(line.split())
    return toks


def load_problem_from_file(path: str) -> LPProblem:
    """Read a problem from a text file (format documented in TUTORIAL.txt).

    Token order:  max|min, then n m, then c1..cn, then for each constraint
    a1..an  <rel>  b.
    """
    try:
        with open(path, "r") as f:
            toks = _tokenize(f.read())
    except OSError:
        raise ParseError(f"nao foi possivel abrir o arquivo: {path}")

    pos = 0

    def nxt(what: str) -> str:
        nonlocal pos
        if pos >= len(toks):
            raise ParseError(f"entrada incompleta: esperava {what}")
        t = toks[pos]
        pos += 1
        return t

    def num(what: str) -> float:
        t = nxt(what)
        try:
            return float(t)
        except ValueError:
            raise ParseError(f"valor numerico invalido para {what}: '{t}'")

    p = LPProblem()

    o = nxt("'max' ou 'min'").lower()
    if o not in _OBJ:
        raise ParseError(f"primeiro token deve ser 'max' ou 'min', veio '{o}'")
    p.objective = _OBJ[o]

    try:
        p.n = int(nxt("n"))
        p.m = int(nxt("m"))
    except ValueError:
        raise ParseError("n e m devem ser inteiros")
    if p.n <= 0 or p.m <= 0:
        raise ParseError("n e m devem ser positivos")

    p.c = [num(f"c{j + 1}") for j in range(p.n)]

    for r in range(p.m):
        row = [num(f"a[{r + 1}][{j + 1}]") for j in range(p.n)]
        rel_tok = nxt(f"relacao da restricao {r + 1}")
        if rel_tok not in _REL:
            raise ParseError(f"relacao invalida '{rel_tok}' na restricao {r + 1}")
        p.A.append(row)
        p.rel.append(_REL[rel_tok])
        p.b.append(num(f"b{r + 1}"))

    validate_problem(p)
    return p


def _read_int(prompt: str, positive: bool = False) -> int:
    while True:
        s = input(prompt).strip()
        try:
            v = int(s)
        except ValueError:
            print("  digite um numero inteiro.")
            continue
        if positive and v <= 0:
            print("  digite um inteiro positivo.")
            continue
        return v


def read_problem_from_console() -> LPProblem:
    """Read a problem interactively from stdin (guided prompts).

    Each numeric line is split on whitespace, so the user may type the whole
    objective (e.g. "3 5") or a whole constraint (e.g. "1 0 <= 4") on one line.
    """
    p = LPProblem()

    o = input("Objetivo (max/min): ").strip().lower()
    while o not in _OBJ:
        o = input("  digite 'max' ou 'min': ").strip().lower()
    p.objective = _OBJ[o]

    p.n = _read_int("Numero de variaveis de decisao (n): ", positive=True)
    p.m = _read_int("Numero de restricoes (m): ", positive=True)

    # Objective coefficients, all on one line.
    while True:
        toks = input(f"Coeficientes do objetivo (c1..c{p.n}, separados por espaco): ").split()
        if len(toks) != p.n:
            print(f"  esperados {p.n} valores, recebidos {len(toks)}.")
            continue
        try:
            p.c = [float(t) for t in toks]
            break
        except ValueError:
            print("  valores numericos invalidos, tente de novo.")

    # Each constraint on one line:  a1 .. an  REL  b
    for r in range(p.m):
        while True:
            toks = input(f"Restricao {r + 1} (a1..a{p.n}  REL(<=,>=,=)  b): ").split()
            if len(toks) != p.n + 2:
                print(f"  esperados {p.n + 2} campos (n coeficientes + relacao + b), "
                      f"recebidos {len(toks)}.")
                continue
            rel_tok = toks[p.n]
            if rel_tok not in _REL:
                print(f"  relacao invalida '{rel_tok}', use <=, >= ou =.")
                continue
            try:
                row = [float(t) for t in toks[:p.n]]
                bval = float(toks[p.n + 1])
            except ValueError:
                print("  valores numericos invalidos, tente de novo.")
                continue
            p.A.append(row)
            p.rel.append(_REL[rel_tok])
            p.b.append(bval)
            break

    validate_problem(p)
    return p


def validate_problem(p: LPProblem) -> None:
    """Basic dimension checks. Raises ParseError if something is inconsistent."""
    if p.n <= 0 or p.m <= 0:
        raise ParseError("n e m devem ser positivos")
    if len(p.c) != p.n:
        raise ParseError("numero de coeficientes do objetivo != n")
    if len(p.A) != p.m or len(p.rel) != p.m or len(p.b) != p.m:
        raise ParseError("numero de restricoes inconsistente")
    for r, row in enumerate(p.A):
        if len(row) != p.n:
            raise ParseError(f"restricao {r + 1} tem numero de coeficientes errado")
