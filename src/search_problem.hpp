#pragma once
/*
SearchProblem: Composes multiple SubPatterns into a masked search.

Each SubPattern is associated with:
- A mask function: determines which (x,y,t) positions use this subpattern

The SearchProblem:
1. Validates that masks completely cover the bounds (no gaps, explicit assignment)
2. Builds each SubPattern
3. Generates the composite variable grid
4. Generates all GoL transition clauses (using SubPatterns for cell values)
*/

#include <vector>
#include <functional>
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include "sub_pattern.hpp"
#include "sat_logic.hpp"
#include "union_find.hpp"
#include "profiling.hpp"

// Hash function for transition signatures (center + 8 sorted neighbors)
struct SignatureHash {
    size_t operator()(const std::pair<int, std::array<int, 8>>& sig) const {
        size_t h = std::hash<int>{}(sig.first);
        for (int v : sig.second) {
            h ^= std::hash<int>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

const int OUTSIDE_BOUNDS_INDEX = INT_MIN;  // Special index for out-of-bounds cells
const int NOT_FOUND_INDEX = -1;    // Special index for uncovered cells

// TODO I dislike the special case for boundary cells. Bounary cells should be handled like any other cells,
// just we consider off-the-edge cells as "dead and don't follow rules."

// TODO since you can shift the VariablePattern itself, having another shift here seems redundant.
struct SubPatternEntry {
    SubPattern* pattern;
    std::function<bool(Point)> mask;  // Returns true if this entry provides the value at composite position
};

class SearchProblem {
private:
    Bounds bounds;
    std::vector<SubPatternEntry> entries;

    // Built state
    bool is_built = false;
    int total_variables = 0;
    std::vector<int> entry_base_var;  // Base variable index for each entry

    // Variable deduplication: maps old global var to new global var
    // var_remap[old_var - 2] = new_var (both use same 0=dead, 1=alive, >=2 convention)
    std::vector<int> var_remap;
    int remapped_num_vars = 0;

    // Precomputed flat arrays (populated during build, indexed by flat_index)
    int x_min, y_min, t_min;
    int sz_x, sz_y, sz_t;
    std::vector<int> raw_cell_values;       // result of get_raw_cell_value() for each in-bounds cell
    std::vector<bool> cell_follows_rules;   // result of follows_rules() for each in-bounds cell
    std::vector<int> remapped_cell_values;  // result of get_cell_value() for each in-bounds cell (after dedup)

    int flat_index(int x, int y, int t) const {
        return (t - t_min) * (sz_y * sz_x) + (y - y_min) * sz_x + (x - x_min);
    }

    // Fast lookups for hot loops (return 0 for out-of-bounds)
    int raw_value_at(int x, int y, int t) const {
        if (x < x_min || x >= x_min + sz_x || y < y_min || y >= y_min + sz_y || t < t_min || t >= t_min + sz_t)
            return 0;
        return raw_cell_values[flat_index(x, y, t)];
    }

    int remapped_value_at(int x, int y, int t) const {
        if (x < x_min || x >= x_min + sz_x || y < y_min || y >= y_min + sz_y || t < t_min || t >= t_min + sz_t)
            return 0;
        return remapped_cell_values[flat_index(x, y, t)];
    }

public:
    SearchProblem(Bounds bounds) : bounds(bounds) {}

    SearchProblem(int width, int height, int max_gen)
        : bounds({{0, width - 1}, {0, height - 1}, {0, max_gen}}) {}

    Bounds get_bounds() const { return bounds; }

    // Add a subpattern entry
    void add_entry(SubPattern* pattern, std::function<bool(Point)> mask) {
        entries.push_back({pattern, mask});
        is_built = false;
    }

    // Find which entry provides the value at a composite position
    int find_entry(Point p) const {
        if (!in_limits(p, bounds))
            return OUTSIDE_BOUNDS_INDEX;  // Out of bounds
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].mask(p))
                return i;
        }
        return NOT_FOUND_INDEX;  // No entry found
    }

