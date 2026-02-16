#pragma once
/*
VariablePattern: A pattern with unknown cells and symmetry constraints via cell groups.
Inherits from SubPattern for use in SearchProblem composition.

Cell groups define:
- Spatial transformations: symmetries within each generation
- Time transformation: how cells map between generations (e.g., t -> t+1 for stable)

The build() method runs union-find to determine which cells share the same variable.
*/

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <functional>
#include <cassert>
#include "sub_pattern.hpp"
#include "cell.hpp"
#include "sat_logic.hpp"
#include "union_find.hpp"

class VariablePattern : public SubPattern {
    private:
        std::vector<CellGroup> cell_groups;
        std::vector<Cell> cell_list;
        Bounds bounds;
        int x_min, y_min, t_min;
        int sz_x, sz_y;

        // Built state (populated by build())
        bool is_built = false;
        std::unordered_map<Point, int, PointHash> cell_to_var;  // maps position -> variable index (0=dead, 1=alive, >=2 variable)
        int variable_count = 0;  // number of unique variables (not counting 0 and 1)

        // O(1) flat index into cell_list from grid coordinates.
        // Returns -1 if out of bounds.
        int cell_index(Point p) const {
            auto [x, y, t] = p;
            int lx = x - x_min, ly = y - y_min, lt = t - t_min;
            auto [xlims, ylims, tlims] = bounds;
            int sz_t = tlims.second - tlims.first + 1;
            if (lx < 0 || lx >= sz_x || ly < 0 || ly >= sz_y || lt < 0 || lt >= sz_t)
                return -1;
            return lt * (sz_y * sz_x) + ly * sz_x + lx;
        }

        Cell* get_cell_ptr(Point p);

    public:
        VariablePattern(Bounds bounds);
        VariablePattern(int width, int height, int max_gen);

        // Getters
        const std::vector<Cell>& get_cells() const { return cell_list; }
        const std::vector<CellGroup>& get_cell_groups() const { return cell_groups; }
        Cell get_cell(Point p) const;

        // Modifiers
        void shift_by(Point rel_shift);
        int add_cell_group(const CellGroup& group);
        int add_cell_group(AffineTransf time_transformation);

        void set_cell_group(Point p, int group_idx);
        void set_cell_group_if(int group_idx, std::function<bool(const Cell&)> predicate);

        void set_known(Point p, bool state);
        void set_dead(Point p);
        void set_alive(Point p);
        void set_known_if(bool state, std::function<bool(const Cell&)> predicate);

        bool is_boundary(Point p) const override;

        // SubPattern interface implementation
        Bounds get_bounds() const override { return bounds; }

        void build() override;

        int get_cell_value(Point p) const override {
            assert(is_built);
            auto it = cell_to_var.find(p);
            if (it == cell_to_var.end()) {
                // Out of bounds - treat as dead
                return 0;
            }
            return it->second;  // 0 = dead, 1 = alive, >= 2 = local variable index
        }

        int num_variables() const override {
            assert(is_built);
            return variable_count;
        }

        bool is_known(Point p) const override {
            const Cell& cell = get_cell(p);
            return cell.known;
        }

        bool get_state(Point p) const override {
            const Cell& cell = get_cell(p);
            return cell.state;  // Only valid if is_known(p) is true
        }

        bool follows_rules(Point p) const override {
            const Cell& cell = get_cell(p);
            return cell.follows_rules;
        }

        ClauseList get_clauses(int base_var_index) const override;
};

// Constructor: creates a grid with given bounds
VariablePattern::VariablePattern(Bounds bounds)
    : bounds(bounds),
      x_min(std::get<0>(bounds).first), y_min(std::get<1>(bounds).first), t_min(std::get<2>(bounds).first),
      sz_x(std::get<0>(bounds).second - x_min + 1), sz_y(std::get<1>(bounds).second - y_min + 1)
{
    auto [xlims, ylims, tlims] = bounds;
    for (int t = tlims.first; t <= tlims.second; t++) {
        for (int y = ylims.first; y <= ylims.second; y++) {
            for (int x = xlims.first; x <= xlims.second; x++) {
                Cell cell;
                cell.position = {x, y, t};
                cell.cell_group = DEFAULT_CELL_GROUP;
                cell.follows_rules = true;
                cell.known = false;
                cell.state = false;
                cell_list.push_back(cell);
            }
        }
    }
}

