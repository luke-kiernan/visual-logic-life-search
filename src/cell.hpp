#pragma once
#include <vector>
#include "cell_group.hpp"

struct Cell {
    Point position;
    int cell_group; // really a pointer to a CellGroup, but indices are easier to manage.
    bool follows_rules;
    bool known;
    bool state; // only relevant if known == true.
};

bool is_live(Cell c) {
    return c.known && c.state;
}

bool is_dead(Cell c) {
    return c.known && !c.state;
}