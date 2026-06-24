// simplex.h
// The SIMPLEX engine. One class, Simplex, that:
//   * builds the standard-form tableau (slack / surplus / artificial columns),
//   * runs Phase 1 (only when artificial variables are needed),
//   * runs Phase 2 to optimality,
//   * and reports the "accidents": degeneracy, infeasibility, unboundedness,
//     alternate optima.
//
// Minimization is handled by maximizing the negated objective internally; the
// reported objective value is always computed back from the original problem.
#ifndef SIMPLEX_H
#define SIMPLEX_H

#include <string>
#include <vector>

#include "lp_problem.h"

// How the solve ended. Requirement #4 asks us to make the difference between a
// final and an interrupted ("accident") answer explicit -- this enum is how.
enum class SolveStatus { OPTIMAL, INFEASIBLE, UNBOUNDED, ITERATION_LIMIT };

// What a tableau column represents. Used for naming and for forbidding
// artificial columns from re-entering the basis during Phase 2.
enum class VarKind { DECISION, SLACK, SURPLUS, ARTIFICIAL };

// Everything the reporter needs to present an answer for ANY terminal state.
struct SolveResult {
    SolveStatus status = SolveStatus::OPTIMAL;
    double objectiveValue = 0.0;             // value of the ORIGINAL objective
    std::vector<double> variableValues;      // decision variable values (size n)
    int iterations = 0;                      // total pivots across both phases
    bool degenerate = false;                 // a basic variable sits at zero, or a ratio-test tie occurred
    bool alternateOptima = false;            // another optimal vertex exists
    std::vector<std::string> degenerateVars; // names of basic variables equal to zero
    std::vector<double> alternateVertex;     // a second optimal vertex (size n), empty if none
    std::string note;                        // free-form human-readable explanation
};

class Simplex {
public:
    // verbose      -> print the tableau at every iteration (requirement #3)
    // stepThrough  -> pause for <Enter> between iterations (nice for the live demo)
    Simplex(const LPProblem& problem, bool verbose, bool stepThrough);

    SolveResult solve();

private:
    // ----- the tableau -----
    // T has (m+1) rows: rows 0..m-1 are constraints, row m is the objective
    // ("z") row. Each row has (totalVars + 1) columns; the last column is RHS.
    std::vector<std::vector<double>> T;
    std::vector<int> basis;            // basis[i] = column basic in constraint row i
    std::vector<VarKind> kind;         // kind[j] for each variable column
    std::vector<std::string> colName;  // display name for each variable column
    std::vector<bool> forbidden;       // column excluded from entering (artificials in Phase 2)

    const LPProblem& orig;
    int n = 0;          // original decision variables
    int m = 0;          // constraints
    int totalVars = 0;  // decision + slack + surplus + artificial
    int rhs = 0;        // index of the RHS column (== totalVars)
    int objRow = 0;     // index of the objective row (== m)

    bool verbose;
    bool stepThrough;
    int iterations = 0;
    bool tieSeen = false;     // a ratio-test tie happened somewhere (degeneracy signal)

    std::vector<int> artificialCols;  // indices of artificial columns (for Phase-1 feasibility check)

    // ----- construction -----
    void buildStandardForm();

    // ----- core simplex steps -----
    // Make the objective row consistent with the current basis (price-out):
    // every basic column reads 0 in the objective row. Used to initialize a
    // phase and -- critically -- at the Phase-1 -> Phase-2 hand-off.
    void priceOutObjectiveRow();

    int  chooseEnteringVariable();                 // -1 if optimal
    int  ratioTest(int enterCol, bool& unbounded); // leaving row, or -1 if unbounded
    void pivot(int row, int col);

    // ----- phases -----
    // Returns true if feasible (Phase 1 reached zero artificial sum).
    bool runPhaseOne();
    void runPhaseTwo(SolveResult& result);

    // ----- helpers / accident detection -----
    void setObjectiveForPhaseTwo();      // load real (max-form) objective into the z row
    double artificialSum() const;        // sum of current values of artificial variables
    void evictArtificialsFromBasis();    // drive zero-valued artificials out after Phase 1
    void detectDegeneracy(SolveResult& result);
    void detectAlternateOptima(SolveResult& result);
    void extractSolution(SolveResult& result);  // recover x and the original objective value
    void printIfVerbose(const std::string& phase);
};

#endif  // SIMPLEX_H
