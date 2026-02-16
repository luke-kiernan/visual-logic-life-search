#pragma once
#include <vector>
#include "geometry.hpp"

/*
CellGroup: Defines symmetry constraints for a group of cells.

- spatial_transformations: Symmetries within each generation (e.g., reflection, rotation)
- time_transformation: How cells map between generations (e.g., t -> t+1 for stable patterns)

Cell groups are used by VariablePattern to determine which cells share the same variable.
*/
struct CellGroup {
    std::vector<AffineTransf> spatial_transformations;
    AffineTransf time_transformation;

    CellGroup() : spatial_transformations(), time_transformation(IDENTITY) {}
};

const int DEFAULT_CELL_GROUP = -1;