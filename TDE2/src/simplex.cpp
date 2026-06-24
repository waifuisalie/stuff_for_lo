// simplex.cpp -- the SIMPLEX engine (tabular form, two-phase).
//
// Tableau convention (verified by hand on the Wyndor problem):
//   * The objective ("z") row stores  z_j - c_j  for every column, and the
//     current objective value in its RHS cell.
//   * For maximization: the basis is OPTIMAL when every z-row entry is >= 0.
//     The ENTERING variable is the column with the most negative z-row entry.
//   * Minimization is solved by maximizing the negated objective internally;
//     the value we report is recomputed from the ORIGINAL coefficients.
#include "simplex.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "reporter.h"

namespace {
const double EPS = 1e-9;       // "is this number zero?" tolerance
const double TIE_EPS = 1e-7;   // ratio-test tie tolerance (degeneracy signal)
const double FEAS_EPS = 1e-7;  // Phase-1 feasibility tolerance on artificial sum
}  // namespace

Simplex::Simplex(const LPProblem& problem, bool verbose_, bool stepThrough_)
    : orig(problem), n(problem.n), m(problem.m), verbose(verbose_), stepThrough(stepThrough_) {}

// ---------------------------------------------------------------------------
// Standard form: add slack/surplus/artificial columns and pick the initial
// basis. We first make every b_i >= 0 (flipping the relation when we negate a
// row) so that slacks/artificials start at non-negative values.
// ---------------------------------------------------------------------------
void Simplex::buildStandardForm() {
    std::vector<std::vector<double>> A = orig.A;
    std::vector<double> b = orig.b;
    std::vector<ConstraintType> rel = orig.rel;

    for (int r = 0; r < m; ++r) {
        if (b[r] < 0.0) {
            for (int j = 0; j < n; ++j) A[r][j] = -A[r][j];
            b[r] = -b[r];
            if (rel[r] == ConstraintType::LE) rel[r] = ConstraintType::GE;
            else if (rel[r] == ConstraintType::GE) rel[r] = ConstraintType::LE;
            // EQ stays EQ
        }
    }

    int extra = 0;
    for (int r = 0; r < m; ++r) {
        if (rel[r] == ConstraintType::LE) extra += 1;       // slack
        else if (rel[r] == ConstraintType::GE) extra += 2;  // surplus + artificial
        else extra += 1;                                    // artificial
    }

    totalVars = n + extra;
    rhs = totalVars;
    objRow = m;

    T.assign(m + 1, std::vector<double>(totalVars + 1, 0.0));
    kind.assign(totalVars, VarKind::DECISION);
    colName.assign(totalVars, "");
    forbidden.assign(totalVars, false);
    basis.assign(m, -1);

    for (int j = 0; j < n; ++j) {
        kind[j] = VarKind::DECISION;
        colName[j] = "x" + std::to_string(j + 1);
    }
    for (int r = 0; r < m; ++r) {
        for (int j = 0; j < n; ++j) T[r][j] = A[r][j];
        T[r][rhs] = b[r];
    }

    int col = n;
    for (int r = 0; r < m; ++r) {
        if (rel[r] == ConstraintType::LE) {
            kind[col] = VarKind::SLACK;
            colName[col] = "s" + std::to_string(r + 1);
            T[r][col] = 1.0;
            basis[r] = col;
            ++col;
        } else if (rel[r] == ConstraintType::GE) {
            kind[col] = VarKind::SURPLUS;
            colName[col] = "e" + std::to_string(r + 1);
            T[r][col] = -1.0;
            ++col;
            kind[col] = VarKind::ARTIFICIAL;
            colName[col] = "a" + std::to_string(r + 1);
            T[r][col] = 1.0;
            basis[r] = col;
            artificialCols.push_back(col);
            ++col;
        } else {  // EQ
            kind[col] = VarKind::ARTIFICIAL;
            colName[col] = "a" + std::to_string(r + 1);
            T[r][col] = 1.0;
            basis[r] = col;
            artificialCols.push_back(col);
            ++col;
        }
    }
}

// Make the objective row consistent with the current basis: every basic column
// must read 0 in the z row. This is both the per-phase initializer AND the
// critical Phase-1 -> Phase-2 hand-off step ("price out" the real objective).
void Simplex::priceOutObjectiveRow() {
    for (int i = 0; i < m; ++i) {
        int bc = basis[i];
        double f = T[objRow][bc];
        if (std::fabs(f) > EPS) {
            for (int c = 0; c <= rhs; ++c) T[objRow][c] -= f * T[i][c];
        }
    }
}

