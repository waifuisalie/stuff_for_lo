// reporter.cpp -- all user-facing output.
#include "reporter.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

// Print a number in a fixed-width cell. Values that are (numerically) integers
// are shown without a decimal part to keep the tableau readable.
std::string cell(double v) {
    if (std::fabs(v) < 1e-9) v = 0.0;  // avoid "-0"
    std::ostringstream os;
    if (std::fabs(v - std::llround(v)) < 1e-6)
        os << std::llround(v);
    else
        os << std::fixed << std::setprecision(3) << v;
    return os.str();
}

}  // namespace

namespace reporter {

void printProblem(const LPProblem& p) {
    std::cout << "\n==================== PROBLEMA ====================\n";
    std::cout << (p.objective == Objective::MAX ? "Maximizar" : "Minimizar") << "  Z =";
    for (int j = 0; j < p.n; ++j) {
        std::cout << (j == 0 ? " " : " + ") << cell(p.c[j]) << "*x" << (j + 1);
    }
    std::cout << "\nsujeito a:\n";
    for (int r = 0; r < p.m; ++r) {
        std::cout << "  ";
        for (int j = 0; j < p.n; ++j)
            std::cout << (j == 0 ? "" : " + ") << cell(p.A[r][j]) << "*x" << (j + 1);
        std::cout << " " << constraintSymbol(p.rel[r]) << " " << cell(p.b[r]) << "\n";
    }
    std::cout << "  x_j >= 0\n";
    std::cout << "==================================================\n";
}

void printTableau(int iteration,
                  const std::string& phase,
                  const std::vector<std::vector<double>>& T,
                  const std::vector<int>& basis,
                  const std::vector<std::string>& colName,
                  bool stepThrough) {
    int m = (int)basis.size();
    int totalVars = (int)colName.size();
    int rhs = totalVars;
    int objRow = m;
    const int W = 9;

    std::cout << "\n--- " << phase << " | Iteracao " << iteration << " ---\n";
    std::cout << std::setw(7) << "Base";
    for (int j = 0; j < totalVars; ++j) std::cout << std::setw(W) << colName[j];
    std::cout << std::setw(W) << "RHS" << "\n";

    for (int i = 0; i < m; ++i) {
        std::cout << std::setw(7) << colName[basis[i]];
        for (int j = 0; j <= rhs; ++j) std::cout << std::setw(W) << cell(T[i][j]);
        std::cout << "\n";
    }
    std::cout << std::setw(7) << "Z";
    for (int j = 0; j <= rhs; ++j) std::cout << std::setw(W) << cell(T[objRow][j]);
    std::cout << "\n";

    if (stepThrough) {
        std::cout << "[Enter para continuar]";
        std::cout.flush();
        std::string dummy;
        std::getline(std::cin, dummy);
    }
}

void printResult(const LPProblem& p, const SolveResult& r) {
    std::cout << "\n==================== RESULTADO ====================\n";

    bool isFinal = (r.status == SolveStatus::OPTIMAL);
    switch (r.status) {
        case SolveStatus::OPTIMAL:
            std::cout << "STATUS: SOLUCAO OTIMA ENCONTRADA (resposta final)\n";
            break;
        case SolveStatus::INFEASIBLE:
            std::cout << "STATUS: PROBLEMA INVIAVEL (nao existe solucao factivel)\n";
            break;
        case SolveStatus::UNBOUNDED:
            std::cout << "STATUS: PROBLEMA SEM FRONTEIRA / ILIMITADO\n";
            break;
        case SolveStatus::ITERATION_LIMIT:
            std::cout << "STATUS: INTERROMPIDO - limite de iteracoes (resposta intermediaria)\n";
            break;
    }

    std::cout << "Numero de iteracoes: " << r.iterations << "\n";

    if (isFinal)
        std::cout << "Valor otimo  Z = " << cell(r.objectiveValue) << "\n";
    else
        std::cout << "Valor atual da funcao objetivo (intermediario) Z = "
                  << cell(r.objectiveValue) << "\n";

    std::cout << "Valores das variaveis"
              << (isFinal ? ":" : " (estado atual / intermediario):") << "\n";
    for (int j = 0; j < p.n; ++j)
        std::cout << "  x" << (j + 1) << " = " << cell(r.variableValues[j]) << "\n";

    if (r.degenerate) {
        std::cout << "[DEGENERACAO] O problema apresenta degeneracao";
        if (!r.degenerateVars.empty()) {
            std::cout << " (variaveis basicas nulas:";
            for (const auto& v : r.degenerateVars) std::cout << " " << v;
            std::cout << ")";
        }
        std::cout << ".\n";
    }

    if (r.alternateOptima) {
        std::cout << "[OTIMOS ALTERNADOS] Existe mais de uma solucao otima.\n";
        if (!r.alternateVertex.empty()) {
            std::cout << "  Vertice otimo alternativo:";
            for (int j = 0; j < p.n; ++j)
                std::cout << " x" << (j + 1) << "=" << cell(r.alternateVertex[j]);
            std::cout << "\n";
        }
    }

    if (!r.note.empty()) std::cout << "Obs: " << r.note << "\n";
    std::cout << "===================================================\n";
}

}  // namespace reporter
