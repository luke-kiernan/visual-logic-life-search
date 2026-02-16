#include <iostream>
#include <cassert>
#include "../src/search_problem.hpp"
#include "../src/variable_pattern.hpp"
#include "../src/known_pattern.hpp"
#include "../src/known_pattern.cpp"
#include "../src/solver.hpp"

// Test: Find a stable pattern that replaces one or both halves of the P44 oscillator
//
// The P44 has 180° rotational symmetry with the LOM (rotating block) at the center.
// We use the full P44 to define the LOM region, then search for stable catalysts.

int main() {
    // Full P44 oscillator (two P22 halves with 180° symmetry)
    std::string p22_rle = R"(x = 49, y = 31, rule = B3/S23
22b2o7bo$23bo8bo2bo$23bobo4bo4bo4b2o$24b2o13bo2bo$29bo8b2ob2o$28b2ob2o
8bo$28bo2bo13b2o$29b2o4bo4bo4bobo$35bo2bo8bo$39bo7b2o4$23bo8b2o$16bo5b
2o7bobo$15bobo3bo10bo$15b2o5bobo$22bobo2$19b2o$2o16bo2bo$bo16b2obo$bob
o4b3o5bo2b2o$2b2o4bobo4bo$7bo3bo3bo3bo$7bo3bo3bo3bo$11bo4bobo4b2o$6b2o
2bo5b3o4bobo$5bob2o16bo$5bo2bo16b2o$6b2o!)";

    bool spatial_symmetry = true;

    KnownPattern p22(p22_rle, 22);

    auto bounds = p22.get_bounds();
    int x_min = std::get<0>(bounds).first;
    int x_max = std::get<0>(bounds).second;
    int y_min = std::get<1>(bounds).first;
    int y_max = std::get<1>(bounds).second;

    // Find center of pattern (center of 180° rotation)
    int cx = (x_min + x_max) / 2;
    int cy = (y_min + y_max) / 2;

    p22.shift_by(Point(-cx, -cy, 0));

    bounds = p22.get_bounds();
    x_min = std::get<0>(bounds).first;
    x_max = std::get<0>(bounds).second;
    y_min = std::get<1>(bounds).first;
    y_max = std::get<1>(bounds).second;

    std::cout << "Center of rotation: (" << cx << ", " << cy << ")\n";
    std::cout << "bbox: x=[" << x_min << "," << x_max << "], y=[" << y_min << "," << y_max << "]\n";
    std::cout << "dimensions: " << x_max - x_min + 1 << " by " << y_max - y_min + 1 << "\n";



    std::cout << "P22 oscillator at t=0:\n";
    p22.print_gen(0);

    // center part that should match between the two patterns
    std::pair<int, int> matches_p22_xlims = {-3, 3};
    std::pair<int, int> matches_p22_ylims = {-2, 3};
    // the LOM region, the hole in the stable catalyst region.
    std::pair<int, int> lom_xlims = {-4, 4};
    std::pair<int, int> lom_ylims = {-2, 3};

    int max_gen = 22;

    // 1. Stable catalyst pattern (with t -> t+1 symmetry)
    int x_sub = 8;
    int y_sub = 7;
    int stable_xmin = x_min + x_sub;
    int stable_xmax = x_max - x_sub;
    int stable_ymin = y_min + y_sub;
    int stable_ymax = y_max - y_sub;
    VariablePattern stable_catalyst(Bounds({{stable_xmin, stable_xmax}, {stable_ymin, stable_ymax}, {0, max_gen}}));
    {
        // Border cell group (added first for lower priority):
        // known dead, stable, with spatial symmetry so interior cells mapping here are forced dead.
        CellGroup border_group;
        border_group.time_transformation = {1, 0, 0, 1, 0, 0, 1};  // t -> t+1
        if(spatial_symmetry)
            border_group.spatial_transformations.push_back({-1, 0, 0, -1, 0, 1, 0});
        int border_idx = stable_catalyst.add_cell_group(border_group);

        // Interior cell group
        CellGroup stable_group;
        stable_group.time_transformation = {1, 0, 0, 1, 0, 0, 1};  // t -> t+1
        if(spatial_symmetry)
            stable_group.spatial_transformations.push_back({-1, 0, 0, -1, 0, 1, 0});
        int stable_idx = stable_catalyst.add_cell_group(stable_group);

        stable_catalyst.set_cell_group_if(stable_idx, [&](const Cell& c) {
            return !stable_catalyst.is_boundary(c.position);
        });
        stable_catalyst.set_cell_group_if(border_idx, [&](const Cell& c) {
            return stable_catalyst.is_boundary(c.position);
        });
        stable_catalyst.set_known_if(false, [&](const Cell& c) {
            return stable_catalyst.is_boundary(c.position);
        });
    }
    stable_catalyst.build();


    // 2. Interaction region for middle generations (no symmetry)
    VariablePattern interaction(Bounds({{stable_xmin, stable_xmax}, {stable_ymin, stable_ymax}, {0, max_gen}}));
    if (spatial_symmetry) {
        CellGroup interaction_group;
        // (x,y,t) -> (-x, 1-y, t+11)
        interaction_group.time_transformation = {-1, 0, 0, -1, 0, 1, 11};
        int idx = interaction.add_cell_group(interaction_group);
        interaction.set_cell_group_if(idx, [](const Cell&){return true;});
    }
    interaction.build();


    // 3. Create SearchProblem
    std::cout << "Creating search problem...\n";
    SearchProblem problem(Bounds({{stable_xmin, stable_xmax}, {stable_ymin, stable_ymax}, {0, max_gen}}));

    // Lambda to check if point is in LOM region
    auto in_lom = [lom_xlims, lom_ylims](Point p) {
        auto [x, y, t] = p;
        return x >= lom_xlims.first && x <= lom_xlims.second &&
               y >= lom_ylims.first && y <= lom_ylims.second;
    };

    // Entry 0: matches P22.
    problem.add_entry(&p22,
        [=](Point p) { 
            auto [x, y, t] = p;
            return in_lom(p) && (t <= 4 || (t >= 10 && t <= 11+4) || t >= 11+10); }
    );

    // Entry 1: Stable catalyst. Non-LOM region at t=0-3 and t=10,11; t = 11-14 and t = 21,22 
    problem.add_entry(&stable_catalyst,
        [=](Point p) {
            auto [x, y, t] = p;
            return !in_lom(p) && (t <= 4 || (t >= 10 && t <= 11+4) || t >= 11+10);
        }
    );

    // Entry 2: Interaction region.
    problem.add_entry(&interaction,
        [=](Point p) {
            auto [x, y, t] = p;
            return ((t >= 5) && (t <= 9)) || (t >= 5+11 && t <= 9+11);
        }
    );

    std::cout << "Building search problem...\n";
    problem.build();

    std::cout << "Generating clauses...\n";
    ClauseList clauses = problem.get_clauses();
    int num_vars = problem.num_variables();

    // Note: "at least one cell alive" constraint is redundant - the catalyst
    // must be non-empty to perturb the active region.

    std::cout << "  " << clauses.size() << " clauses, " << num_vars << " variables\n";

    std::cout << "Calling solver...\n";
    SolverResult result = solve(clauses, num_vars);

    auto print_gen = [&](int t) {
        std::cout << "Generation " << t << ":\n";
        for (int y = stable_ymin; y <= stable_ymax; y++) {
            for (int x = stable_xmin; x <= stable_xmax; x++) {
                int var_idx = problem.get_cell_value(Point(x, y, t));
                bool alive;
                if (var_idx == 0) alive = false;
                else if (var_idx == 1) alive = true;
                else alive = result.solution.count(var_idx - 1) > 0;
                std::cout << (alive ? 'o' : '.');
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    };

    if (result.status == SolverStatus::SAT) {
        std::cout << "SATISFIABLE!\n\n";
        print_gen(0);
        print_gen(5);
        print_gen(11);
    } else if (result.status == SolverStatus::UNSAT) {
        std::cout << "UNSATISFIABLE\n";
    } else {
        std::cout << "ERROR: " << result.error_message << "\n";
    }

    return 0;
}
