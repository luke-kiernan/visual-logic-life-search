#pragma once
#include <vector>
#include <assert.h>


// copy-pasted from scourge, https://gitlab.com/terezi/scourge logic.h file.

// x -> r is the CGoL rules, where 9 bits of x are 3x3 neighborhood.
// so table is truth table for CGoL, where x + 512 * r corresponds to input x and output r. 
std::vector<int> table = []() -> std::vector<int> {
  std::vector<int> table(1024);
  for (int x = 0; x < 512; x++) {
    int r = abs(__builtin_popcount(x & 495) * 2 + (x >> 4) % 2 - 6) <= 1;
    table[x + r * 512] = 1;
  }
  return table;
}();

// this prime implication computation expresses (not CGoL) as (OR of ANDs), 
// which then gives us CGoL as (AND of ORs).
// each of our CNF clauses is OR over care bits i of (x_i != force_i)
// where 9 LSB of x are neighborhood and 10th is next gen state.
std::vector<std::pair<int, int>> primeImplicants = []() {
  std::vector<std::pair<int, int>> ans;
  for (int care = 1; care < 1024; care++) {
    for (int force = care;; force = (force - 1) & care) { // force is a subset of care.
      for (int x = care; x < 1024; x = (x + 1) | care) // x is a superset of care.
        if (table[x ^ force])
          goto no;
      for (auto [a, b] : ans) // minimality check.
        if ((a & care) == a && (a & force) == b)
          goto no;
      ans.push_back({care, force});
    no:;
      if (force == 0)
        break;
    }
  }
  for (int i = 0; i < 1024; i++) {
    bool s = 1;
    for (auto [a, b] : ans)
      s = s && (a & (~(i ^ b)));
    assert(s == table[i]);
  }
  return ans;
}();