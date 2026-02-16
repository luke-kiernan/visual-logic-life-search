#include <cassert>
#include <iostream>
#include <set>
#include "../src/variable_grid.cpp"

// Construct an LWSS search pattern:
// - 6x5 grid (x: 0-5, y: -2 to 2)
// - 3 generations (t: 0, 1, 2)
// - Time transformation: flip y, shift x+1, after 2 gens
//   i.e. (x, y, t) -> (x+1, -y, t+2)
VariablePattern create_lwss_search_pattern() {
    // Start with basic 6x5 grid, 3 generations
    VariablePattern pattern(6, 5, 2);

    // Shift y by -2 so y ranges from -2 to 2 (centered for flip symmetry)
    pattern.shift_by({0, -2, 0});

    // Add cell group with time transformation: (x, y, t) -> (x+1, -y, t+2)
    int lwss_group = pattern.add_cell_group({1, 0, 0, -1, 1, 0, 2});

    // Assign all cells to the LWSS group
    pattern.set_cell_group_if(lwss_group, [](const Cell&){ return true; });

    return pattern;
}

void test_lwss_variable_grid() {
    std::cout << "Testing LWSS search pattern -> variable grid...\n";

    VariablePattern pattern = create_lwss_search_pattern();
    VariableGrid var_grid = construct_variable_grid(pattern);

    // Check dimensions
    assert(var_grid.grid.size() == 3);      // 3 generations
    assert(var_grid.grid[0].size() == 5);   // 5 rows (y: -2 to 2)
    assert(var_grid.grid[0][0].size() == 6); // 6 columns (x: 0 to 5)

    std::cout << "  Grid dimensions: " << var_grid.grid[0][0].size() << "x"
              << var_grid.grid[0].size() << "x" << var_grid.grid.size() << "\n";

    // The time transformation maps (x, y, 0) -> (x+1, -y, 2)
    // So cells at t=0 should have the same variable as their transformed counterparts at t=2
    //
    // Grid indexing: grid[t][y - ymin][x - xmin] = grid[t][y + 2][x]
    //
    // (0, 0, 0) -> (1, 0, 2): should have same variable
    // (0, 1, 0) -> (1, -1, 2): should have same variable
    // (0, -1, 0) -> (1, 1, 2): should have same variable
    // (1, 2, 0) -> (2, -2, 2): should have same variable

    auto get_var = [&](int x, int y, int t) {
        return var_grid.grid[t][y + 2][x];
    };

    // Check that transformed pairs have the same variable
    int pairs_checked = 0;
    for (int x = 0; x <= 4; x++) {  // x+1 must be <= 5
        for (int y = -2; y <= 2; y++) {
            int var_t0 = get_var(x, y, 0);
            int var_t2 = get_var(x + 1, -y, 2);

            if (var_t0 != var_t2) {
                std::cerr << "  MISMATCH: (" << x << "," << y << ",0) has var " << var_t0
                          << " but (" << x+1 << "," << -y << ",2) has var " << var_t2 << "\n";
                assert(false);
            }
            pairs_checked++;
        }
    }
    std::cout << "  Verified " << pairs_checked << " transformed pairs have matching variables\n";

    // Count unique variables (excluding 0=dead, 1=alive)
    std::set<int> unique_vars;
    for (int t = 0; t < 3; t++) {
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 6; x++) {
                int var = var_grid.grid[t][y][x];
                if (var >= 2) unique_vars.insert(var);
            }
        }
    }
    std::cout << "  Unique unknown variables: " << unique_vars.size() << "\n";

    // With the symmetry, we expect fewer variables than 6*5*3 = 90
    // The transformation links t=0 with t=2, so roughly:
    // - t=0: 6*5 = 30 cells
    // - t=1: 6*5 = 30 cells (no symmetry links these)
    // - t=2: most linked to t=0 via symmetry
    // Edge cells at x=0 in t=0 map to x=1 in t=2, but x=5 in t=0 maps to x=6 which is out of bounds
    // So we expect around 30 + 30 + 5 = 65 unique variables (rough estimate)

    std::cout << "PASSED: test_lwss_variable_grid\n";
}

void test_lwss_clauses() {
    std::cout << "Testing LWSS clause generation...\n";

    VariablePattern pattern = create_lwss_search_pattern();
    VariableGrid var_grid = construct_variable_grid(pattern);

    int num_vars = 0;
    auto clauses = calculate_clauses(var_grid, num_vars);

    std::cout << "  Generated " << clauses.size() << " clauses with " << num_vars << " variables\n";

    // Basic sanity checks
    assert(clauses.size() > 0);
    assert(num_vars > 0);

    std::cout << "PASSED: test_lwss_clauses\n";
}

int main() {
    test_lwss_variable_grid();
    test_lwss_clauses();

    std::cout << "\nAll LWSS search tests passed!\n";
    return 0;
}