// Most-negative z-row entry among allowed columns. Ties keep the smallest
// index (an anti-cycling tie-break). Returns -1 when the basis is optimal.
int Simplex::chooseEnteringVariable() {
    int best = -1;
    double mostNeg = -EPS;  // must be strictly more negative than -EPS to enter
    for (int j = 0; j < totalVars; ++j) {
        if (forbidden[j]) continue;
        double rc = T[objRow][j];
        if (rc < mostNeg) {
            mostNeg = rc;
            best = j;
        }
    }
    return best;
}

// Minimum-ratio test on the entering column. Sets `unbounded` if the column has
// no positive entry (the objective can grow without limit). A tie in the
// minimum ratio is recorded as a degeneracy signal.
int Simplex::ratioTest(int enterCol, bool& unbounded) {
    int leave = -1;
    double best = 0.0;
    bool any = false;
    for (int i = 0; i < m; ++i) {
        double a = T[i][enterCol];
        if (a > EPS) {
            double ratio = T[i][rhs] / a;
            if (!any || ratio < best - EPS) {
                best = ratio;
                leave = i;
                any = true;
            }
        }
    }
    if (!any) {
        unbounded = true;
        return -1;
    }
    unbounded = false;

    int ties = 0;
    for (int i = 0; i < m; ++i) {
        double a = T[i][enterCol];
        if (a > EPS && std::fabs(T[i][rhs] / a - best) <= TIE_EPS) ++ties;
    }
    if (ties > 1) tieSeen = true;
    return leave;
}

// Gauss-Jordan pivot on (row, col): normalize the pivot row, then eliminate the
// pivot column from every other row INCLUDING the objective row.
void Simplex::pivot(int row, int col) {
    double p = T[row][col];
    for (int c = 0; c <= rhs; ++c) T[row][c] /= p;
    for (int i = 0; i <= objRow; ++i) {
        if (i == row) continue;
        double f = T[i][col];
        if (std::fabs(f) > EPS) {
            for (int c = 0; c <= rhs; ++c) T[i][c] -= f * T[row][c];
        }
    }
    basis[row] = col;
}

// Phase 1: minimize the sum of artificial variables (== maximize its negative).
// Returns true if the problem is feasible (that sum reaches zero).
bool Simplex::runPhaseOne() {
    // Phase-1 objective: c = -1 for artificials, 0 otherwise -> z row = -c.
    for (int j = 0; j < totalVars; ++j)
        T[objRow][j] = (kind[j] == VarKind::ARTIFICIAL) ? 1.0 : 0.0;
    T[objRow][rhs] = 0.0;
    priceOutObjectiveRow();

    const int maxIter = 50 * (m + totalVars) + 100;
    printIfVerbose("FASE 1");
    while (iterations <= maxIter) {
        int enter = chooseEnteringVariable();
        if (enter == -1) break;  // Phase-1 optimal
        bool unbounded = false;
        int leave = ratioTest(enter, unbounded);
        if (unbounded) break;  // not expected in Phase 1; bail safely
        if (verbose)
            std::cout << "  -> entra " << colName[enter] << ", sai "
                      << colName[basis[leave]] << "\n";
        pivot(leave, enter);
        ++iterations;
        printIfVerbose("FASE 1");
    }

    if (artificialSum() > FEAS_EPS) return false;  // infeasible
    evictArtificialsFromBasis();
    return true;
}

double Simplex::artificialSum() const {
    double s = 0.0;
    for (int i = 0; i < m; ++i)
        if (kind[basis[i]] == VarKind::ARTIFICIAL) s += T[i][rhs];
    return s;
}

// After a feasible Phase 1, any artificial left basic must be at value 0. Try to
// pivot it out in favor of a real variable; if the whole non-artificial part of
// the row is zero, that constraint is redundant and we leave the artificial in.
void Simplex::evictArtificialsFromBasis() {
    for (int i = 0; i < m; ++i) {
        if (kind[basis[i]] != VarKind::ARTIFICIAL) continue;
        int pcol = -1;
        for (int j = 0; j < totalVars; ++j) {
            if (kind[j] == VarKind::ARTIFICIAL) continue;
            if (std::fabs(T[i][j]) > EPS) {
                pcol = j;
                break;
            }
        }
        if (pcol != -1) pivot(i, pcol);
        // else: redundant constraint; artificial stays basic at 0.
    }
}

// Load the real objective (in maximization form) into the z row and forbid the
// artificial columns from ever re-entering the basis, then price it out.
void Simplex::setObjectiveForPhaseTwo() {
    for (int j = 0; j < totalVars; ++j) {
        double cj = 0.0;
        if (kind[j] == VarKind::DECISION)
            cj = (orig.objective == Objective::MAX) ? orig.c[j] : -orig.c[j];
        T[objRow][j] = -cj;
    }
    T[objRow][rhs] = 0.0;
    for (int j = 0; j < totalVars; ++j)
        if (kind[j] == VarKind::ARTIFICIAL) forbidden[j] = true;
    priceOutObjectiveRow();
}

