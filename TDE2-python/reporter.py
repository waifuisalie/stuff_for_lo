"""reporter.py -- all user-facing output (Python version).

 * print_problem  -- echo the LP that was read in.
 * print_tableau  -- one iteration of the tableau (requirement #3).
 * print_result   -- the uniform final block used for EVERY terminal state, so
                     even an interrupted answer still shows variable values and a
                     clear status label (requirement #4).
"""
from __future__ import annotations

from typing import List

from lp_problem import LPProblem, Objective, ConstraintType
from simplex import SolveResult, SolveStatus

_SYM = {ConstraintType.LE: "<=", ConstraintType.GE: ">=", ConstraintType.EQ: "="}


def _cell(v: float) -> str:
    """Format a number: integers print without a decimal part for readability."""
    if abs(v) < 1e-9:
        v = 0.0  # avoid "-0"
    if abs(v - round(v)) < 1e-6:
        return str(int(round(v)))
    return f"{v:.3f}"


def _terms(coeffs: List[float]) -> str:
    return " + ".join(f"{_cell(coeffs[j])}*x{j + 1}" for j in range(len(coeffs)))


def print_problem(p: LPProblem) -> None:
    print("\n==================== PROBLEMA ====================")
    sense = "Maximizar" if p.objective == Objective.MAX else "Minimizar"
    print(f"{sense}  Z = {_terms(p.c)}")
    print("sujeito a:")
    for r in range(p.m):
        print(f"  {_terms(p.A[r])} {_SYM[p.rel[r]]} {_cell(p.b[r])}")
    print("  x_j >= 0")
    print("==================================================")


def print_tableau(iteration: int, phase: str, T, basis, col_name, step_through: bool) -> None:
    m = len(basis)
    total_vars = len(col_name)
    rhs = total_vars
    obj_row = m
    w = 9

    print(f"\n--- {phase} | Iteracao {iteration} ---")
    header = f"{'Base':>7}" + "".join(f"{name:>{w}}" for name in col_name) + f"{'RHS':>{w}}"
    print(header)
    for i in range(m):
        row = f"{col_name[basis[i]]:>7}" + "".join(f"{_cell(T[i][j]):>{w}}" for j in range(rhs + 1))
        print(row)
    obj = f"{'Z':>7}" + "".join(f"{_cell(T[obj_row][j]):>{w}}" for j in range(rhs + 1))
    print(obj)

    if step_through:
        try:
            input("[Enter para continuar]")
        except EOFError:
            pass


def print_result(p: LPProblem, r: SolveResult) -> None:
    print("\n==================== RESULTADO ====================")

    is_final = r.status == SolveStatus.OPTIMAL
    labels = {
        SolveStatus.OPTIMAL: "STATUS: SOLUCAO OTIMA ENCONTRADA (resposta final)",
        SolveStatus.INFEASIBLE: "STATUS: PROBLEMA INVIAVEL (nao existe solucao factivel)",
        SolveStatus.UNBOUNDED: "STATUS: PROBLEMA SEM FRONTEIRA / ILIMITADO",
        SolveStatus.ITERATION_LIMIT: "STATUS: INTERROMPIDO - limite de iteracoes (resposta intermediaria)",
    }
    print(labels[r.status])
    print(f"Numero de iteracoes: {r.iterations}")

    if is_final:
        print(f"Valor otimo  Z = {_cell(r.objective_value)}")
    else:
        print(f"Valor atual da funcao objetivo (intermediario) Z = {_cell(r.objective_value)}")

    print("Valores das variaveis" + (":" if is_final else " (estado atual / intermediario):"))
    for j in range(p.n):
        print(f"  x{j + 1} = {_cell(r.variable_values[j])}")

    if r.degenerate:
        msg = "[DEGENERACAO] O problema apresenta degeneracao"
        if r.degenerate_vars:
            msg += " (variaveis basicas nulas: " + " ".join(r.degenerate_vars) + ")"
        print(msg + ".")

    if r.alternate_optima:
        print("[OTIMOS ALTERNADOS] Existe mais de uma solucao otima.")
        if r.alternate_vertex:
            verts = " ".join(f"x{j + 1}={_cell(r.alternate_vertex[j])}" for j in range(p.n))
            print(f"  Vertice otimo alternativo: {verts}")

    if r.note:
        print(f"Obs: {r.note}")
    print("===================================================")
