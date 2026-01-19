#include "variable_grid.hpp"
#include <map>
#include <fstream>
#include <stdexcept>
#include <cassert>
#include "sat_logic.hpp"


Point union_find_find(std::map<Point, Point>& repr, Point p){
    assert(repr.find(p) != repr.end());
    while(repr[p] != p){
        repr[p] = repr[repr[p]];
        p = repr[p];
    }
    return p;
}

void union_find_union(std::map<Point, Point>& repr, Point a, Point b){
    Point ra = union_find_find(repr, a);
    Point rb = union_find_find(repr, b);
    if(ra != rb){
        repr[max(ra, rb)] = min(ra, rb);
    }
}



VariableGrid construct_variable_grid(const SearchPattern& pattern){
    std::map<Point, Point> repr; // union-find structure for equivalent cells.
    Bounds bounds = pattern.get_bounds();
    auto xlims = std::get<0>(bounds);
    auto ylims = std::get<1>(bounds);
    auto tlims = std::get<2>(bounds);
    Point live_cell = Point(-9999, -9999, -9999); // sentinel for live cells.
    Point dead_cell = Point(-9998, -9998, -9998); // sentinel for dead cells.
    repr[live_cell] = live_cell;
    repr[dead_cell] = dead_cell;
    for (const Cell& cell : pattern.get_cells()){
        repr[cell.position] = cell.position;  // initialize as singleton first
        if (is_live(cell)){
            union_find_union(repr, cell.position, live_cell);
        } else if (is_dead(cell)){
            union_find_union(repr, cell.position, dead_cell);
        }
    }
    // now go work out which cells are equivalent, via union-find and the transformations in each cell group.
    // assumptions:
    // (1) if it matches a cell in a known pattern, it should be set to known and live/dead already.
    const auto& cell_groups = pattern.get_cell_groups();
    for (const Cell& cell : pattern.get_cells()){
        // Skip cells with no cell group (sentinel value -1)
        if (cell.cell_group == DEFAULT_CELL_GROUP)
            continue;
        const CellGroup& cell_group = cell_groups[cell.cell_group];
        Point cell_pos = cell.position;
        // spatial transformations:
        std::set<Point> images = find_all_images(cell_pos,
            cell_group.spatial_transformations,
            bounds
        );
        for (Point img : images) {
            const Cell& target = pattern.get_cell(img);
            // Only link if target's priority <= source's priority
            // Never link to DEFAULT_CELL_GROUP cells (boundary/excluded cells)
            if (target.cell_group != DEFAULT_CELL_GROUP && target.cell_group <= cell.cell_group)
                union_find_union(repr, cell_pos, img);
        }
        // time transformation:
        Point time_img = transform(cell_group.time_transformation, cell_pos);
        if(in_limits(time_img, bounds) && time_img != cell_pos) {
            const Cell& target = pattern.get_cell(time_img);
            // Only link if target's priority <= source's priority
            // Never link to DEFAULT_CELL_GROUP cells (boundary/excluded cells)
            if (target.cell_group != DEFAULT_CELL_GROUP && target.cell_group <= cell.cell_group)
                union_find_union(repr, cell_pos, time_img);
        }
    }

    // fill in the variable grid.
    int grid_size_x = xlims.second - xlims.first + 1;
    int grid_size_y = ylims.second - ylims.first + 1;
    int grid_size_t = tlims.second - tlims.first + 1;
    std::vector<std::vector<std::vector<int>>> grid(
        grid_size_t,
        std::vector<std::vector<int>>(grid_size_y,
            std::vector<int>(grid_size_x, -1) // TODO or do I want 0?
        )
    );

    std::map<Point, int> repr_to_varindex;
    repr_to_varindex[union_find_find(repr, dead_cell)] = 0;
    repr_to_varindex[union_find_find(repr, live_cell)] = 1;
    int next_var_index = 2; // 0 = dead, 1 = live
    for (const Cell& cell : pattern.get_cells()){
        Point cell_pos = cell.position;
        Point cell_repr = union_find_find(repr, cell_pos);
        if(repr_to_varindex.find(cell_repr) == repr_to_varindex.end()){
            repr_to_varindex[cell_repr] = next_var_index;
            next_var_index++;
        }
        int var_index = repr_to_varindex[cell_repr];
        auto [x, y, t] = cell_pos;
        grid[t - tlims.first][y - ylims.first][x - xlims.first] = var_index;
    }
    // Initialize follows_rule from Cell.follows_rules
    std::vector<std::vector<std::vector<bool>>> follows_rule(
        grid_size_t,
        std::vector<std::vector<bool>>(grid_size_y,
            std::vector<bool>(grid_size_x, true)
        )
    );
    for (const Cell& cell : pattern.get_cells()) {
        auto [x, y, t] = cell.position;
        follows_rule[t - tlims.first][y - ylims.first][x - xlims.first] = cell.follows_rules;
    }

    VariableGrid var_grid;
    var_grid.grid = std::move(grid);
    var_grid.follows_rule = std::move(follows_rule);
    return var_grid;
}

std::ofstream file_checks(const std::string& filename, bool overwrite){
    if(!overwrite){
        std::ifstream check(filename);
        if(check.good()){
            throw std::runtime_error("File already exists: " + filename);
        }
    }
    std::ofstream file(filename);
    if(!file.is_open()){
        throw std::runtime_error("Could not open file for writing: " + filename);
    }
    return file;
}

