#include <cassert>
#include <iostream>
#include <bitset>
#include "../src/variable_grid.cpp"

// B3/S23 truth table for a single transition
// x is 10 bits: bits 0-8 are neighborhood (bit 4 is center), bit 9 is next gen
bool is_valid_b3s23(int x) {
    int neighborhood = x & 511;  // bits 0-8
    int next_gen = (x >> 9) & 1; // bit 9

    int center = (neighborhood >> 4) & 1;
    int neighbor_count = __builtin_popcount(neighborhood & 0b111101111); // exclude center

    int expected_next;
    if (center == 1) {
        // Survival: 2 or 3 neighbors
        expected_next = (neighbor_count == 2 || neighbor_count == 3) ? 1 : 0;
    } else {
        // Birth: exactly 3 neighbors
        expected_next = (neighbor_count == 3) ? 1 : 0;
    }

    return next_gen == expected_next;
}

// Compute expected next gen state from B3/S23 rules
int b3s23_next_gen(int neighborhood_9bit) {
    int center = (neighborhood_9bit >> 4) & 1;
    int neighbor_count = __builtin_popcount(neighborhood_9bit & 0b111101111);
    if (center == 1) {
        return (neighbor_count == 2 || neighbor_count == 3) ? 1 : 0;
    } else {
        return (neighbor_count == 3) ? 1 : 0;
    }
}

// Check if a 10-bit assignment is valid for the full 3x3 grid evolution
// where t=1 edges are known dead and only the center is unknown.
// This must check ALL 9 transitions.
bool is_valid_full_evolution(int assignment) {
    // assignment bits 0-8: t=0 grid in row-major order (grid[y][x] = bit[x + 3*y])
    // assignment bit 9: t=1 center

    // t=1 grid: edges are 0 (dead), center is bit 9

    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            // Gather 3x3 neighborhood at t=0 around (x, y)
            int neighborhood = 0;
            int bit_idx = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    int cell_state = 0;
                    if (nx >= 0 && nx < 3 && ny >= 0 && ny < 3) {
                        int src_bit = nx + 3 * ny;
                        cell_state = (assignment >> src_bit) & 1;
                    }
                    // boundary cells are 0 (dead)
                    neighborhood |= (cell_state << bit_idx);
                    bit_idx++;
                }
            }

            int expected_next = b3s23_next_gen(neighborhood);

            // Actual next gen at (x, y)
            int actual_next;
            if (x == 1 && y == 1) {
                actual_next = (assignment >> 9) & 1;  // center is unknown
            } else {
                actual_next = 0;  // edges are known dead
            }

            if (expected_next != actual_next) {
                return false;
            }
        }
    }
    return true;
}

// Check if an assignment satisfies a single clause
// assignment is indexed by variable number (1-based)
bool satisfies_clause(const std::vector<int>& clause, const std::vector<bool>& assignment) {
    for (int lit : clause) {
        int var = std::abs(lit);
        bool val = assignment[var];
        if (lit > 0 && val) return true;
        if (lit < 0 && !val) return true;
    }
    return false;
}

// Check if an assignment satisfies all clauses
bool satisfies_all_clauses(const std::vector<std::vector<int>>& clauses,
                           const std::vector<bool>& assignment) {
    for (const auto& clause : clauses) {
        if (!satisfies_clause(clause, assignment)) {
            return false;
        }
    }
    return true;
}

// Create a 3x3 variable grid with 2 time steps
// - 9 cells at t=0 (indices 2-10, SAT vars 1-9)
// - 1 cell at t=1 center (index 11, SAT var 10)
// - Only center cell at t=1 follows the rule
VariableGrid create_test_grid_all_unknown() {
    VariableGrid grid;
    // 3x3, 2 time steps - grid always starts at (0,0,0)

    // grid[t][y][x]
    grid.grid = {
        // t=0: all cells are unknown variables 2-10
        {
            {2, 3, 4},      // y=0: var indices 2,3,4
            {5, 6, 7},      // y=1: var indices 5,6,7
            {8, 9, 10}      // y=2: var indices 8,9,10
        },
        // t=1: center is unknown var 11, edges are 0 (dead)
        {
            {0, 0, 0},
            {0, 11, 0},
            {0, 0, 0}
        }
    };

    // Only enforce rule for center cell at t=1
    grid.follows_rule = {
        // t=0: doesn't matter (no t=-1)
        {
            {false, false, false},
            {false, false, false},
            {false, false, false}
        },
        // t=1: only center follows the rule
        {
            {false, false, false},
            {false, true, false},
            {false, false, false}
        }
    };

    return grid;
}

