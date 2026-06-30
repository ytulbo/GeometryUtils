#include "geometry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void expectNear(double actual, double expected, double tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << "FAILED: " << message << " actual=" << actual
                  << " expected=" << expected << '\n';
        std::exit(1);
    }
}

void test_triangle_crosses_plane() {
    const Triangle triangle{
        Point{-1, 0, 0},
        Point{1, 0, 0},
        Point{0, 1, 0},
    };
    const Plane yzPlane{1, 0, 0, 0};

    expect(triangleIntersectingPlane(triangle, yzPlane).has_value(),
           "triangle with vertices on both sides should intersect plane");
}

void test_triangle_on_one_side_does_not_intersect_plane() {
    const Triangle triangle{
        Point{1, 0, 0},
        Point{2, 0, 0},
        Point{1, 1, 0},
    };
    const Plane yzPlane{1, 0, 0, 0};

    expect(!triangleIntersectingPlane(triangle, yzPlane).has_value(),
           "triangle fully on one side should not intersect plane");
}

void test_triangle_with_vertex_on_plane_intersects_plane() {
    const Triangle triangle{
        Point{0, 0, 0},
        Point{1, 0, 0},
        Point{1, 1, 0},
    };
    const Plane yzPlane{1, 0, 0, 0};

    expect(triangleIntersectingPlane(triangle, yzPlane).has_value(),
           "triangle with one vertex on plane should intersect plane");
}

void test_segment_plane_intersection() {
    const Point p1{-1, 2, 3};
    const Point p2{1, 2, 3};
    const Plane yzPlane{1, 0, 0, 0};

    const Point intersection = segmentPlaneIntersection(p1, p2, yzPlane);

    expectNear(intersection.x, 0.0, 1e-12, "segment intersection x");
    expectNear(intersection.y, 2.0, 1e-12, "segment intersection y");
    expectNear(intersection.z, 3.0, 1e-12, "segment intersection z");
}

void test_mesh_reading_from_ply(const char* meshPath) {
    const Mesh mesh{meshPath};

    expect(!mesh.vertices.empty(), "mesh should contain vertices");
    expect(!mesh.triangles.empty(), "mesh should contain triangular faces");
}

void test_mesh_plane_intersection_from_ply(const char* meshPath) {
    const Mesh mesh{meshPath};

    double minX = mesh.vertices.front().x;
    double maxX = mesh.vertices.front().x;
    for (const Point& vertex : mesh.vertices) {
        minX = std::min(minX, vertex.x);
        maxX = std::max(maxX, vertex.x);
    }

    const double middleX = (minX + maxX) / 2.0;
    const Plane cuttingPlane{1, 0, 0, -middleX};
    const std::vector<Segment> segments = cutMeshWithPlane(mesh, cuttingPlane);

    expect(!segments.empty(), "middle x plane should intersect girl.ply mesh");
}

void test_closed_contours_from_square_segments() {
    const std::vector<Segment> squareSegments{
        {{1, 0, 0}, {1, 1, 0}},
        {{0, 1, 0}, {0, 0, 0}},
        {{1, 1, 0}, {0, 1, 0}},
        {{0, 0, 0}, {1, 0, 0}},
    };

    const std::vector<std::vector<Segment>> contours = closedContours(squareSegments);

    expect(contours.size() == 1, "square segments should produce one closed contour");
    expect(contours.front().size() == 4, "square contour should contain four segments");
}

} // namespace

int main(int argc, char** argv) {
    test_triangle_crosses_plane();
    test_triangle_on_one_side_does_not_intersect_plane();
    test_triangle_with_vertex_on_plane_intersects_plane();
    test_segment_plane_intersection();
    test_closed_contours_from_square_segments();

    if (argc == 2) {
        test_mesh_reading_from_ply(argv[1]);
        test_mesh_plane_intersection_from_ply(argv[1]);
    } else {
        std::cout << "Skipping mesh-file tests; pass a mesh path to enable them\n";
    }

    std::cout << "triangle_plane_tests passed\n";
    return 0;
}