// Constructor: creates a width x height grid with max_gen+1 generations
VariablePattern::VariablePattern(int width, int height, int max_gen)
    : VariablePattern(Bounds({{0, width - 1}, {0, height - 1}, {0, max_gen}}))
{
}

void VariablePattern::shift_by(Point rel_shift) {
    auto [dx, dy, dt] = rel_shift;
    auto [xlims, ylims, tlims] = this->bounds;

    xlims.first += dx;
    xlims.second += dx;
    ylims.first += dy;
    ylims.second += dy;
    tlims.first += dt;
    tlims.second += dt;
    this->bounds = Bounds(xlims, ylims, tlims);
    x_min = xlims.first;  y_min = ylims.first;  t_min = tlims.first;

    for(auto& cell : this->cell_list) {
        auto [x, y, t] = cell.position;
        cell.position = Point(x + dx, y + dy, t + dt);
    }

    is_built = false;  // Need to rebuild after shift
}

int VariablePattern::add_cell_group(const CellGroup& group) {
    cell_groups.push_back(group);
    is_built = false;
    return cell_groups.size() - 1;
}

int VariablePattern::add_cell_group(AffineTransf time_transformation) {
    CellGroup group;
    group.time_transformation = time_transformation;
    return add_cell_group(group);
}

Cell* VariablePattern::get_cell_ptr(Point p) {
    int idx = cell_index(p);
    if (idx < 0) return nullptr;
    return &cell_list[idx];
}

void VariablePattern::set_cell_group(Point p, int group_idx) {
    Cell* cell = get_cell_ptr(p);
    if(cell) {
        cell->cell_group = group_idx;
        is_built = false;
    }
}

void VariablePattern::set_cell_group_if(int group_idx, std::function<bool(const Cell&)> predicate) {
    for(auto& cell : cell_list) {
        if(predicate(cell)) cell.cell_group = group_idx;
    }
    is_built = false;
}

void VariablePattern::set_known(Point p, bool state) {
    Cell* cell = get_cell_ptr(p);
    if(cell) {
        cell->known = true;
        cell->state = state;
        is_built = false;
    }
}

void VariablePattern::set_dead(Point p) {
    set_known(p, false);
}

void VariablePattern::set_alive(Point p) {
    set_known(p, true);
}

void VariablePattern::set_known_if(bool state, std::function<bool(const Cell&)> predicate) {
    for(auto& cell : cell_list) {
        if(predicate(cell)) {
            cell.known = true;
            cell.state = state;
        }
    }
    is_built = false;
}

bool VariablePattern::is_boundary(Point p) const {
    auto [x, y, t] = p;
    auto [xlims, ylims, tlims] = bounds;
    return x == xlims.first || x == xlims.second ||
           y == ylims.first || y == ylims.second;
}

Cell VariablePattern::get_cell(Point p) const {
    int idx = cell_index(p);
    if (idx >= 0) return cell_list[idx];
    Cell default_cell;
    default_cell.position = p;
    default_cell.cell_group = DEFAULT_CELL_GROUP;
    default_cell.follows_rules = true;
    default_cell.known = false;
    default_cell.state = false;
    return default_cell;
}

