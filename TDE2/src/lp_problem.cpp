// lp_problem.cpp  -- problem model + the two input paths (file and console).
#include "lp_problem.h"

#include <fstream>
#include <iostream>
#include <sstream>

const char* constraintSymbol(ConstraintType t) {
    switch (t) {
        case ConstraintType::LE: return "<=";
        case ConstraintType::GE: return ">=";
        case ConstraintType::EQ: return "=";
    }
    return "?";
}

// Turn a relation token from the input into a ConstraintType.
// Accepts the common spellings so the file format is forgiving.
static bool parseRelation(const std::string& tok, ConstraintType& out) {
    if (tok == "<=" || tok == "<") { out = ConstraintType::LE; return true; }
    if (tok == ">=" || tok == ">") { out = ConstraintType::GE; return true; }
    if (tok == "="  || tok == "==") { out = ConstraintType::EQ; return true; }
    return false;
}

static bool parseObjective(const std::string& tok, Objective& out) {
    if (tok == "max" || tok == "MAX" || tok == "maximize") { out = Objective::MAX; return true; }
    if (tok == "min" || tok == "MIN" || tok == "minimize") { out = Objective::MIN; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// File input.
//
// We first flatten the file into a stream of tokens, dropping blank lines and
// anything after a '#' comment marker. Then we consume tokens in order:
//
//   <max|min>
//   <n> <m>
//   c1 c2 ... cn
//   (for each of m constraints)  a1 a2 ... an  <rel>  b
// ---------------------------------------------------------------------------
bool loadProblemFromFile(const std::string& path, LPProblem& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "could not open file: " + path; return false; }

    std::vector<std::string> tok;
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments: everything from '#' to end of line.
        std::size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream ls(line);
        std::string t;
        while (ls >> t) tok.push_back(t);
    }

    std::size_t i = 0;
    auto next = [&](std::string& dst) -> bool {
        if (i >= tok.size()) return false;
        dst = tok[i++];
        return true;
    };

    std::string s;
    if (!next(s) || !parseObjective(s, out.objective)) {
        err = "expected 'max' or 'min' as the first token"; return false;
    }

    std::string sn, sm;
    if (!next(sn) || !next(sm)) { err = "expected 'n m' (variable and constraint counts)"; return false; }
    try {
        out.n = std::stoi(sn);
        out.m = std::stoi(sm);
    } catch (...) { err = "n and m must be integers"; return false; }
    if (out.n <= 0 || out.m <= 0) { err = "n and m must be positive"; return false; }

    out.c.resize(out.n);
    for (int j = 0; j < out.n; ++j) {
        if (!next(s)) { err = "not enough objective coefficients"; return false; }
        try { out.c[j] = std::stod(s); } catch (...) { err = "bad objective coefficient: " + s; return false; }
    }

    out.A.assign(out.m, std::vector<double>(out.n, 0.0));
    out.rel.resize(out.m);
    out.b.resize(out.m);
    for (int r = 0; r < out.m; ++r) {
        for (int j = 0; j < out.n; ++j) {
            if (!next(s)) { err = "not enough coefficients for constraint " + std::to_string(r + 1); return false; }
            try { out.A[r][j] = std::stod(s); } catch (...) { err = "bad coefficient: " + s; return false; }
        }
        if (!next(s) || !parseRelation(s, out.rel[r])) {
            err = "expected a relation (<=, >=, =) in constraint " + std::to_string(r + 1); return false;
        }
        if (!next(s)) { err = "missing right-hand side in constraint " + std::to_string(r + 1); return false; }
        try { out.b[r] = std::stod(s); } catch (...) { err = "bad right-hand side: " + s; return false; }
    }

    return validateProblem(out, err);
}

// ---------------------------------------------------------------------------
// Console input (guided). Used when the program is launched with no file.
// ---------------------------------------------------------------------------
LPProblem readProblemFromConsole() {
    LPProblem p;
    std::string s;

    std::cout << "Objetivo (max/min): ";
    std::cin >> s;
    Objective obj;
    while (!parseObjective(s, obj)) {
        std::cout << "  digite 'max' ou 'min': ";
        std::cin >> s;
    }
    p.objective = obj;

    std::cout << "Numero de variaveis de decisao (n): ";
    std::cin >> p.n;
    std::cout << "Numero de restricoes (m): ";
    std::cin >> p.m;

    p.c.resize(p.n);
    std::cout << "Coeficientes da funcao objetivo (c1..c" << p.n << "):\n";
    for (int j = 0; j < p.n; ++j) {
        std::cout << "  c" << (j + 1) << " = ";
        std::cin >> p.c[j];
    }

    p.A.assign(p.m, std::vector<double>(p.n, 0.0));
    p.rel.resize(p.m);
    p.b.resize(p.m);
    for (int r = 0; r < p.m; ++r) {
        std::cout << "Restricao " << (r + 1) << ":\n";
        for (int j = 0; j < p.n; ++j) {
            std::cout << "  a" << (r + 1) << (j + 1) << " = ";
            std::cin >> p.A[r][j];
        }
        std::cout << "  relacao (<=, >=, =): ";
        std::cin >> s;
        ConstraintType ct;
        while (!parseRelation(s, ct)) {
            std::cout << "    use <=, >= ou = : ";
            std::cin >> s;
        }
        p.rel[r] = ct;
        std::cout << "  b" << (r + 1) << " = ";
        std::cin >> p.b[r];
    }
    return p;
}

bool validateProblem(const LPProblem& p, std::string& err) {
    if (p.n <= 0 || p.m <= 0) { err = "n and m must be positive"; return false; }
    if ((int)p.c.size() != p.n) { err = "objective coefficient count != n"; return false; }
    if ((int)p.A.size() != p.m) { err = "constraint row count != m"; return false; }
    if ((int)p.rel.size() != p.m || (int)p.b.size() != p.m) { err = "relation/RHS count != m"; return false; }
    for (int r = 0; r < p.m; ++r) {
        if ((int)p.A[r].size() != p.n) {
            err = "constraint " + std::to_string(r + 1) + " has wrong coefficient count"; return false;
        }
    }
    return true;
}
