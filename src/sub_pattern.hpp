#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <stdexcept>
#include "geometry.hpp"

// Maximum literals in a GoL transition clause (from prime implicant analysis)
constexpr int MAX_CLAUSE_LEN = 9;

// Fixed-size clause for GoL transitions
// Literals are sorted, unused slots filled with sentinel 0
using Clause = std::array<int, MAX_CLAUSE_LEN>;
using ClauseList = std::vector<Clause>;

// For arbitrary-sized clauses (e.g., "at least one cell alive")
using BigClause = std::vector<int>;
using BigClauseList = std::vector<BigClause>;

// Create a Clause from up to MAX_CLAUSE_LEN literals (sorts and pads with 0)
inline Clause make_clause(std::initializer_list<int> lits) {
    Clause c{};  // Zero-initialized
    int i = 0;
    for (int lit : lits) {
        if (i < MAX_CLAUSE_LEN) c[i++] = lit;
    }
    std::sort(c.begin(), c.end());
    return c;
}

// Deduplicate a ClauseList in place
inline void deduplicate_clauses(ClauseList& clauses) {
    std::sort(clauses.begin(), clauses.end());
    clauses.erase(std::unique(clauses.begin(), clauses.end()), clauses.end());
}

// Helper for building clauses incrementally
// Detects tautologies (x and -x) and produces sorted Clause
class ClauseBuilder {
    Clause lits{};  // Zero-initialized
    int count = 0;
    bool tautology = false;

public:
    void clear() {
        for (int i = 0; i < count; i++) lits[i] = 0;
        count = 0;
        tautology = false;
    }

    // Add a literal. Returns true if clause becomes a tautology.
    bool add(int literal) {
        if (tautology) return true;
        // Check for tautology (x and -x)
        for (int i = 0; i < count; i++) {
            if (lits[i] == -literal) {
                tautology = true;
                return true;
            }
        }
        if (count >= MAX_CLAUSE_LEN) {
            throw std::runtime_error("ClauseBuilder: clause exceeds MAX_CLAUSE_LEN literals");
        }
        lits[count++] = literal;
        return false;
    }

    bool is_tautology() const { return tautology; }
    bool empty() const { return count == 0; }

    // Get sorted clause (call only if not tautology and not empty)
    Clause get() const {
        Clause c = lits;
        std::sort(c.begin(), c.begin() + count);
        return c;
    }
};

/*
SubPattern: Abstract base class representing a sequence of generations that follow GoL rules.

Two concrete implementations:
- KnownPattern: All cells are determined (alive or dead)
- VariablePattern: Some cells are unknown, with symmetry constraints via cell groups

Used by SearchProblem to compose multiple sub-patterns into a masked search.
*/

class SubPattern {
public:
    virtual ~SubPattern() = default;

    // === Pattern-level properties ===

    // Get the bounds of this pattern
    virtual Bounds get_bounds() const = 0;

    // Check if a point is within this pattern's bounds
    virtual bool contains(Point p) const {
        return in_limits(p, get_bounds());
    }

    // Check if a point is on the spatial boundary of the pattern
    virtual bool is_boundary(Point p) const {
        auto [x, y, t] = p;
        auto [xlims, ylims, tlims] = get_bounds();
        return x == xlims.first || x == xlims.second ||
               y == ylims.first || y == ylims.second;
    }

    // Prepare the pattern for querying.
    // For KnownPattern: no-op (already fully determined)
    // For VariablePattern: builds union-find structure for variable equivalences
    virtual void build() = 0;

    // Number of unique variable indices in this pattern (after build).
    // For KnownPattern: always 0
    // For VariablePattern: count of distinct variables after union-find
    virtual int num_variables() const = 0;

    // === Cell-level properties ===

    // Get the cell value at a position.
    // Returns: 0 = known dead, 1 = known alive, >= 2 = local variable index
    // Precondition: build() has been called, p is within bounds
    virtual int get_cell_value(Point p) const = 0;

    // Check if the cell state is known (true for KnownPattern, depends on cell for VariablePattern)
    virtual bool is_known(Point p) const = 0;

    // Get the known state of a cell (only valid if is_known returns true)
    virtual bool get_state(Point p) const = 0;

    // Check if the cell at p should follow GoL transition rules
    // (i.e., whether the cell at t+1 is constrained by the neighborhood at t)
    virtual bool follows_rules(Point p) const = 0;

    // === Clause generation ===

    // Generate all GoL transition clauses internal to this subpattern.
    // Only generates clauses where all 9 neighborhood cells + offspring are within bounds.
    // Variable indices are offset by base_var_index.
    // Precondition: build() has been called
    virtual ClauseList get_clauses(int base_var_index) const = 0;
};