void Simplex::runPhaseTwo(SolveResult& result) {
    const int maxIter = 50 * (m + totalVars) + 100;
    // Only ratio-test ties that happen in Phase 2 say anything about whether the
    // FINAL vertex is degenerate; Phase-1 ties are common and irrelevant here.
    tieSeen = false;
    printIfVerbose("FASE 2");
    while (true) {
        int enter = chooseEnteringVariable();
        if (enter == -1) {
            result.status = SolveStatus::OPTIMAL;
            break;
        }
        bool unbounded = false;
        int leave = ratioTest(enter, unbounded);
        if (unbounded) {
            result.status = SolveStatus::UNBOUNDED;
            result.note = "Coluna de entrada (" + colName[enter] +
                          ") nao possui razao positiva: problema sem fronteira.";
            break;
        }
        if (verbose)
            std::cout << "  -> entra " << colName[enter] << ", sai "
                      << colName[basis[leave]] << "\n";
        pivot(leave, enter);
        ++iterations;
        printIfVerbose("FASE 2");
        if (iterations > maxIter) {
            result.status = SolveStatus::ITERATION_LIMIT;
            result.note = "Limite de iteracoes atingido (possivel ciclagem). "
                          "Resposta intermediaria.";
            break;
        }
    }

    detectDegeneracy(result);
    extractSolution(result);
    if (result.status == SolveStatus::OPTIMAL) detectAlternateOptima(result);
}

void Simplex::detectDegeneracy(SolveResult& result) {
    for (int i = 0; i < m; ++i) {
        if (std::fabs(T[i][rhs]) < EPS) {  // a basic variable sits at zero
            result.degenerate = true;
            result.degenerateVars.push_back(colName[basis[i]]);
        }
    }
    if (tieSeen) result.degenerate = true;
}

// Recover the decision-variable values and the ORIGINAL objective value. Called
// for EVERY terminal state so even an interrupted answer shows variable values.
void Simplex::extractSolution(SolveResult& result) {
    result.variableValues.assign(n, 0.0);
    for (int i = 0; i < m; ++i) {
        int bc = basis[i];
        if (bc >= 0 && bc < n) result.variableValues[bc] = T[i][rhs];
    }
    double z = 0.0;
    for (int j = 0; j < n; ++j) z += orig.c[j] * result.variableValues[j];
    result.objectiveValue = z;
}

// A non-basic, non-artificial variable with zero reduced cost at the optimum
// means another optimal vertex exists. We flag it and -- to "show the path" --
// pivot it in to expose the second optimal vertex.
void Simplex::detectAlternateOptima(SolveResult& result) {
    std::vector<bool> isBasic(totalVars, false);
    for (int i = 0; i < m; ++i) isBasic[basis[i]] = true;

    // A zero reduced cost on a non-basic variable only means alternate optima if
    // we can actually REACH a different optimal point with it. If the only pivot
    // it allows is degenerate (ratio 0, same vertex), that is degeneracy, not an
    // alternate optimum -- so we keep looking and do not flag it here.
    for (int j = 0; j < totalVars; ++j) {
        if (forbidden[j] || isBasic[j]) continue;
        if (std::fabs(T[objRow][j]) >= EPS) continue;  // reduced cost not zero

        bool unbounded = false;
        int leave = ratioTest(j, unbounded);
        if (unbounded) {
            // Z stays constant along an unbounded edge -> infinitely many optima.
            result.alternateOptima = true;
            return;
        }
        if (leave == -1) continue;

        double minRatio = T[leave][rhs] / T[leave][j];
        if (minRatio > EPS) {
            // Bringing j in moves us to a genuinely different optimal vertex.
            result.alternateOptima = true;
            pivot(leave, j);
            std::vector<double> alt(n, 0.0);
            for (int i = 0; i < m; ++i)
                if (basis[i] >= 0 && basis[i] < n) alt[basis[i]] = T[i][rhs];
            result.alternateVertex = alt;
            return;
        }
        // else: degenerate pivot (ratio 0) -> same vertex; try another column.
    }
}

void Simplex::printIfVerbose(const std::string& phase) {
    if (verbose) reporter::printTableau(iterations, phase, T, basis, colName, stepThrough);
}

SolveResult Simplex::solve() {
    buildStandardForm();

    SolveResult result;

    if (!artificialCols.empty()) {
        bool feasible = runPhaseOne();
        if (!feasible) {
            result.status = SolveStatus::INFEASIBLE;
            result.note = "Fase 1 terminou com soma de variaveis artificiais > 0.";
            result.iterations = iterations;
            extractSolution(result);  // still show whatever values we have
            return result;
        }
    }

    setObjectiveForPhaseTwo();
    runPhaseTwo(result);
    result.iterations = iterations;
    return result;
}
