#!/usr/bin/env python3
"""main.py -- command-line front end for the SIMPLEX solver (Python version).

Usage:
  python main.py                 enter the problem interactively (console)
  python main.py problema.txt    load the problem from a file
  flags (any order):
    --step      pause between iterations (good for the live demo)
    --quiet     do not print the tableau at each iteration
    --help      show this help

The interactive entry and the file format are documented in TUTORIAL.txt.
"""
import sys

import lp_problem
import reporter
from simplex import Simplex

HELP = """SIMPLEX - solucionador de Programacao Linear (TDE 2) - versao Python

Uso:
  python main.py                 entrada interativa pelo console
  python main.py <arquivo.txt>   carrega o problema de um arquivo

Opcoes:
  --step    pausa entre as iteracoes (demonstracao)
  --quiet   nao imprime a tabela a cada iteracao
  --help    mostra esta ajuda

Formato do arquivo (veja TUTORIAL.txt):
  max|min
  <n> <m>
  c1 c2 ... cn
  a11 ... a1n  <=|>=|=  b1
  ...
"""


def main(argv) -> int:
    verbose = True
    step_through = False
    file = None

    for a in argv[1:]:
        if a in ("--help", "-h"):
            print(HELP)
            return 0
        elif a in ("--step", "--demo"):
            step_through = True
        elif a == "--quiet":
            verbose = False
        elif a.startswith("-"):
            print(f"Opcao desconhecida: {a}", file=sys.stderr)
            return 1
        else:
            file = a  # treat as input file path

    print("====================================================")
    print("   SIMPLEX - Otimizacao de Sistemas Lineares (TDE 2)")
    print("   (versao Python)")
    print("====================================================")

    try:
        if file:
            problem = lp_problem.load_problem_from_file(file)
            print(f"Problema carregado de: {file}")
        else:
            problem = lp_problem.read_problem_from_console()
    except (lp_problem.ParseError, ValueError) as e:
        print(f"Erro na entrada: {e}", file=sys.stderr)
        return 1

    reporter.print_problem(problem)

    solver = Simplex(problem, verbose, step_through, reporter=reporter)
    result = solver.solve()

    reporter.print_result(problem, result)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