void test_all_b3s23_arrangements() {
    std::cout << "Testing all B3/S23 arrangements...\n";

    VariableGrid grid = create_test_grid_all_unknown();
    int num_vars = 0;
    auto clauses = calculate_clauses(grid, num_vars);

    std::cout << "  Generated " << clauses.size() << " clauses with " << num_vars << " variables\n";

    int valid_count = 0;
    int invalid_count = 0;

    // Test all 2^10 = 1024 assignments
    for (int x = 0; x < 1024; x++) {
        // Build assignment vector (1-indexed, so size num_vars+1)
        std::vector<bool> assignment(num_vars + 1);

        // Map bits to variables:
        // bit 0-8 -> neighborhood cells (SAT vars 1-9)
        // bit 9 -> next gen center (SAT var 10)
        for (int bit = 0; bit < 10; bit++) {
            assignment[bit + 1] = (x >> bit) & 1;
        }

        bool clauses_satisfied = satisfies_all_clauses(clauses, assignment);
        bool is_valid = is_valid_b3s23(x);

        if (is_valid != clauses_satisfied) {
            std::cerr << "  MISMATCH at x=" << x << " (" << std::bitset<10>(x) << "): "
                      << "expected " << (is_valid ? "valid" : "invalid")
                      << ", got " << (clauses_satisfied ? "satisfied" : "unsatisfied") << "\n";
            assert(false);
        }

        if (is_valid) valid_count++;
        else invalid_count++;
    }

    std::cout << "  Valid arrangements: " << valid_count << "\n";
    std::cout << "  Invalid arrangements: " << invalid_count << "\n";
    std::cout << "PASSED: test_all_b3s23_arrangements\n";
}

// Test with some known cells in previous generation
void test_known_cells_prev_gen() {
    std::cout << "Testing with known cells in previous generation...\n";

    VariableGrid grid;
    // 3x3, 2 time steps

    // Set corners to known dead (0), center and some neighbors known alive (1)
    // Unknown cells get var indices 2+
    grid.grid = {
        // t=0: corners dead, center alive, others unknown
        {
            {0, 2, 0},      // corners dead, top unknown
            {3, 1, 4},      // left/right unknown, center alive
            {0, 5, 0}       // corners dead, bottom unknown
        },
        // t=1: center unknown
        {
            {0, 0, 0},
            {0, 6, 0},
            {0, 0, 0}
        }
    };

    // Only enforce rule for center cell at t=1
    grid.follows_rule = {
        {{false, false, false}, {false, false, false}, {false, false, false}},
        {{false, false, false}, {false, true, false}, {false, false, false}}
    };

    int num_vars = 0;
    auto clauses = calculate_clauses(grid, num_vars);

    std::cout << "  Generated " << clauses.size() << " clauses with " << num_vars << " variables\n";

    // With corners dead and center alive, we have 5 unknown cells:
    // top, left, right, bottom (at t=0) and center at t=1
    // Var indices: 2,3,4,5 -> SAT vars 1,2,3,4 for t=0 neighbors
    // Var index 6 -> SAT var 5 for t=1 center

    // Test all 2^5 = 32 assignments
    int valid_count = 0;
    for (int x = 0; x < 32; x++) {
        std::vector<bool> assignment(num_vars + 1);
        for (int bit = 0; bit < 5; bit++) {
            assignment[bit + 1] = (x >> bit) & 1;
        }

        // Reconstruct full 10-bit state for B3/S23 check
        // Layout: bit 0=TL, 1=T, 2=TR, 3=L, 4=C, 5=R, 6=BL, 7=B, 8=BR, 9=next
        // Known: corners (0,2,6,8) = 0, center (4) = 1
        // Unknown: T(1)=var1, L(3)=var2, R(5)=var3, B(7)=var4, next(9)=var5
        int full_state = 0;
        full_state |= (1 << 4);  // center alive
        if (assignment[1]) full_state |= (1 << 1);  // top
        if (assignment[2]) full_state |= (1 << 3);  // left
        if (assignment[3]) full_state |= (1 << 5);  // right
        if (assignment[4]) full_state |= (1 << 7);  // bottom
        if (assignment[5]) full_state |= (1 << 9);  // next gen

        bool clauses_satisfied = satisfies_all_clauses(clauses, assignment);
        bool is_valid = is_valid_b3s23(full_state);

        if (is_valid != clauses_satisfied) {
            std::cerr << "  MISMATCH at x=" << x << ": expected " << is_valid
                      << ", got " << clauses_satisfied << "\n";
            assert(false);
        }

        if (is_valid) valid_count++;
    }

    std::cout << "  Valid arrangements: " << valid_count << " out of 32\n";
    std::cout << "PASSED: test_known_cells_prev_gen\n";
}

