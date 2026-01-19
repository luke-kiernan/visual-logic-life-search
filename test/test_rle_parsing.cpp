#include <cassert>
#include <iostream>
#include "../src/known_pattern.cpp"

// Test that headers (lines starting with 'x') are skipped
void test_skips_header() {
    // RLE with a header line: "x = 3, y = 3" followed by a blinker
    std::string rle_with_header = "x = 3, y = 1\n3o!";
    KnownPattern pattern(rle_with_header, 0);

    // Should parse the blinker (3 horizontal cells)
    assert(pattern.get_state({0, 0, 0}) == true);
    assert(pattern.get_state({1, 0, 0}) == true);
    assert(pattern.get_state({2, 0, 0}) == true);
    assert(pattern.get_state({3, 0, 0}) == false);

    std::cout << "PASSED: test_skips_header\n";
}

// Test that comments (lines starting with '#') are skipped
void test_skips_comments() {
    // RLE with comment lines
    std::string rle_with_comments = "#C This is a comment\n#N Pattern name\n3o!";
    KnownPattern pattern(rle_with_comments, 0);

    // Should parse the 3-cell horizontal line
    assert(pattern.get_state({0, 0, 0}) == true);
    assert(pattern.get_state({1, 0, 0}) == true);
    assert(pattern.get_state({2, 0, 0}) == true);
    assert(pattern.get_state({3, 0, 0}) == false);

    std::cout << "PASSED: test_skips_comments\n";
}

// Test with both header and comments
void test_skips_header_and_comments() {
    std::string rle = "#C Comment line 1\n#N Name\nx = 2, y = 2\n2o$2o!";
    KnownPattern pattern(rle, 0);

    // Should parse a 2x2 block
    assert(pattern.get_state({0, 0, 0}) == true);
    assert(pattern.get_state({1, 0, 0}) == true);
    assert(pattern.get_state({0, 1, 0}) == true);
    assert(pattern.get_state({1, 1, 0}) == true);
    assert(pattern.get_state({2, 0, 0}) == false);

    std::cout << "PASSED: test_skips_header_and_comments\n";
}

int main() {
    test_skips_header();
    test_skips_comments();
    test_skips_header_and_comments();

    std::cout << "\nAll RLE parsing tests passed!\n";
    return 0;
}
