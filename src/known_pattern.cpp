#include "known_pattern.hpp"

KnownPattern::KnownPattern(std::string rle, int max_gen){
    on_cells = std::set<Point>();
    int x = 0;
    int y = 0;
    int max_x = 0;
    int max_y = 0;
    int i = 0;
    int count = 0;
    while(i < rle.size()){
        char c = rle[i];
        if(c == 'x' || c == '#') // skip header and comments.
            while( i < rle.size() && rle[i] != '\n') i++;
        if(c >= '0' && c <= '9'){
            count = count * 10 + (c - '0');
        } else {
            if(count == 0) count = 1;
            if(c == 'b'){
                x += count;
            } else if(c == 'o'){
                for(int j = 0; j < count; j++){
                    on_cells.insert(Point(x, y, 0));
                    if(x > max_x) max_x = x;
                    x += 1;
                }
            } else if(c == '$'){
                y += count;
                x = 0;
            } else if(c == '!'){
                break;
            }
            count = 0;
        }
        i++;
    }
    bounds = Bounds({0, max_x}, {0, y}, {0, max_gen});
    shift = Point(0,0,0);
    for (int gen = 1; gen <= max_gen; gen++){
        add_next_gen(*this, gen);
    }
};

void update_bounds(std::pair<int, int>& bounds, int val){
    if(val < bounds.first) bounds.first = val;
    if(val > bounds.second) bounds.second = val;
}

// naive implementation of next generation computation.
void KnownPattern::add_next_gen(KnownPattern& pattern, int gen){
    std::set<Point> new_on_cells;
    std::pair<int, int> new_xbounds = {std::get<0>(pattern.bounds).first, std::get<0>(pattern.bounds).second};
    std::pair<int, int> new_ybounds = {std::get<1>(pattern.bounds).first, std::get<1>(pattern.bounds).second};
    for(int x = std::get<0>(pattern.bounds).first - 1; x <= std::get<0>(pattern.bounds).second + 1; x++){
        for(int y = std::get<1>(pattern.bounds).first - 1; y <= std::get<1>(pattern.bounds).second + 1; y++){
            Point p = Point(x,y,gen);
            int live_neighbors = 0;
            for(int dy = -1; dy <= 1; dy++){
                for(int dx = -1; dx <= 1; dx++){
                    if(dx == 0 && dy == 0) continue;
                    Point neighbor = Point(x + dx, y + dy, gen - 1);
                    if(pattern.get_state(neighbor)){
                        live_neighbors++;
                    }
                }
            }
            bool currently_alive = pattern.get_state({x, y, gen - 1});
            if( (currently_alive && live_neighbors == 2) || (live_neighbors == 3)){
                new_on_cells.insert(p);
                update_bounds(new_xbounds, x);
                update_bounds(new_ybounds, y);
            }
        }
    }
    // more efficient to extend?
    for(auto p : new_on_cells){
        pattern.on_cells.insert(p);
    }
    pattern.bounds = Bounds(new_xbounds, new_ybounds, {0, gen});
}

void KnownPattern::print_gen(int gen) const {
    Point shift = this->shift;
    int xmin = std::get<0>(bounds).first + std::get<0>(shift);
    int xmax = std::get<0>(bounds).second + std::get<0>(shift);
    int ymin = std::get<1>(bounds).first + std::get<1>(shift);
    int ymax = std::get<1>(bounds).second + std::get<1>(shift);
    printf("Generation %d:\n", gen);
    printf("shift at (%d, %d, %d)\n", std::get<0>(shift), std::get<1>(shift), std::get<2>(shift));
    for (int y = ymin; y <= ymax; y++) {
        for (int x = xmin; x <= xmax; x++) {
            if (get_state({x, y, gen})) {
                printf("o");
            } else if (x == 0 && y == 0) {
                printf("+");
            } else if (x == 0) {
                printf("|");
            } else if (y == 0) {
                printf("-");
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
    printf("\n\n\n\n\n\n");
}
