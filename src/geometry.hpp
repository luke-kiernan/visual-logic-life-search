#pragma once
#include <tuple>
#include <utility>
#include <set>
#include <functional>

/* POINT: represents (x, y, t) coordinates, or sometimes a vector in (x, y, t) space */
using Point = std::tuple<int, int, int>;

// Hash function for Point (for use with unordered containers)
struct PointHash {
    size_t operator()(const Point& p) const {
        auto [x, y, t] = p;
        size_t h = std::hash<int>{}(x);
        h ^= std::hash<int>{}(y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

Point operator +(Point p1, Point p2) {
    auto [x1, y1, t1] = p1;
    auto [x2, y2, t2] = p2;
    return {x1 + x2, y1 + y2, t1 + t2};
}

Point operator -(Point p1, Point p2) {
    auto [x1, y1, t1] = p1;
    auto [x2, y2, t2] = p2;
    return {x1 - x2, y1 - y2, t1 - t2};
}


/*
AFFINE TRANSFORMATION:
represents v -> Av + b, an affine transformation on (x, y, t) space, where
A = [a1 a2 0; a3 a4 0; 0 0 1] and b = (a5, a6, a7).
*/
using AffineTransf = std::tuple<int, int, int, int, int, int, int>;


const AffineTransf IDENTITY = {1, 0, 0, 1, 0, 0, 0};


Point transform(AffineTransf transf, Point p) {
    auto [a1, a2, a3, a4, a5, a6, a7] = transf;
    auto [x, y, t] = p;

    int new_x = a1 * x + a2 * y + a5;
    int new_y = a3 * x + a4 * y + a6;
    int new_t = t + a7;

    return {new_x, new_y, new_t};
}

bool spatial_only(AffineTransf transf) {
    return std::get<6>(transf) == 0;
}

/* BOUNDS: represents rectangular bounds in (x, y, t) space. */


using Limits = std::pair<int, int>;
const Limits EMPTY_LIMITS = {0, -1};

using Bounds = std::tuple<Limits, Limits, Limits>; // xlimits, ylimits, tlimits

bool in_limits(Point p,
    Bounds bounds
) {
    auto [x, y, t] = p;
    auto [xlimits, ylimits, tlimits] = bounds;
    return (x >= xlimits.first && x <= xlimits.second &&
            y >= ylimits.first && y <= ylimits.second &&
            t >= tlimits.first && t <= tlimits.second);
}

Bounds operator +(Bounds b, Point p) {
    auto [dx, dy, dt] = p;
    auto [xlimits, ylimits, tlimits] = b;
    Limits new_xlimits = {xlimits.first + dx, xlimits.second + dx};
    Limits new_ylimits = {ylimits.first + dy, ylimits.second + dy};
    Limits new_tlimits = {tlimits.first + dt, tlimits.second + dt};
    return {new_xlimits, new_ylimits, new_tlimits};
}

Bounds operator -(Bounds b, Point p) {
    auto [dx, dy, dt] = p;
    auto [xlimits, ylimits, tlimits] = b;
    Limits new_xlimits = {xlimits.first - dx, xlimits.second - dx};
    Limits new_ylimits = {ylimits.first - dy, ylimits.second - dy};
    Limits new_tlimits = {tlimits.first - dt, tlimits.second - dt};
    return {new_xlimits, new_ylimits, new_tlimits};
}

const Bounds EMPTY_BOUNDS = {EMPTY_LIMITS, EMPTY_LIMITS, EMPTY_LIMITS};


// probably belongs in some other file, but putting it here for now.
std::set<Point> find_new_images(const std::set<Point>& points,
    const std::vector<AffineTransf>& transf_list,
    const Bounds bounds
) {
    std::set<Point> new_points;
    for (const auto& p : points)
        for (const auto& transf : transf_list) {
            auto transformed = transform(transf, p);
            if (points.find(transformed) == points.end() &&
                    in_limits(transformed, bounds))
                new_points.insert(transformed);
        }
    return new_points;
}

std::set<Point> find_all_images(const Point p,
    const std::vector<AffineTransf>& transf_list,
    const Bounds bounds
) {
    std::set<Point> images;
    images.insert(p);
    std::set<Point> new_images = find_new_images(images, transf_list, bounds);
    while (!new_images.empty()) {
        images.insert(new_images.begin(), new_images.end());
        new_images = find_new_images(images, transf_list, bounds);
    }
    return images;
}