void write_csv(const VariableGrid& var_grid, const std::string& filename, bool overwrite){
    std::ofstream file = file_checks(filename, overwrite);

    const auto& grid = var_grid.grid;
    int grid_size_t = grid.size();

    for(int t = 0; t < grid_size_t; t++){
        if(t > 0){
            file << "\n"; // empty line between grids
        }
        const auto& grid_t = grid[t];
        int grid_size_y = grid_t.size();
        for(int y = 0; y < grid_size_y; y++){
            const auto& row = grid_t[y];
            int grid_size_x = row.size();
            for(int x = 0; x < grid_size_x; x++){
                if(x > 0){
                    file << ",";
                }
                file << row[x];
            }
            file << "\n";
        }
    }
}

bool add_clause_term(std::set<int>& clause, int literal){
    if(clause.find(-literal) != clause.end())
        return true; // clause is satisfied: x or -x or ... = true
    clause.insert(literal);
    return false; // clause not yet satisfied
}

// TODO: other encodings, like knuth 3-sat or APG's "split" variant.
// Python implementation of both: https://gitlab.com/apgoucher/metasat/-/blob/master/grills.py
// C implementation of knuth: https://taocp-fun.gitlab.io/v4f6-sat-knuth/pdf/sat-life.pdf
ClauseList calculate_clauses(const VariableGrid& var_grid, int& num_variables){
    std::array<int, 10> ten_cells{}; // 9 neighborhood + next gen

    const auto& grid = var_grid.grid;
    int grid_size_t = grid.size();
    int grid_size_y = grid[0].size();
    int grid_size_x = grid[0][0].size();

    // PERF: switch to using fixed-size array for each clause, with 0 as sentinel value
    // PERF: duplicate clauses elimination.
    ClauseList clauses;
    std::set<int> clause; // max length 10, using set to detect tautologies (x OR ~x)
    num_variables = 0;
    for (int t = 0; t + 1 < grid_size_t; t++){
        for (int y = 0; y < grid_size_y; y++){
            for (int x = 0; x < grid_size_x; x++){
                // Skip if this cell doesn't follow the rule
                if (!var_grid.follows_rule[t + 1][y][x])
                    continue;
                // gather neighborhood
                int i = 0;
                for (int dy = -1; dy <= 1; dy++){
                    for (int dx = -1; dx <= 1; dx++){
                        int x_new = x + dx;
                        int y_new = y + dy;
                        if (x_new < 0 || x_new >= grid_size_x || y_new < 0 || y_new >= grid_size_y)
                            ten_cells[i] = 0; // assume out-of-bounds cells are dead.
                        else
                            ten_cells[i] = grid[t][y_new][x_new];
                        i++;
                    }
                }
                // next gen cell
                ten_cells[9] = grid[t + 1][y][x];
                // now ten_cells has the 9 neighborhood cells and the next gen cell.
                // iterate thru prime implicants to add clauses.
                // for known cells: skip cell if unsatisfied, skip clause if satisfied.
                for (const auto [care, force] : primeImplicants){
                    bool clause_satisfied = false;
                    for (int bit = 0; bit < 10; bit++){
                        if(care & (1 << bit)){
                            int var_index = ten_cells[bit];
                            if (var_index < 2){ // known cell
                                bool force_state = (force & (1 << bit)) != 0;
                                bool cell_state = (var_index != 0);
                                if (cell_state == force_state)
                                    clause_satisfied = true;
                            } else {
                                // unknown cell: add literal
                                int sign = (force & (1 << bit)) ? 1 : -1;
                                clause_satisfied = add_clause_term(clause, sign * (var_index - 1));
                                num_variables = std::max(num_variables, var_index - 1);
                            }
                            if (clause_satisfied)
                                break;
                        }
                    }
                    if(!clause_satisfied && !clause.empty())
                        clauses.emplace_back(Clause(clause.begin(), clause.end()));
                    clause.clear();
                }
            }
        }
    }
    return clauses;
}

void write_cnf(const VariableGrid& var_grid, const std::string& filename, bool overwrite){
    std::ofstream file = file_checks(filename, overwrite);

    int num_variables = 0;
    ClauseList clauses = calculate_clauses(var_grid, num_variables);
    int num_clauses = clauses.size();

    // write CNF header
    file << "p cnf " << num_variables << " " << num_clauses << "\n";

    // write clauses to file.
    for (const auto& cl : clauses){
        for (int lit : cl){
            file << lit << " ";
        }
        file << "0\n";
    }
}

void print_variable_grid(const VariableGrid& var_grid){
    const auto& grid = var_grid.grid;
    int size_t = var_grid.size_t();
    int size_y = var_grid.size_y();
    int size_x = var_grid.size_x();

    // Find max variable index to determine column width
    int max_var = 0;
    for (const auto& t_grid : grid) {
        for (const auto& row : t_grid) {
            for (int var : row) {
                if (var > max_var) max_var = var;
            }
        }
    }
    int width = 1;
    while (max_var >= 10) { max_var /= 10; width++; }

    for (int t = 0; t < size_t; t++) {
        printf("Generation %d:\n", t);
        printf("  x:");
        for (int x = 0; x < size_x; x++) {
            printf(" %*d", width, x);
        }
        printf("\n");

        for (int y = 0; y < size_y; y++) {
            printf("y=%*d:", width, y);
            for (int x = 0; x < size_x; x++) {
                int var = grid[t][y][x];
                bool follows = var_grid.follows_rule[t][y][x];
                if (!follows) {
                    printf(" %*s", width, "*");  // mark cells that don't follow rules
                } else if (var == 0) {
                    printf(" %*s", width, ".");  // known dead
                } else if (var == 1) {
                    printf(" %*s", width, "o");  // known alive
                } else {
                    printf(" %*d", width, var);
                }
            }
            printf("\n");
        }
        printf("\n");
    }
}
