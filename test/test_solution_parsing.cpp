#include <iostream>
#include <cassert>
#include "../src/variable_grid.cpp"
#include "../src/solver.hpp"

// Test that SAT solutions are parsed back to the grid correctly.
// We set gen 0 to a known still life (boat), gen 1 to all unknown,
// solve, and verify gen 1 matches gen 0.

// Boat pattern (a still life):
// oo.
// o.o
// .o.

VariablePattern create_boat_pattern() {
    // 3x3 grid, 2 generations
    VariablePattern pattern(3, 3, 1);

    // Gen 0: set up the boat (known cells)
    // (0,0)=alive, (1,0)=alive, (2,0)=dead
    // (0,1)=alive, (1,1)=dead,  (2,1)=alive
    // (0,2)=dead,  (1,2)=alive, (2,2)=dead
    pattern.set_alive({0, 0, 0});
    pattern.set_alive({1, 0, 0});
    pattern.set_dead({2, 0, 0});
    pattern.set_alive({0, 1, 0});
    pattern.set_dead({1, 1, 0});
    pattern.set_alive({2, 1, 0});
    pattern.set_dead({0, 2, 0});
    pattern.set_alive({1, 2, 0});
    pattern.set_dead({2, 2, 0});

    // Gen 1: all unknown (default)
    // No changes needed - cells start as unknown

    return pattern;
}

// Extract the state of a cell from solver result
bool get_cell_state(const VariableGrid& grid, const SolverResult& result, int x, int y, int t) {
    int var_idx = grid.grid[t][y][x];
    if (var_idx == 0) return false;  // known dead
    if (var_idx == 1) return true;   // known alive
    int sat_var = var_idx - 1;
    return result.solution.count(sat_var) > 0;
}

void test_boat_still_life() {
    std::cout << "Testing boat still life solution parsing...\n";

    VariablePattern pattern = create_boat_pattern();
    VariableGrid var_grid = construct_variable_grid(pattern);

    std::cout << "Variable grid:\n";
    print_variable_grid(var_grid);

    int num_vars = 0;
    auto clauses = calculate_clauses(var_grid, num_vars);
    std::cout << "  " << clauses.size() << " clauses, " << num_vars << " variables\n";

    SolverResult result = solve(clauses, num_vars);
    assert(result.status == SolverStatus::SAT);
    std::cout << "  Solver returned SAT\n";

    // Verify gen 1 matches gen 0 (boat is a still life)
    // Gen 0 pattern:
    // (0,0)=1, (1,0)=1, (2,0)=0
    // (0,1)=1, (1,1)=0, (2,1)=1
    // (0,2)=0, (1,2)=1, (2,2)=0
    bool expected[3][3] = {
        {true,  true,  false},  // y=0
        {true,  false, true},   // y=1
        {false, true,  false}   // y=2
    };

    std::cout << "  Checking gen 1 matches expected boat pattern:\n";
    bool all_match = true;
    for (int y = 0; y < 3; y++) {
        std::cout << "    y=" << y << ": ";
        for (int x = 0; x < 3; x++) {
            bool actual = get_cell_state(var_grid, result, x, y, 1);
            bool exp = expected[y][x];
            char c = actual ? 'o' : '.';
            std::cout << c;
            if (actual != exp) {
                all_match = false;
                std::cout << "(expected " << (exp ? 'o' : '.') << ")";
            }
        }
        std::cout << "\n";
    }

    assert(all_match && "Gen 1 should match boat pattern (still life)");
    std::cout << "PASSED: test_boat_still_life\n";
}

void test_blinker_oscillator() {
    std::cout << "\nTesting blinker oscillator solution parsing...\n";

    // Blinker oscillates between horizontal and vertical:
    // Gen 0:  Gen 1:
    // .o.     ...
    // .o.     ooo
    // .o.     ...

    VariablePattern pattern(3, 3, 1);

    // Gen 0: vertical blinker
    pattern.set_dead({0, 0, 0});
    pattern.set_alive({1, 0, 0});
    pattern.set_dead({2, 0, 0});
    pattern.set_dead({0, 1, 0});
    pattern.set_alive({1, 1, 0});
    pattern.set_dead({2, 1, 0});
    pattern.set_dead({0, 2, 0});
    pattern.set_alive({1, 2, 0});
    pattern.set_dead({2, 2, 0});

    // Gen 1: all unknown

    VariableGrid var_grid = construct_variable_grid(pattern);

    std::cout << "Variable grid:\n";
    print_variable_grid(var_grid);

    int num_vars = 0;
    auto clauses = calculate_clauses(var_grid, num_vars);
    std::cout << "  " << clauses.size() << " clauses, " << num_vars << " variables\n";

    SolverResult result = solve(clauses, num_vars);
    assert(result.status == SolverStatus::SAT);
    std::cout << "  Solver returned SAT\n";

    // Gen 1 should be horizontal blinker:
    // (0,0)=0, (1,0)=0, (2,0)=0
    // (0,1)=1, (1,1)=1, (2,1)=1
    // (0,2)=0, (1,2)=0, (2,2)=0
    bool expected[3][3] = {
        {false, false, false},  // y=0
        {true,  true,  true},   // y=1
        {false, false, false}   // y=2
    };

    std::cout << "  Checking gen 1 matches expected horizontal blinker:\n";
    bool all_match = true;
    for (int y = 0; y < 3; y++) {
        std::cout << "    y=" << y << ": ";
        for (int x = 0; x < 3; x++) {
            bool actual = get_cell_state(var_grid, result, x, y, 1);
            bool exp = expected[y][x];
            char c = actual ? 'o' : '.';
            std::cout << c;
            if (actual != exp) {
                all_match = false;
                std::cout << "(expected " << (exp ? 'o' : '.') << ")";
            }
        }
        std::cout << "\n";
    }

    assert(all_match && "Gen 1 should be horizontal blinker");
    std::cout << "PASSED: test_blinker_oscillator\n";
}

int main() {
    test_boat_still_life();
    test_blinker_oscillator();

    std::cout << "\nAll solution parsing tests passed!\n";
    return 0;
}
