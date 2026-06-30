#include <iostream>
#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

double length(const Segment& segment) {
    return std::hypot(
        segment.p2.x - segment.p1.x,
        segment.p2.y - segment.p1.y,
        segment.p2.z - segment.p1.z
    );
}

int main(int argc, char** argv){
    if (argc != 2) {
        std::cerr << "Usage: geometry <mesh.ply>\n";
        return 1;
    }

    Mesh mesh(argv[1]);
    const auto& vs = mesh.vertices;

    if (vs.empty()) {
        std::cerr << "Mesh has no vertices: " << argv[1] << '\n';
        return 1;
    }

    double ymax=std::ranges::max_element(vs, [](const Point& a, const Point& b) { return a.y < b.y; })->y;
    double ymin=std::ranges::min_element(vs, [](const Point& a, const Point& b) { return a.y < b.y; })->y;
    Plane plane(0, 1, 0, -0.5 * (ymax + ymin));
    std::vector<Segment> intersectionSegments = cutMeshWithPlane(mesh, plane);

    if (intersectionSegments.empty()) {
        std::cout << "No intersection segments found.\n";
        return 0;
    }

    auto maxSegment = std::ranges::max_element(intersectionSegments, {}, length);
    auto minSegment = std::ranges::min_element(intersectionSegments, {}, length);
    std::cout << "Shortest segment length: " << length(*minSegment) << std::endl;
    std::cout << "Longest segment length: " << length(*maxSegment) << std::endl;
    std::vector<std::vector<Segment>> contours = closedContours(intersectionSegments);

    // Output the number of closed contours found
    std::cout << "Number of closed contours found: " << contours.size() << std::endl;

    return 0;
}
