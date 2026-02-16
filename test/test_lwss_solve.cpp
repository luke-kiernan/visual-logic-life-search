#include <iostream>
#include "../src/variable_grid.cpp"
#include "../src/known_pattern.cpp"
#include "../src/solver.hpp"

// Create LWSS search pattern with boundary of dead cells
// Interior: 6x5 (x: 0-5, y: -2 to 2)
// With boundary: 8x7 (x: -1 to 6, y: -3 to 3)
VariablePattern create_lwss_search_pattern() {
    // Create 8x7 grid with 3 generations
    VariablePattern pattern(8, 7, 2);

    // Shift so grid is at x: -1 to 6, y: -3 to 3
    pattern.shift_by({-1, -3, 0});

    // Add cell group with LWSS glide-reflection: (x, y, t) -> (x+1, -y, t+2)
    int lwss_group = pattern.add_cell_group({1, 0, 0, -1, 1, 0, 2});

    // All cells use the LWSS cell group (boundary cells are known dead,
    // so the symmetry propagates the dead constraint to interior cells whose images land on them)
    pattern.set_cell_group_if(lwss_group, [](const Cell&){ return true; });

    // Boundary cells are known dead
    pattern.set_known_if(false, [&](const Cell& c){ return pattern.is_boundary(c.position); });

    return pattern;
}

// Create a KnownPattern from the solver result (all generations)
KnownPattern solution_to_known_pattern(const VariableGrid& grid, const SolverResult& result) {
    KnownPattern pattern;
    int size_x = grid.size_x();
    int size_y = grid.size_y();
    int size_t = grid.size_t();

    // Extract on-cells from solution for all generations
    for (int t = 0; t < size_t; t++) {
        for (int y = 0; y < size_y; y++) {
            for (int x = 0; x < size_x; x++) {
                int var_idx = grid.grid[t][y][x];

                bool is_alive;
                if (var_idx == 0) {
                    is_alive = false;
                } else if (var_idx == 1) {
                    is_alive = true;
                } else {
                    int sat_var = var_idx - 1;
                    is_alive = result.solution.count(sat_var) > 0;
                }

                if (is_alive) {
                    pattern.on_cells.insert({x, y, t});
                }
            }
        }
    }

    pattern.bounds = {{0, size_x - 1}, {0, size_y - 1}, {0, size_t - 1}};
    return pattern;
}

int main() {
    std::cout << "Creating LWSS search pattern...\n";
    VariablePattern pattern = create_lwss_search_pattern();

    std::cout << "Building variable grid...\n";
    VariableGrid var_grid = construct_variable_grid(pattern);

    std::cout << "\nVariable grid:\n";
    print_variable_grid(var_grid);

    std::cout << "Generating clauses...\n";
    int num_vars = 0;
    auto clauses = calculate_clauses(var_grid, num_vars);

    // Add constraint: at least one cell in each generation must be alive
    BigClauseList big_clauses;
    for (int t = 0; t < var_grid.size_t(); t++) {
        BigClause at_least_one_alive;
        for (int y = 0; y < var_grid.size_y(); y++) {
            for (int x = 0; x < var_grid.size_x(); x++) {
                int var_idx = var_grid.grid[t][y][x];
                if (var_idx >= 2) {
                    at_least_one_alive.push_back(var_idx - 1);
                }
            }
        }
        if (!at_least_one_alive.empty()) {
            big_clauses.push_back(at_least_one_alive);
        }
    }

    std::cout << "  " << (clauses.size() + big_clauses.size()) << " clauses, " << num_vars << " variables\n";

    std::cout << "Calling solver...\n";
    SolverResult result = solve(clauses, num_vars, "kissat", big_clauses);

    if (result.status == SolverStatus::SAT) {
        std::cout << "SATISFIABLE!\n\n";
        KnownPattern solution = solution_to_known_pattern(var_grid, result);
        solution.print_gen(0);
        solution.print_gen(1);
        solution.print_gen(2);
    } else if (result.status == SolverStatus::UNSAT) {
        std::cout << "UNSATISFIABLE\n";
    } else {
        std::cout << "ERROR: " << result.error_message << "\n";
    }

    return 0;
}