// Test with known cell in next generation
void test_known_cell_next_gen() {
    std::cout << "Testing with known cell in next generation...\n";

    VariableGrid grid;
    // 3x3, 2 time steps

    // All t=0 cells unknown, but next gen center is known alive
    grid.grid = {
        // t=0: all unknown (var indices 2-10)
        {
            {2, 3, 4},
            {5, 6, 7},
            {8, 9, 10}
        },
        // t=1: center known alive (1)
        {
            {0, 0, 0},
            {0, 1, 0},
            {0, 0, 0}
        }
    };

    // Only enforce rule for center cell at t=1
    grid.follows_rule = {
        {{false, false, false}, {false, false, false}, {false, false, false}},
        {{false, false, false}, {false, true, false}, {false, false, false}}
    };

    int num_vars = 0;
    auto clauses = calculate_clauses(grid, num_vars);

    std::cout << "  Generated " << clauses.size() << " clauses with " << num_vars << " variables\n";

    // 9 unknown cells at t=0, next gen known alive
    // Test all 2^9 = 512 assignments
    int valid_count = 0;
    for (int x = 0; x < 512; x++) {
        std::vector<bool> assignment(num_vars + 1);
        for (int bit = 0; bit < 9; bit++) {
            assignment[bit + 1] = (x >> bit) & 1;
        }

        // Full state: neighborhood x, next gen = 1 (alive)
        int full_state = x | (1 << 9);

        bool clauses_satisfied = satisfies_all_clauses(clauses, assignment);
        bool is_valid = is_valid_b3s23(full_state);

        if (is_valid != clauses_satisfied) {
            std::cerr << "  MISMATCH at x=" << x << ": expected " << is_valid
                      << ", got " << clauses_satisfied << "\n";
            assert(false);
        }

        if (is_valid) valid_count++;
    }

    std::cout << "  Valid arrangements (producing alive): " << valid_count << " out of 512\n";

    // Also test with next gen known dead
    grid.grid[1][1][1] = 0;  // next gen center known dead

    clauses = calculate_clauses(grid, num_vars);

    int valid_dead_count = 0;
    for (int x = 0; x < 512; x++) {
        std::vector<bool> assignment(num_vars + 1);
        for (int bit = 0; bit < 9; bit++) {
            assignment[bit + 1] = (x >> bit) & 1;
        }

        // Full state: neighborhood x, next gen = 0 (dead)
        int full_state = x;  // bit 9 = 0

        bool clauses_satisfied = satisfies_all_clauses(clauses, assignment);
        bool is_valid = is_valid_b3s23(full_state);

        if (is_valid != clauses_satisfied) {
            std::cerr << "  MISMATCH (dead next) at x=" << x << ": expected " << is_valid
                      << ", got " << clauses_satisfied << "\n";
            assert(false);
        }

        if (is_valid) valid_dead_count++;
    }

    std::cout << "  Valid arrangements (producing dead): " << valid_dead_count << " out of 512\n";
    std::cout << "PASSED: test_known_cell_next_gen\n";
}

int main() {
    test_all_b3s23_arrangements();
    test_known_cells_prev_gen();
    test_known_cell_next_gen();

    std::cout << "\nAll calculate_clauses tests passed!\n";
    return 0;
}
