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
    // Pattern found by solver (Gen 0)
    // From the output, reading interior (x=0 to 4, y=-2 to 2):
    // y=-2: o.....
    // y=-1: .oo...
    // y=0:  ..o...
    // y=1:  o.o...
    // y=2:  oo....
    
    // Converting to RLE (starting at y=0 for RLE convention)
    // We need to shift pattern so y=-2 becomes y=0
    // Shift down by 2: y=-2->0, y=-1->1, y=0->2, y=1->3, y=2->4
    std::string rle = "o$b2o$2bo$ob2o$2o!";  // 5 rows
    
    std::cout << "=== VERIFYING SOLVER SOLUTION ===\n\n";
    std::cout << "Initial pattern (RLE: " << rle << "):\n\n";
    
    KnownPattern pattern(rle, 4);
    
    for (int gen = 0; gen <= 4; gen++) {
        print_simple(pattern, gen, 0, 5, 0, 4);
    }
    
    // Check glide-reflection symmetry
    // (x, y, t) -> (x+1, -y, t+2) means Gen 0 reflected and shifted should match Gen 2
    // With our coordinate system where pattern is at y=0 to 4 (shifted from y=-2 to 2)
    // The reflection axis is at y=2 (original y=0)
    // Reflection: y -> 4-y (maps 0->4, 1->3, 2->2, 3->1, 4->0)
    
    std::cout << "=== CHECKING SYMMETRY ===\n";
    std::cout << "Gen 0 reflected across y=2 and shifted right by 1 should match Gen 2:\n\n";
    
    std::cout << "Gen 0 (for reference):\n";
    print_simple(pattern, 0, 0, 5, 0, 4);
    
    std::cout << "Gen 0 reflected and shifted:\n";
    for (int y = 0; y <= 4; y++) {
        for (int x = 0; x <= 5; x++) {
            // Map (x, y) -> (x-1, 4-y) in Gen 0
            int src_x = x - 1;
            int src_y = 4 - y;
            bool val = (src_x >= 0) && pattern.get_state({src_x, src_y, 0});
            std::cout << (val ? 'o' : '.');
        }
        std::cout << '\n';
    }
    std::cout << '\n';
    
    std::cout << "Gen 2 (actual):\n";
    print_simple(pattern, 2, 0, 5, 0, 4);
    
    return 0;
}
