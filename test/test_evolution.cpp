#include <cassert>
#include <iostream>
#include "../src/known_pattern.cpp"

// Helper to check if a set of points matches the expected on-cells at a given generation
void assert_generation_matches(KnownPattern& pattern, int gen,
                                const std::set<Point>& expected_on) {
    // Check that all expected cells are on
    for (const auto& p : expected_on) {
        Point pt = {std::get<0>(p), std::get<1>(p), gen};
        assert(pattern.get_state(pt) == true);
    }
}

bool check_square(KnownPattern& pattern, bool state, std::pair<int,int> top_left, std::pair<int,int> size, int gen) {
    for(int x = top_left.first; x < top_left.first + size.first; x++) {
        for(int y = top_left.second; y < top_left.second + size.second; y++) {
            if(pattern.get_state({x, y, gen}) != state) {
                return false;
            }
        }
    }
    return true;
}

void test_shift() {
    std::string block = "2o$2o!";
    KnownPattern pattern(block, 2);
    assert(pattern.shift == Point(0,0,0));
    assert(check_square(pattern, true, {0,0}, {2,2}, 0));
    pattern.shift_by({-2,-4,0});
    pattern.print_gen(0);
    assert(check_square(pattern, true, {-2,-4}, {2,2}, 0));
    std::cout << "PASSED: test_shift\n";
}


// Test a blinker oscillates with period 2
// Gen 0:  ooo     Gen 1:  .o.
//                         .o.
//                         .o.
void test_blinker_oscillation() {
    std::string blinker_rle = "3o!";
    KnownPattern pattern(blinker_rle, 4);
    pattern.print_gen(0);

    // Generation 0: horizontal
    assert(pattern.get_state({0, 0, 0}) == true);
    assert(pattern.get_state({1, 0, 0}) == true);
    assert(pattern.get_state({2, 0, 0}) == true);
    assert(pattern.get_state({1, -1, 0}) == false);
    assert(pattern.get_state({1, 1, 0}) == false);

    // Generation 1: vertical
    assert(pattern.get_state({0, 0, 1}) == false);
    assert(pattern.get_state({1, -1, 1}) == true);
    assert(pattern.get_state({1, 0, 1}) == true);
    assert(pattern.get_state({1, 1, 1}) == true);
    assert(pattern.get_state({2, 0, 1}) == false);

    // Generation 2: horizontal again
    assert(pattern.get_state({0, 0, 2}) == true);
    assert(pattern.get_state({1, 0, 2}) == true);
    assert(pattern.get_state({2, 0, 2}) == true);
    assert(pattern.get_state({1, -1, 2}) == false);

    // Generation 3: vertical again
    assert(pattern.get_state({1, -1, 3}) == true);
    assert(pattern.get_state({1, 0, 3}) == true);
    assert(pattern.get_state({1, 1, 3}) == true);

    std::cout << "PASSED: test_blinker_oscillation\n";
}


bool check_gens_match(KnownPattern& pattern1, KnownPattern& pattern2, int gen1, int gen2) {
    for(int x = std::get<0>(pattern1.bounds).first; x <= std::get<0>(pattern1.bounds).second; x++) {
        for(int y = std::get<1>(pattern1.bounds).first; y <= std::get<1>(pattern1.bounds).second; y++) {
            if(pattern1.get_state({x, y, gen1}) != pattern2.get_state({x, y, gen2})) {
                return false;
            }
        }
    }
    return true;
}

void test_honeyfarm_evolution() {
    std::string honeyfarm = "3o$o2bo$b2o!";
    KnownPattern pattern(honeyfarm, 20);
    pattern.shift_by({-1, 0, 0});
    pattern.print_gen(0);

    // Block should remain stable for all generations
    check_square(pattern, true, {-1, -5}, {2, 3}, 16);
    check_square(pattern, true, {-1, 4}, {2, 3}, 16);
    check_square(pattern, true, {-5, -1}, {3, 2}, 16);
    check_square(pattern, true, {4, -1}, {3, 2}, 16);

    std::string final_state = "x = 13, y = 13, rule = B3/S23\n6bo$5bobo$5bobo$6bo2$b2o7b2o$o2bo5bo2bo$b2o7b2o2$6bo$5bobo$5bobo$6bo!";
    KnownPattern expected_pattern(final_state, 1);
    assert(check_gens_match(expected_pattern, expected_pattern, 0, 1));
    pattern.print_gen(17);
    expected_pattern.shift_by({-6, -6, 0});
    expected_pattern.print_gen(0);
    assert(check_gens_match(pattern, expected_pattern, 17, 0));
    assert(check_gens_match(pattern, expected_pattern, 20, 0));

    std::cout << "PASSED: test_pre_beehive_evolution (block stability)\n";
}

// Test glider movement over 4 generations
// A glider moves diagonally and returns to its original shape after 4 gens
void test_glider_evolution() {
    // Glider RLE: bo$2bo$3o!
    // Shape:  .o.
    //         ..o
    //         ooo
    std::string glider_rle = "bo$2bo$3o!";
    KnownPattern pattern(glider_rle, 4);

    // Gen 0 cells
    assert(pattern.get_state({1, 0, 0}) == true);
    assert(pattern.get_state({2, 1, 0}) == true);
    assert(pattern.get_state({0, 2, 0}) == true);
    assert(pattern.get_state({1, 2, 0}) == true);
    assert(pattern.get_state({2, 2, 0}) == true);

    // Gen 4: glider should have moved 1 cell right and 1 cell down
    // and returned to same orientation
    assert(pattern.get_state({2, 1, 4}) == true);  // was (1,0)
    assert(pattern.get_state({3, 2, 4}) == true);  // was (2,1)
    assert(pattern.get_state({1, 3, 4}) == true);  // was (0,2)
    assert(pattern.get_state({2, 3, 4}) == true);  // was (1,2)
    assert(pattern.get_state({3, 3, 4}) == true);  // was (2,2)

    std::cout << "PASSED: test_glider_evolution\n";
}

int main() {
    test_shift();
    test_blinker_oscillation();
    test_honeyfarm_evolution();
    test_glider_evolution();

    std::cout << "\nAll evolution tests passed!\n";
    return 0;
}
