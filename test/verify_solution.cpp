#include <iostream>
#include "../src/known_pattern.cpp"

void print_simple(KnownPattern& pattern, int gen, int xmin, int xmax, int ymin, int ymax) {
    std::cout << "Gen " << gen << ":\n";
    for (int y = ymin; y <= ymax; y++) {
        for (int x = xmin; x <= xmax; x++) {
            std::cout << (pattern.get_state({x, y, gen}) ? 'o' : '.');
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

int main() {
    // The pattern found by the solver (Gen 0), converted to RLE
    // oo...o
    // .ooo..
    // o...oo
    // .ooo..
    // oo...o
    std::string rle = "2o3bo$b3o2b$o3b2o$b3o2b$2o3bo!";

    std::cout << "=== REAL B3/S23 EVOLUTION ===\n\n";
    KnownPattern pattern(rle, 4);

    // Print with y from 0 to 4 (as in original RLE)
    for (int gen = 0; gen <= 4; gen++) {
        print_simple(pattern, gen, 0, 5, 0, 4);
    }

    return 0;
}
