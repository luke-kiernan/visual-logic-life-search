#pragma once
#include <vector>
#include <algorithm>
#include <functional>
#include "cell.hpp"

class SearchPattern {
    private:
        std::vector<CellGroup> cell_groups;
        std::vector<Cell> cell_list; // invariant: this is lexographically sorted by (x, y, t).
        Bounds bounds;

    public:
        SearchPattern(Bounds bounds);
        SearchPattern(int width, int height, int max_gen);

        // Getters
        Bounds get_bounds() const { return bounds; }
        const std::vector<Cell>& get_cells() const { return cell_list; }
        const std::vector<CellGroup>& get_cell_groups() const { return cell_groups; }
        Cell get_cell(Point p) const;

        // Modifiers
        void shift_by(Point rel_shift);
        int add_cell_group(const CellGroup& group);
        int add_cell_group(AffineTransf time_transformation);

        // Set cell group for a single cell
        void set_cell_group(Point p, int group_idx);
        // Set cell group for all cells matching a predicate
        void set_cell_group_if(int group_idx, std::function<bool(const Cell&)> predicate);

        // Set known state for a single cell
        void set_known(Point p, bool state);
        // Set known dead for a single cell
        void set_dead(Point p);
        // Set known alive for a single cell
        void set_alive(Point p);
        // Set known state for all cells matching a predicate
        void set_known_if(bool state, std::function<bool(const Cell&)> predicate);

        // Check if a point is on the boundary of the grid
        bool is_boundary(Point p) const;

    private:
        Cell* get_cell_ptr(Point p);
};

// Basic constructor: creates a width x height grid with max_gen+1 generations
// All cells are unknown and belong to a single default cell group
SearchPattern::SearchPattern(int width, int height, int max_gen)
    : bounds({{0, width - 1}, {0, height - 1}, {0, max_gen}})
{
    // Create all cells in the grid
    for (int t = 0; t <= max_gen; t++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
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

void SearchPattern::shift_by(Point rel_shift){
    auto [dx, dy, dt] = rel_shift;
    auto [xlims, ylims, tlims] = this->bounds;

    // Update bounds
    xlims.first += dx;
    xlims.second += dx;
    ylims.first += dy;
    ylims.second += dy;
    tlims.first += dt;
    tlims.second += dt;
    this->bounds = Bounds(xlims, ylims, tlims);

    // Update each cell's position
    for(auto& cell : this->cell_list){
        auto [x, y, t] = cell.position;
        cell.position = Point(x + dx, y + dy, t + dt);
    }
}

int SearchPattern::add_cell_group(const CellGroup& group){
    cell_groups.push_back(group);
    return cell_groups.size() - 1;
}

int SearchPattern::add_cell_group(AffineTransf time_transformation){
    CellGroup group;
    group.time_transformation = time_transformation;
    return add_cell_group(group);
}

Cell* SearchPattern::get_cell_ptr(Point p){
    for(auto& cell : cell_list){
        if(cell.position == p) return &cell;
    }
    return nullptr;
}

void SearchPattern::set_cell_group(Point p, int group_idx){
    Cell* cell = get_cell_ptr(p);
    if(cell) cell->cell_group = group_idx;
}

void SearchPattern::set_cell_group_if(int group_idx, std::function<bool(const Cell&)> predicate){
    for(auto& cell : cell_list){
        if(predicate(cell)) cell.cell_group = group_idx;
    }
}

void SearchPattern::set_known(Point p, bool state){
    Cell* cell = get_cell_ptr(p);
    if(cell){
        cell->known = true;
        cell->state = state;
    }
}

void SearchPattern::set_dead(Point p){
    set_known(p, false);
}

void SearchPattern::set_alive(Point p){
    set_known(p, true);
}

void SearchPattern::set_known_if(bool state, std::function<bool(const Cell&)> predicate){
    for(auto& cell : cell_list){
        if(predicate(cell)){
            cell.known = true;
            cell.state = state;
        }
    }
}

bool SearchPattern::is_boundary(Point p) const {
    auto [x, y, t] = p;
    auto [xlims, ylims, tlims] = bounds;
    return x == xlims.first || x == xlims.second ||
           y == ylims.first || y == ylims.second;
}

Cell SearchPattern::get_cell(Point p) const {
    for (const auto& cell : cell_list) {
        if (cell.position == p) return cell;
    }
    // Return a default cell if not found (shouldn't happen with valid points)
    Cell default_cell;
    default_cell.position = p;
    default_cell.cell_group = DEFAULT_CELL_GROUP;
    default_cell.follows_rules = true;
    default_cell.known = false;
    default_cell.state = false;
    return default_cell;
}