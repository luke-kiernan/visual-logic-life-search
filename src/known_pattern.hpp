#pragma once
/*
KnownPattern: A fully-determined pattern where all cell states are known.
Inherits from SubPattern for use in SearchProblem composition.
*/

#include <set>
#include <utility>
#include <algorithm>
#include <string>
#include "sub_pattern.hpp"

class KnownPattern : public SubPattern {
    private:
        void add_next_gen(KnownPattern& pattern, int gen);
    public:
        std::set<Point> on_cells;
        Bounds bounds; // xlimits, ylimits, tlimits (no shift applied)
        Point shift;
        
        // Get the state (alive/dead) at a position
        bool get_state(Point p) const override {
            // on_cells has origin (0,0,0), but pattern has been shifted by 'shift'.
            return on_cells.find(p - shift) != on_cells.end();
        }

        void shift_by(Point rel_shift){
            this->shift = this->shift + rel_shift;
        }

        KnownPattern() : on_cells(), bounds(EMPTY_BOUNDS), shift(0,0,0) {}
        KnownPattern(std::string rle, int max_gen);
        void print_gen(int gen) const;

        // SubPattern interface implementation
        Bounds get_bounds() const override {
            return bounds + shift;
        }

        void build() override {
            // No-op: KnownPattern is already fully determined
        }

        int num_variables() const override {
            return 0;  // No variables in a known pattern
        }

        int get_cell_value(Point p) const override {
            // KnownPattern always returns 0 (dead) or 1 (alive)
            return get_state(p) ? 1 : 0;
        }

        bool is_known(Point p) const override {
            return true;  // All cells in a known pattern are known
        }

        bool follows_rules(Point p) const override {
            return true;  // All cells in a known pattern follow rules
        }

        ClauseList get_clauses(int base_var_index) const override {
            // No clauses needed - all cells are already determined
            return ClauseList();
        }
};


const KnownPattern EMPTY_PAT = KnownPattern();

