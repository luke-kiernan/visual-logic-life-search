#pragma once
/*
UnionFind: Generic union-find data structure with path compression.
Uses std::unordered_map for O(1) amortized lookups.
Requires a hash function for the key type (default std::hash works for primitives).
*/

#include <unordered_map>
#include <functional>

template<typename T, typename Hash = std::hash<T>>
class UnionFind {
private:
    std::unordered_map<T, T, Hash> parent;

public:
    // Reserve space for expected number of elements
    void reserve(size_t n) {
        parent.reserve(n);
    }

    // Ensure element exists in the structure
    void make_set(const T& x) {
        if (parent.find(x) == parent.end()) {
            parent[x] = x;
        }
    }

    // Find with path compression
    T find(const T& x) {
        if (parent.find(x) == parent.end()) {
            parent[x] = x;
            return x;
        }
        if (parent[x] != x) {
            parent[x] = find(parent[x]);
        }
        return parent[x];
    }

    // Union two elements (smaller root becomes parent for consistency)
    void unite(const T& a, const T& b) {
        T ra = find(a);
        T rb = find(b);
        if (ra != rb) {
            if (rb < ra) std::swap(ra, rb);
            parent[rb] = ra;
        }
    }

    // Check if two elements are in the same set
    bool same(const T& a, const T& b) {
        return find(a) == find(b);
    }
};