    // Build all subpatterns and compute variable indices
    void build() {
        auto build_start = std::chrono::high_resolution_clock::now();

        auto [xlims, ylims, tlims] = bounds;
        x_min = xlims.first;  y_min = ylims.first;  t_min = tlims.first;
        sz_x = xlims.second - xlims.first + 1;
        sz_y = ylims.second - ylims.first + 1;
        sz_t = tlims.second - tlims.first + 1;
        size_t total_cells = size_t(sz_x) * sz_y * sz_t;

        // Precompute entry map and validate coverage (single pass over mask functions)
        std::vector<int> entry_map(total_cells);
        for (size_t fi = 0; fi < total_cells; fi++) {
            int x = x_min + fi % sz_x;
            int y = y_min + (fi / sz_x) % sz_y;
            int t = t_min + fi / (sz_x * sz_y);
            Point p(x, y, t);
            int entry_idx = NOT_FOUND_INDEX;
            for (size_t i = 0; i < entries.size(); i++) {
                if (entries[i].mask(p)) {
                    entry_idx = i;
                    break;
                }
            }
            if (entry_idx == NOT_FOUND_INDEX) {
                throw std::runtime_error("SearchProblem: not all cells are covered by masks");
            }
            entry_map[fi] = entry_idx;
        }

        auto t0 = std::chrono::high_resolution_clock::now();

        // Build each subpattern
        for (auto& entry : entries) {
            entry.pattern->build();
        }

        auto t1 = std::chrono::high_resolution_clock::now();

        // Compute base variable indices for each entry
        entry_base_var.clear();
        int next_var = 2;  // Start at 2 (0=dead, 1=alive internally, but SAT vars start at 1)
        for (const auto& entry : entries) {
            entry_base_var.push_back(next_var);
            next_var += entry.pattern->num_variables();
        }
        total_variables = next_var - 2;

        is_built = true;

        // Precompute raw cell values and follows_rules (one pass, no more mask calls)
        raw_cell_values.resize(total_cells);
        cell_follows_rules.resize(total_cells);
        for (size_t fi = 0; fi < total_cells; fi++) {
            int x = x_min + fi % sz_x;
            int y = y_min + (fi / sz_x) % sz_y;
            int t = t_min + fi / (sz_x * sz_y);
            int entry_idx = entry_map[fi];
            const auto& entry = entries[entry_idx];
            Point p(x, y, t);

            // Raw cell value
            int local_val = entry.pattern->get_cell_value(p);
            if (local_val < 2)
                raw_cell_values[fi] = local_val;
            else
                raw_cell_values[fi] = entry_base_var[entry_idx] + (local_val - 2);

            // Follows rules
            cell_follows_rules[fi] = entry.pattern->follows_rules(p);
        }

        // Deduplicate variables based on transition signatures
        deduplicate_transitions();

        // Precompute remapped cell values
        remapped_cell_values.resize(total_cells);
        for (size_t i = 0; i < total_cells; i++) {
            int raw = raw_cell_values[i];
            if (raw < 2)
                remapped_cell_values[i] = raw;
            else
                remapped_cell_values[i] = var_remap[raw - 2];
        }

        auto t2 = std::chrono::high_resolution_clock::now();

        auto pattern_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        auto dedup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - build_start).count();
        std::cout << "  Build phase: " << format_duration(build_ms)
                  << " (pattern builds: " << format_duration(pattern_ms)
                  << ", dedup transitions: " << format_duration(dedup_ms)
                  << ", " << total_variables << " vars before dedup, "
                  << remapped_num_vars << " after)\n";
    }

    // Get raw (pre-remapping) cell value
    int get_raw_cell_value(Point p) const {
        auto [x, y, t] = p;
        return raw_value_at(x, y, t);
    }

    // Deduplicate output variables based on transition signatures
    // Two transitions with same (center, multiset of neighbors) share the same output
    void deduplicate_transitions() {
        UnionFind<int> uf;
        uf.reserve(total_variables + 2);

        // Signature: (center, sorted 8 neighbors)
        using Signature = std::pair<int, std::array<int, 8>>;
        std::unordered_map<Signature, int, SignatureHash> sig_to_output;

        auto [xlims, ylims, tlims] = bounds;
        // Reserve space: estimate unique signatures as fraction of total transitions
        size_t num_transitions = size_t(sz_x) * sz_y * (tlims.second - tlims.first);
        sig_to_output.reserve(num_transitions / 2);  // Expect ~50% unique signatures
        for (int t = tlims.first; t < tlims.second; t++) {
            for (int y = ylims.first; y <= ylims.second; y++) {
                for (int x = xlims.first; x <= xlims.second; x++) {
                    // Skip outputs that don't follow rules
                    if (!cell_follows_rules[flat_index(x, y, t + 1)])
                        continue;

                    int output_val = raw_value_at(x, y, t + 1);

                    // Build signature: center + sorted neighbors
                    int center = raw_value_at(x, y, t);
                    std::array<int, 8> neighbors;
                    int idx = 0;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            neighbors[idx++] = raw_value_at(x + dx, y + dy, t);
                        }
                    }
                    std::sort(neighbors.begin(), neighbors.end());

                    Signature sig = {center, neighbors};
                    auto it = sig_to_output.find(sig);
                    if (it == sig_to_output.end()) {
                        sig_to_output[sig] = output_val;
                    } else if (output_val >= 2) {
                        // Current output is a variable - merge with recorded output
                        uf.unite(output_val, it->second);
                    } else {
                        // Both are known - they must match
                        if (it->second < 2 && it->second != output_val) {
                            std::string neighbors_str = "";
                            for (int n : neighbors)
                                neighbors_str += std::to_string(n) + " ";

                            throw std::runtime_error(
                                "SearchProblem: contradictory known outputs for same transition signature: position ("
                                + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(t) + "), center " +
                                std::to_string(center) + ", neighbors " + neighbors_str + ", output " + std::to_string(output_val) + " vs " + std::to_string(it->second));
                        }
                        // If recorded is a variable, unite it with this known value
                        if (it->second >= 2) {
                            uf.unite(it->second, output_val);
                        }
                    }
                }
            }
        }

        // Build remapping from union-find
        std::map<int, int> root_to_new;
        int next_new = 2;
        var_remap.resize(total_variables);

        for (int v = 2; v < 2 + total_variables; v++) {
            int root = uf.find(v);
            // If root is known (0 or 1), map directly to that value
            if (root < 2) {
                var_remap[v - 2] = root;
                continue;
            }
            if (root_to_new.find(root) == root_to_new.end()) {
                root_to_new[root] = next_new++;
            }
            var_remap[v - 2] = root_to_new[root];
        }

        remapped_num_vars = next_new - 2;
    }

    // Get the cell value at a composite position (with deduplication applied)
    // Returns: 0 = dead, 1 = alive, >= 2 = global variable index
    int get_cell_value(Point p) const {
        assert(is_built);
        auto [x, y, t] = p;
        return remapped_value_at(x, y, t);
    }

    int num_variables() const {
        assert(is_built);
        return remapped_num_vars;
    }

    // Get all clauses for the SAT problem
    // Generates GoL transition clauses for all cells in bounds
    ClauseList get_clauses() const {
        auto clause_start = std::chrono::high_resolution_clock::now();

        assert(is_built);
        auto [xlims, ylims, tlims] = bounds;
        ClauseList clauses;
        clauses.reserve(remapped_num_vars * 400);  // ~360 clauses/var empirically
        ClauseBuilder clause;
        std::array<int, 10> ten_cells{};

        for (int t = tlims.first; t < tlims.second; t++) {
            for (int y = ylims.first; y <= ylims.second; y++) {
                for (int x = xlims.first; x <= xlims.second; x++) {
                    // Check if output cell follows rules
                    if (!cell_follows_rules[flat_index(x, y, t + 1)])
                        continue;

                    // Gather neighborhood
                    int i = 0;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            ten_cells[i++] = remapped_value_at(x + dx, y + dy, t);
                        }
                    }
                    ten_cells[9] = remapped_value_at(x, y, t + 1);

                    // Generate clauses from prime implicants
                    for (const auto [care, force] : primeImplicants) {
                        bool clause_satisfied = false;
                        for (int bit = 0; bit < 10; bit++) {
                            if (care & (1 << bit)) {
                                int var_index = ten_cells[bit];
                                if (var_index < 2) {
                                    bool force_state = (force & (1 << bit)) != 0;
                                    bool cell_state = (var_index != 0);
                                    if (cell_state == force_state)
                                        clause_satisfied = true;
                                } else {
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

        auto clause_end = std::chrono::high_resolution_clock::now();
        auto clause_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clause_end - clause_start).count();
        std::cout << "  Clause generation: " << format_duration(clause_ms)
                  << " (" << clauses.size() << " clauses)\n";

        return clauses;
    }
};