void VariablePattern::build() {
    UnionFind<Point, PointHash> uf;
    uf.reserve(cell_list.size() + 2);  // +2 for sentinels

    Point live_cell = Point(-9999, -9999, -9999);  // sentinel for live cells
    Point dead_cell = Point(-9998, -9998, -9998);  // sentinel for dead cells
    uf.make_set(live_cell);
    uf.make_set(dead_cell);

    // Initialize union-find: each cell starts as its own representative
    for (const Cell& cell : cell_list) {
        uf.make_set(cell.position);
        if (cell.known && cell.state) {
            uf.unite(cell.position, live_cell);
        } else if (cell.known && !cell.state) {
            uf.unite(cell.position, dead_cell);
        }
    }

    // Apply transformations from cell groups
    for (const Cell& cell : cell_list) {
        if (cell.cell_group == DEFAULT_CELL_GROUP)
            continue;

        const CellGroup& cell_group = cell_groups[cell.cell_group];
        Point cell_pos = cell.position;

        // Spatial transformations
        std::set<Point> images = find_all_images(cell_pos,
            cell_group.spatial_transformations,
            bounds
        );
        for (Point img : images) {
            const Cell& target = get_cell(img);
            // Only link if target's priority <= source's priority
            // Never link to DEFAULT_CELL_GROUP cells
            if (target.cell_group != DEFAULT_CELL_GROUP && target.cell_group <= cell.cell_group)
                uf.unite(cell_pos, img);
        }

        // Time transformation
        Point time_img = transform(cell_group.time_transformation, cell_pos);
        if (in_limits(time_img, bounds) && time_img != cell_pos) {
            const Cell& target = get_cell(time_img);
            if (target.cell_group != DEFAULT_CELL_GROUP && target.cell_group <= cell.cell_group)
                uf.unite(cell_pos, time_img);
        }
    }

    // Build cell_to_var mapping
    cell_to_var.clear();
    cell_to_var.reserve(cell_list.size());
    std::unordered_map<Point, int, PointHash> repr_to_varindex;
    repr_to_varindex.reserve(cell_list.size());
    repr_to_varindex[uf.find(dead_cell)] = 0;
    repr_to_varindex[uf.find(live_cell)] = 1;
    int next_var_index = 2;

    for (const Cell& cell : cell_list) {
        Point cell_pos = cell.position;
        Point cell_repr = uf.find(cell_pos);
        if (repr_to_varindex.find(cell_repr) == repr_to_varindex.end()) {
            repr_to_varindex[cell_repr] = next_var_index;
            next_var_index++;
        }
        cell_to_var[cell_pos] = repr_to_varindex[cell_repr];
    }

    variable_count = next_var_index - 2;  // Don't count 0 and 1
    is_built = true;
}

ClauseList VariablePattern::get_clauses(int base_var_index) const {
    assert(is_built);

    auto [xlims, ylims, tlims] = bounds;
    ClauseList clauses;
    clauses.reserve(variable_count * 400);  // ~360 clauses/var empirically
    ClauseBuilder clause;
    std::array<int, 10> ten_cells{};

    for (int t = tlims.first; t < tlims.second; t++) {
        for (int y = ylims.first; y <= ylims.second; y++) {
            for (int x = xlims.first; x <= xlims.second; x++) {
                Point p(x, y, t + 1);

                // Skip if output cell doesn't follow rules
                if (!follows_rules(p))
                    continue;

                // Helper to convert local cell value to global
                auto to_global = [base_var_index](int local_val) -> int {
                    if (local_val < 2) return local_val;  // 0 = dead, 1 = alive
                    return base_var_index + (local_val - 2);
                };

                // Gather neighborhood (9 cells at time t)
                // Out-of-bounds cells are treated as dead (0)
                int i = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        Point neighbor(x + dx, y + dy, t);
                        if (contains(neighbor)) {
                            ten_cells[i] = to_global(get_cell_value(neighbor));
                        } else {
                            ten_cells[i] = 0;  // Out of bounds = dead
                        }
                        i++;
                    }
                }
                // Output cell at time t+1
                ten_cells[9] = to_global(get_cell_value(p));

                // Generate clauses from prime implicants
                for (const auto [care, force] : primeImplicants) {
                    bool clause_satisfied = false;
                    for (int bit = 0; bit < 10; bit++) {
                        if (care & (1 << bit)) {
                            int var_index = ten_cells[bit];
                            if (var_index < 2) {  // known cell
                                bool force_state = (force & (1 << bit)) != 0;
                                bool cell_state = (var_index != 0);
                                if (cell_state == force_state)
                                    clause_satisfied = true;
                            } else {
                                // unknown cell: add literal
                                int sign = (force & (1 << bit)) ? 1 : -1;
                                clause_satisfied = clause.add(sign * (var_index - 1));
                            }
                            if (clause_satisfied)
                                break;
                        }
                    }
                    if (!clause_satisfied && !clause.empty())
                        clauses.emplace_back(clause.get());
                    clause.clear();
                }
            }
        }
    }
    return clauses;
}
