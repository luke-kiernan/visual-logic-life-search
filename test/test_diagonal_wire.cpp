#include <iostream>
#include <cassert>
#include "../src/variable_grid.cpp"
#include "../src/solver.hpp"

// Test for 2c/3 diagonal wire search
// - Active perturbation travels (2,2) in 3 generations
// - Background (stable wire) has (2,2) spatial symmetry
// - 14x14 search area, perturbation fits in 3x3 box

VariablePattern create_diagonal_wire_pattern() {
    // 20x20 grid, 4 generations (t=0,1,2,3)
    VariablePattern pattern(20, 20, 3);

    // Shift so grid is centered: x,y from -10 to 9
    pattern.shift_by({-10, -10, 0});

    // Cell group 1: Wire (stable + (2,2) spatial, follows rules)
    CellGroup wire_group;
    wire_group.spatial_transformations.push_back({1, 0, 0, 1, 2, 2, 0}); // translate (2,2)
    wire_group.time_transformation = {1, 0, 0, 1, 0, 0, 1}; // stable
    int wire_group_idx = pattern.add_cell_group(wire_group);

    // Cell group 2 (highest priority): Perturbation
    // - No spatial symmetry within the perturbation
    // - Spacetime transformation: (x,y,t) -> (x+2, y+2, t+3)
    int perturb_group_idx = pattern.add_cell_group({1, 0, 0, 1, 2, 2, 3});

    // All interior cells default to wire group
    pattern.set_cell_group_if(wire_group_idx, [&](const Cell& c) {
        return !pattern.is_boundary(c.position);
    });

    // Perturbation region: 3x3 box at origin for t=0,1,2, shifted by (2,2) at t=3
    pattern.set_cell_group_if(perturb_group_idx, [&](const Cell& c) {
        auto [x, y, t] = c.position;
        if (t <= 2) {
            return x >= -1 && x <= 1 && y >= -1 && y <= 1;
        } else { // t == 3
            return x >= 1 && x <= 3 && y >= 1 && y <= 3;
        }
    });

    // Boundary cells don't follow rules (but still part of wire group)
    for (auto& cell : const_cast<std::vector<Cell>&>(pattern.get_cells())) {
        if (pattern.is_boundary(cell.position)) {
            cell.follows_rules = false;
        }
    }

    return pattern;
}

// Extract solution and print a generation
void print_solution_gen(const VariableGrid& grid, const SolverResult& result, int t) {
    int size_x = grid.size_x();
    int size_y = grid.size_y();

    std::cout << "Generation " << t << ":\n";
    for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
            int var_idx = grid.grid[t][y][x];
            bool alive;
            if (var_idx == 0) alive = false;
            else if (var_idx == 1) alive = true;
            else alive = result.solution.count(var_idx - 1) > 0;
            std::cout << (alive ? 'o' : '.');
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

int main() {
    std::cout << "Creating diagonal wire search pattern...\n";
    VariablePattern pattern = create_diagonal_wire_pattern();

    std::cout << "Building variable grid...\n";
    VariableGrid var_grid = construct_variable_grid(pattern);

    std::cout << "Variable grid dimensions: "
              << var_grid.size_x() << "x" << var_grid.size_y() << "x" << var_grid.size_t() << "\n";

    // Debug: check perturbation linking (grid center is at 10,10 for pattern 0,0)
    std::cout << "Debug - perturbation cells (should link via (2,2,3)):\n";
    std::cout << "  (0,0,0): " << var_grid.grid[0][10][10] << "\n";
    std::cout << "  (2,2,3): " << var_grid.grid[3][12][12] << " (should match (0,0,0))\n";
    std::cout << "Debug - wire cells:\n";
    std::cout << "  (4,4,0): " << var_grid.grid[0][14][14] << "\n";
    std::cout << "  (6,6,0): " << var_grid.grid[0][16][16] << " (should match (4,4,0) via (2,2) spatial)\n";
    std::cout << "  (4,4,3): " << var_grid.grid[3][14][14] << " (should match (4,4,0) via stability)\n";

    std::cout << "Generating clauses...\n";
    int num_vars = 0;
    ClauseList clauses = calculate_clauses(var_grid, num_vars);

    // Ensure num_vars includes all variables in grid (not just those in clauses)
    for (int t = 0; t < var_grid.size_t(); t++) {
        for (int y = 0; y < var_grid.size_y(); y++) {
            for (int x = 0; x < var_grid.size_x(); x++) {
                int var_idx = var_grid.grid[t][y][x];
                if (var_idx >= 2) {
                    num_vars = std::max(num_vars, var_idx - 1);
                }
            }
        }
    }

    // Add constraint: at least one cell must be alive in EACH generation
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

    // Add constraint: perturbation must cause change (center cell differs between t=0 and t=1)
    // Pattern coord (0,0) = grid coord (10,10) after shift
    int center_x = 10, center_y = 10;
    int v0 = var_grid.grid[0][center_y][center_x];
    int v1 = var_grid.grid[1][center_y][center_x];
    std::cout << "  Center cell vars: t=0 -> " << v0 << ", t=1 -> " << v1 << "\n";
    if (v0 >= 2 && v1 >= 2 && v0 != v1) {
        // XOR constraint: v0 != v1
        // Encoded as: (v0 OR v1) AND (NOT v0 OR NOT v1)
        clauses.emplace_back(make_clause({v0 - 1, v1 - 1}));           // at least one true
        clauses.emplace_back(make_clause({-(v0 - 1), -(v1 - 1)}));     // at least one false
        std::cout << "  Added XOR constraint\n";
    } else {
        std::cout << "  WARNING: Could not add XOR constraint (vars equal or known)\n";
    }

    std::cout << "  " << (clauses.size() + big_clauses.size()) << " clauses, " << num_vars << " variables\n";

    std::cout << "Calling solver...\n";
    SolverResult result = solve(clauses, num_vars, "kissat", big_clauses);

    if (result.status == SolverStatus::SAT) {
        std::cout << "SATISFIABLE!\n\n";
        for (int t = 0; t < var_grid.size_t(); t++) {
            print_solution_gen(var_grid, result, t);
        }
    } else if (result.status == SolverStatus::UNSAT) {
        std::cout << "UNSATISFIABLE\n";
    } else {
        std::cout << "ERROR: " << result.error_message << "\n";
    }

    return 0;
}
