// reporter.h
// All user-facing output lives here so the solver stays focused on the math.
//  * printProblem  -- echo the LP that was read in.
//  * printTableau  -- one iteration of the tableau (requirement #3).
//  * printResult   -- the uniform final block, used for EVERY terminal state
//                     (optimal / infeasible / unbounded / iteration-limit), so
//                     even an interrupted answer still shows variable values
//                     and a clear status label (requirement #4).
#ifndef REPORTER_H
#define REPORTER_H

#include <string>
#include <vector>

#include "lp_problem.h"
#include "simplex.h"

namespace reporter {

void printProblem(const LPProblem& p);

void printTableau(int iteration,
                  const std::string& phase,
                  const std::vector<std::vector<double>>& T,
                  const std::vector<int>& basis,
                  const std::vector<std::string>& colName,
                  bool stepThrough);

void printResult(const LPProblem& p, const SolveResult& r);

}  // namespace reporter

#endif  // REPORTER_H
