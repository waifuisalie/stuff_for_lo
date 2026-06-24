// main.cpp -- command-line front end for the SIMPLEX solver.
//
// Usage:
//   ./simplex                 -> enter the problem interactively (console)
//   ./simplex problema.txt    -> load the problem from a file
//   flags (any order):
//     --step      pause between iterations (good for the live demo)
//     --quiet     do not print the tableau at each iteration
//     --help      show this help
//
// The interactive entry and the file format are documented in TUTORIAL.txt.
#include <iostream>
#include <string>

#include "lp_problem.h"
#include "reporter.h"
#include "simplex.h"

namespace {

void printHelp() {
    std::cout <<
        "SIMPLEX - solucionador de Programacao Linear (TDE 2)\n\n"
        "Uso:\n"
        "  ./simplex                 entrada interativa pelo console\n"
        "  ./simplex <arquivo.txt>   carrega o problema de um arquivo\n\n"
        "Opcoes:\n"
        "  --step    pausa entre as iteracoes (demonstracao)\n"
        "  --quiet   nao imprime a tabela a cada iteracao\n"
        "  --help    mostra esta ajuda\n\n"
        "Formato do arquivo (veja TUTORIAL.txt):\n"
        "  max|min\n"
        "  <n> <m>\n"
        "  c1 c2 ... cn\n"
        "  a11 ... a1n  <=|>=|=  b1\n"
        "  ...\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool verbose = true;
    bool stepThrough = false;
    std::string file;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printHelp(); return 0; }
        else if (a == "--step" || a == "--demo") stepThrough = true;
        else if (a == "--quiet") verbose = false;
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "Opcao desconhecida: " << a << "\n";
            return 1;
        } else {
            file = a;  // treat as input file path
        }
    }

    std::cout << "====================================================\n"
              << "   SIMPLEX - Otimizacao de Sistemas Lineares (TDE 2)\n"
              << "====================================================\n";

    LPProblem problem;
    if (!file.empty()) {
        std::string err;
        if (!loadProblemFromFile(file, problem, err)) {
            std::cerr << "Erro ao ler '" << file << "': " << err << "\n";
            return 1;
        }
        std::cout << "Problema carregado de: " << file << "\n";
    } else {
        problem = readProblemFromConsole();
    }

    reporter::printProblem(problem);

    Simplex solver(problem, verbose, stepThrough);
    SolveResult result = solver.solve();

    reporter::printResult(problem, result);
    return 0;
}
