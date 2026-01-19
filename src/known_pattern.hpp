#pragma once
/*
A class that represents a known pattern that follows the rules;
used for creating search patterns via inserting unknowns among knowns.
*/

#include <set>
#include <utility>
#include <algorithm>
#include <string>
#include "geometry.hpp"

class KnownPattern {
    private:
        void add_next_gen(KnownPattern& pattern, int gen);
    public:
        std::set<Point> on_cells;
        Bounds bounds; // xlimits, ylimits, tlimits (no shift applied)
        Point shift;
        bool get_state(Point p) const {
            // on_cells has origin (0,0,0), but pattern has been shifted by 'shift'.
            return on_cells.find(p - shift) != on_cells.end();
        }
        void shift_by(Point rel_shift){
            this->shift = this->shift + rel_shift;
        }
        KnownPattern() : on_cells(), bounds(EMPTY_BOUNDS), shift(0,0,0) {}
        KnownPattern(std::string rle, int max_gen);
        void print_gen(int gen) const;
};


const KnownPattern EMPTY_PAT = KnownPattern();

