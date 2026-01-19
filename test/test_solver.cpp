#include <cassert>
#include <iostream>
#include "../src/solver.hpp"

void test_simple_sat() {
    std::cout << "Testing simple SAT problem...\n";

    // Simple satisfiable problem: (x1 OR x2) AND (NOT x1 OR x2)
    // Solution: x2 = true
    std::vector<std::vector<int>> clauses = {
        {1, 2},    // x1 OR x2
        {-1, 2}    // NOT x1 OR x2
    };

    SolverResult result = solve(clauses, 2);

    std::cout << "  Status: ";
    switch (result.status) {
        case SolverStatus::SAT: std::cout << "SAT\n"; break;
        case SolverStatus::UNSAT: std::cout << "UNSAT\n"; break;
        case SolverStatus::ERROR: std::cout << "ERROR: " << result.error_message << "\n"; break;
    }

    assert(result.status == SolverStatus::SAT);
    // x2 must be true
    assert(result.solution.count(2) > 0);

    std::cout << "  Solution: ";
    for (int lit : result.solution) {
        std::cout << lit << " ";
    }
    std::cout << "\n";

    std::cout << "PASSED: test_simple_sat\n";
}

void test_simple_unsat() {
    std::cout << "Testing simple UNSAT problem...\n";

    // Unsatisfiable: (x1) AND (NOT x1)
    std::vector<std::vector<int>> clauses = {
        {1},
        {-1}
    };

    SolverResult result = solve(clauses, 1);

    std::cout << "  Status: ";
    switch (result.status) {
        case SolverStatus::SAT: std::cout << "SAT\n"; break;
        case SolverStatus::UNSAT: std::cout << "UNSAT\n"; break;
        case SolverStatus::ERROR: std::cout << "ERROR: " << result.error_message << "\n"; break;
    }

    assert(result.status == SolverStatus::UNSAT);

    std::cout << "PASSED: test_simple_unsat\n";
}

void test_dimacs_generation() {
    std::cout << "Testing DIMACS generation...\n";

    std::vector<std::vector<int>> clauses = {
        {1, 2, 3},
        {-1, -2},
        {3}
    };

    std::string dimacs = make_dimacs_string(clauses, 3);
    std::cout << "  Generated DIMACS:\n" << dimacs;

    assert(dimacs.find("p cnf 3 3") != std::string::npos);
    assert(dimacs.find("1 2 3 0") != std::string::npos);
    assert(dimacs.find("-1 -2 0") != std::string::npos);

    std::cout << "PASSED: test_dimacs_generation\n";
}

int main() {
    test_dimacs_generation();
    test_simple_sat();
    test_simple_unsat();

    std::cout << "\nAll solver tests passed!\n";
    return 0;
}
