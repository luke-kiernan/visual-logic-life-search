#pragma once
#include <vector>
#include "known_pattern.hpp"

struct CellGroup {
    // ambiguity: what if the reference pattern is inconsistent with the transformations?
    std::vector<AffineTransf> spatial_transformations;
    AffineTransf time_transformation;
    bool has_reference_pattern;
    KnownPattern reference_pat;

    CellGroup() : spatial_transformations(), time_transformation(IDENTITY), has_reference_pattern(false), reference_pat(EMPTY_PAT) {}
};

const int DEFAULT_CELL_GROUP = -1;