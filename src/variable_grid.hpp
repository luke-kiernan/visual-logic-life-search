#pragma once
/*
Represents a rectangular grid of variables. Used as an intermediate between VariablePattern and CNF clauses:
it is at this step that we turn geometry constraints (cell groups, symmetry) into variable equivalences.
0 and 1 have special meanings: 0 = dead cell, 1 = live cell, other variables are for "unknown" cells.
i.e. all cells labeled by 2 have the same state.
*/
#include <vector>
#include <tuple>
#include "sub_pattern.hpp"  // For Clause, ClauseList
#include "variable_pattern.hpp"  // For VariablePattern

struct VariableGrid {
    // grid[t][y][x] = variable index. Upper-left is always (0,0,0).
    std::vector<std::vector<std::vector<int>>> grid;
    // If follows_rule[t][y][x] is false, skip clauses for cell (x,y) at time t
    // evolving from the 3x3 neighborhood at time t-1.
    std::vector<std::vector<std::vector<bool>>> follows_rule;

    int size_x() const { return grid.empty() || grid[0].empty() ? 0 : grid[0][0].size(); }
    int size_y() const { return grid.empty() ? 0 : grid[0].size(); }
    int size_t() const { return grid.size(); }
};

VariableGrid construct_variable_grid(const VariablePattern& pattern);
void write_csv(const VariableGrid& var_grid, const std::string& filename, bool overwrite = false);
ClauseList calculate_clauses(const VariableGrid& var_grid, int& num_variables);
void write_cnf(const VariableGrid& var_grid, const std::string& filename, bool overwrite = false);
void print_variable_grid(const VariableGrid& var_grid);