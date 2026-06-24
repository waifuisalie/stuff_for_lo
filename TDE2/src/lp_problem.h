// lp_problem.h
// Representation of a Linear Programming problem in its ORIGINAL (user) form,
// plus the two ways the user can feed a problem in: a text file or the console.
//
// This is deliberately just the "model": objective sense, the objective
// coefficients, and the constraint rows. All SIMPLEX machinery lives in
// simplex.h/.cpp so that this file stays easy to read and explain.
#ifndef LP_PROBLEM_H
#define LP_PROBLEM_H

#include <string>
#include <vector>

// Objective sense chosen by the user.
enum class Objective { MAX, MIN };

// The relation of a single constraint:  <= ,  >=  or  = .
enum class ConstraintType { LE, GE, EQ };

// An LP in original form:
//
//   max / min   Z = c[0]*x1 + c[1]*x2 + ... + c[n-1]*xn
//   subject to  A[i] . x   (rel[i])   b[i]      for i = 0..m-1
//               x >= 0
struct LPProblem {
    Objective objective = Objective::MAX;
    int n = 0;                              // number of decision variables
    int m = 0;                              // number of constraints
    std::vector<double> c;                  // objective coefficients      (size n)
    std::vector<std::vector<double>> A;     // constraint coefficients     (m x n)
    std::vector<ConstraintType> rel;        // constraint relations        (size m)
    std::vector<double> b;                  // right-hand sides            (size m)
};

// Read a problem from a text file (format documented in TUTORIAL.txt).
// Returns true on success; on failure returns false and fills `err`.
bool loadProblemFromFile(const std::string& path, LPProblem& out, std::string& err);

// Read a problem interactively from stdin (guided prompts).
LPProblem readProblemFromConsole();

// Basic sanity checks on dimensions/sizes. Returns true if OK.
bool validateProblem(const LPProblem& p, std::string& err);

// Helpers used by the reporter / parser.
const char* constraintSymbol(ConstraintType t);   // "<=", ">=", "="

#endif  // LP_PROBLEM_H
