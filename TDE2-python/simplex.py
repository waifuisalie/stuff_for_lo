"""simplex.py -- the SIMPLEX engine (tabular form, two-phase).

This is a faithful port of the C++ version. Tableau convention (verified by hand
on the Wyndor problem):

  * The objective ("z") row stores  z_j - c_j  for every column, and the current
    objective value in its RHS cell.
  * For maximization the basis is OPTIMAL when every z-row entry is >= 0; the
    ENTERING variable is the column with the most negative z-row entry.
  * Minimization is solved by maximizing the negated objective internally; the
    reported value is recomputed from the ORIGINAL coefficients.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional, Tuple

from lp_problem import LPProblem, Objective, ConstraintType

EPS = 1e-9        # "is this number zero?" tolerance
TIE_EPS = 1e-7    # ratio-test tie tolerance (degeneracy signal)
FEAS_EPS = 1e-7   # Phase-1 feasibility tolerance on the artificial sum


class SolveStatus(Enum):
    OPTIMAL = "optimal"
    INFEASIBLE = "infeasible"
    UNBOUNDED = "unbounded"
    ITERATION_LIMIT = "iteration_limit"


class VarKind(Enum):
    DECISION = 0
    SLACK = 1
    SURPLUS = 2
    ARTIFICIAL = 3


@dataclass
class SolveResult:
    status: SolveStatus = SolveStatus.OPTIMAL
    objective_value: float = 0.0
    variable_values: List[float] = field(default_factory=list)
    iterations: int = 0
    degenerate: bool = False
    alternate_optima: bool = False
    degenerate_vars: List[str] = field(default_factory=list)
    alternate_vertex: List[float] = field(default_factory=list)
    note: str = ""


class Simplex:
    def __init__(self, problem: LPProblem, verbose: bool, step_through: bool, reporter=None):
        self.orig = problem
        self.n = problem.n
        self.m = problem.m
        self.verbose = verbose
        self.step_through = step_through
        self.reporter = reporter  # module exposing print_tableau (injected by main)

        # The tableau: (m+1) rows -- rows 0..m-1 are constraints, row m is the
        # objective ("z") row. Each row has (total_vars + 1) entries; the last
        # column is the RHS.
        self.T: List[List[float]] = []
        self.basis: List[int] = []         # basis[i] = column basic in row i
        self.kind: List[VarKind] = []      # kind of each variable column
        self.col_name: List[str] = []      # display name of each column
        self.forbidden: List[bool] = []    # excluded from entering (artificials in Phase 2)

        self.total_vars = 0
        self.rhs = 0       # index of the RHS column
        self.obj_row = 0   # index of the objective row
        self.iterations = 0
        self.tie_seen = False
        self.artificial_cols: List[int] = []

    # ------------------------------------------------------------------ build
    def build_standard_form(self) -> None:
        A = [row[:] for row in self.orig.A]
        b = self.orig.b[:]
        rel = self.orig.rel[:]

        # Make every b_i >= 0 (flip the relation when we negate a row).
        for r in range(self.m):
            if b[r] < 0.0:
                A[r] = [-v for v in A[r]]
                b[r] = -b[r]
                if rel[r] == ConstraintType.LE:
                    rel[r] = ConstraintType.GE
                elif rel[r] == ConstraintType.GE:
                    rel[r] = ConstraintType.LE

        extra = 0
        for r in range(self.m):
            if rel[r] == ConstraintType.LE:
                extra += 1            # slack
            elif rel[r] == ConstraintType.GE:
                extra += 2            # surplus + artificial
            else:
                extra += 1            # artificial

        self.total_vars = self.n + extra
        self.rhs = self.total_vars
        self.obj_row = self.m

        self.T = [[0.0] * (self.total_vars + 1) for _ in range(self.m + 1)]
        self.kind = [VarKind.DECISION] * self.total_vars
        self.col_name = [""] * self.total_vars
        self.forbidden = [False] * self.total_vars
        self.basis = [-1] * self.m

        for j in range(self.n):
            self.kind[j] = VarKind.DECISION
            self.col_name[j] = f"x{j + 1}"
        for r in range(self.m):
            for j in range(self.n):
                self.T[r][j] = A[r][j]
            self.T[r][self.rhs] = b[r]

        col = self.n
        for r in range(self.m):
            if rel[r] == ConstraintType.LE:
                self.kind[col] = VarKind.SLACK
                self.col_name[col] = f"s{r + 1}"
                self.T[r][col] = 1.0
                self.basis[r] = col
                col += 1
            elif rel[r] == ConstraintType.GE:
                self.kind[col] = VarKind.SURPLUS
                self.col_name[col] = f"e{r + 1}"
                self.T[r][col] = -1.0
                col += 1
                self.kind[col] = VarKind.ARTIFICIAL
                self.col_name[col] = f"a{r + 1}"
                self.T[r][col] = 1.0
                self.basis[r] = col
                self.artificial_cols.append(col)
                col += 1
            else:  # EQ
                self.kind[col] = VarKind.ARTIFICIAL
                self.col_name[col] = f"a{r + 1}"
                self.T[r][col] = 1.0
                self.basis[r] = col
                self.artificial_cols.append(col)
                col += 1

    # ------------------------------------------------------------- core steps
    def price_out_objective_row(self) -> None:
        """Make the z row consistent with the current basis: every basic column
        reads 0 in the objective row. This is the per-phase initializer AND the
        critical Phase-1 -> Phase-2 hand-off ("price out" the real objective)."""
        obj = self.T[self.obj_row]
        for i in range(self.m):
            bc = self.basis[i]
            f = obj[bc]
            if abs(f) > EPS:
                row_i = self.T[i]
                for c in range(self.rhs + 1):
                    obj[c] -= f * row_i[c]

    def choose_entering_variable(self) -> int:
        """Most-negative z-row entry among allowed columns (Dantzig rule). Ties
        keep the smallest index. Returns -1 when the basis is optimal."""
        best = -1
        most_neg = -EPS  # must be strictly more negative than -EPS to enter
        obj = self.T[self.obj_row]
        for j in range(self.total_vars):
            if self.forbidden[j]:
                continue
            rc = obj[j]
            if rc < most_neg:
                most_neg = rc
                best = j
        return best

    def ratio_test(self, enter_col: int) -> Tuple[int, bool]:
        """Minimum-ratio test on the entering column. Returns (leave_row,
        unbounded). A tie in the minimum ratio is recorded as a degeneracy
        signal."""
        leave = -1
        best = 0.0
        any_pos = False
        for i in range(self.m):
            a = self.T[i][enter_col]
            if a > EPS:
                ratio = self.T[i][self.rhs] / a
                if not any_pos or ratio < best - EPS:
                    best = ratio
                    leave = i
                    any_pos = True
        if not any_pos:
            return -1, True

        ties = 0
        for i in range(self.m):
            a = self.T[i][enter_col]
            if a > EPS and abs(self.T[i][self.rhs] / a - best) <= TIE_EPS:
                ties += 1
        if ties > 1:
            self.tie_seen = True
        return leave, False

    def pivot(self, row: int, col: int) -> None:
        """Gauss-Jordan pivot: normalize the pivot row, then eliminate the pivot
        column from every other row INCLUDING the objective row."""
        pr = self.T[row]
        p = pr[col]
        for c in range(self.rhs + 1):
            pr[c] /= p
        for i in range(self.obj_row + 1):
            if i == row:
                continue
            ti = self.T[i]
            f = ti[col]
            if abs(f) > EPS:
                for c in range(self.rhs + 1):
                    ti[c] -= f * pr[c]
        self.basis[row] = col

    # ----------------------------------------------------------------- phases
    def run_phase_one(self) -> bool:
        """Minimize the sum of artificial variables (== maximize its negative).
        Returns True if the problem is feasible (that sum reaches zero)."""
        obj = self.T[self.obj_row]
        for j in range(self.total_vars):
            obj[j] = 1.0 if self.kind[j] == VarKind.ARTIFICIAL else 0.0
        obj[self.rhs] = 0.0
        self.price_out_objective_row()

        max_iter = 50 * (self.m + self.total_vars) + 100
        self._print_if_verbose("FASE 1")
        while self.iterations <= max_iter:
            enter = self.choose_entering_variable()
            if enter == -1:
                break  # Phase-1 optimal
            leave, unbounded = self.ratio_test(enter)
            if unbounded:
                break  # not expected in Phase 1; bail safely
            if self.verbose:
                print(f"  -> entra {self.col_name[enter]}, sai {self.col_name[self.basis[leave]]}")
            self.pivot(leave, enter)
            self.iterations += 1
            self._print_if_verbose("FASE 1")

        if self.artificial_sum() > FEAS_EPS:
            return False  # infeasible
        self.evict_artificials_from_basis()
        return True

    def artificial_sum(self) -> float:
        s = 0.0
        for i in range(self.m):
            if self.kind[self.basis[i]] == VarKind.ARTIFICIAL:
                s += self.T[i][self.rhs]
        return s

    def evict_artificials_from_basis(self) -> None:
        """After a feasible Phase 1, drive any zero-valued artificial out of the
        basis. If the whole non-artificial part of the row is zero, the
        constraint is redundant and the artificial stays basic at 0."""
        for i in range(self.m):
            if self.kind[self.basis[i]] != VarKind.ARTIFICIAL:
                continue
            pcol = -1
            for j in range(self.total_vars):
                if self.kind[j] == VarKind.ARTIFICIAL:
                    continue
                if abs(self.T[i][j]) > EPS:
                    pcol = j
                    break
            if pcol != -1:
                self.pivot(i, pcol)

    def set_objective_for_phase_two(self) -> None:
        """Load the real objective (in maximization form) into the z row, forbid
        the artificial columns from re-entering, then price it out."""
        obj = self.T[self.obj_row]
        for j in range(self.total_vars):
            cj = 0.0
            if self.kind[j] == VarKind.DECISION:
                cj = self.orig.c[j] if self.orig.objective == Objective.MAX else -self.orig.c[j]
            obj[j] = -cj
        obj[self.rhs] = 0.0
        for j in range(self.total_vars):
            if self.kind[j] == VarKind.ARTIFICIAL:
                self.forbidden[j] = True
        self.price_out_objective_row()

    def run_phase_two(self, result: SolveResult) -> None:
        max_iter = 50 * (self.m + self.total_vars) + 100
        # Only ratio-test ties that happen in Phase 2 say anything about whether
        # the FINAL vertex is degenerate; Phase-1 ties are common and irrelevant.
        self.tie_seen = False
        self._print_if_verbose("FASE 2")
        while True:
            enter = self.choose_entering_variable()
            if enter == -1:
                result.status = SolveStatus.OPTIMAL
                break
            leave, unbounded = self.ratio_test(enter)
            if unbounded:
                result.status = SolveStatus.UNBOUNDED
                result.note = (f"Coluna de entrada ({self.col_name[enter]}) nao possui "
                               "razao positiva: problema sem fronteira.")
                break
            if self.verbose:
                print(f"  -> entra {self.col_name[enter]}, sai {self.col_name[self.basis[leave]]}")
            self.pivot(leave, enter)
            self.iterations += 1
            self._print_if_verbose("FASE 2")
            if self.iterations > max_iter:
                result.status = SolveStatus.ITERATION_LIMIT
                result.note = ("Limite de iteracoes atingido (possivel ciclagem). "
                               "Resposta intermediaria.")
                break

        self.detect_degeneracy(result)
        self.extract_solution(result)
        if result.status == SolveStatus.OPTIMAL:
            self.detect_alternate_optima(result)

    # -------------------------------------------------- accidents / solution
    def detect_degeneracy(self, result: SolveResult) -> None:
        for i in range(self.m):
            if abs(self.T[i][self.rhs]) < EPS:  # a basic variable sits at zero
                result.degenerate = True
                result.degenerate_vars.append(self.col_name[self.basis[i]])
        if self.tie_seen:
            result.degenerate = True

    def extract_solution(self, result: SolveResult) -> None:
        """Recover decision-variable values and the ORIGINAL objective value.
        Called for EVERY terminal state so even an interrupted answer shows
        variable values."""
        result.variable_values = [0.0] * self.n
        for i in range(self.m):
            bc = self.basis[i]
            if 0 <= bc < self.n:
                result.variable_values[bc] = self.T[i][self.rhs]
        z = 0.0
        for j in range(self.n):
            z += self.orig.c[j] * result.variable_values[j]
        result.objective_value = z

    def detect_alternate_optima(self, result: SolveResult) -> None:
        """A zero reduced cost on a non-basic variable only means alternate
        optima if we can actually REACH a different optimal point with it. If the
        only pivot it allows is degenerate (ratio 0, same vertex), that is
        degeneracy, not an alternate optimum."""
        is_basic = [False] * self.total_vars
        for i in range(self.m):
            is_basic[self.basis[i]] = True
        obj = self.T[self.obj_row]

        for j in range(self.total_vars):
            if self.forbidden[j] or is_basic[j]:
                continue
            if abs(obj[j]) >= EPS:
                continue  # reduced cost not zero

            leave, unbounded = self.ratio_test(j)
            if unbounded:
                result.alternate_optima = True  # Z constant along an unbounded edge
                return
            if leave == -1:
                continue
            min_ratio = self.T[leave][self.rhs] / self.T[leave][j]
            if min_ratio > EPS:
                # Bringing j in moves us to a genuinely different optimal vertex.
                result.alternate_optima = True
                self.pivot(leave, j)
                alt = [0.0] * self.n
                for i in range(self.m):
                    if 0 <= self.basis[i] < self.n:
                        alt[self.basis[i]] = self.T[i][self.rhs]
                result.alternate_vertex = alt
                return
            # else: degenerate pivot (ratio 0) -> same vertex; try another column.

    # ----------------------------------------------------------------- driver
    def _print_if_verbose(self, phase: str) -> None:
        if self.verbose and self.reporter is not None:
            self.reporter.print_tableau(self.iterations, phase, self.T, self.basis,
                                        self.col_name, self.step_through)

    def solve(self) -> SolveResult:
        self.build_standard_form()
        result = SolveResult()

        if self.artificial_cols:
            feasible = self.run_phase_one()
            if not feasible:
                result.status = SolveStatus.INFEASIBLE
                result.note = "Fase 1 terminou com soma de variaveis artificiais > 0."
                result.iterations = self.iterations
                self.extract_solution(result)  # still show whatever values we have
                return result

        self.set_objective_for_phase_two()
        self.run_phase_two(result)
        result.iterations = self.iterations
        return result
