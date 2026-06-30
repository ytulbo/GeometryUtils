#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

struct Point {
    double x{};
    double y{};
    double z{};

    constexpr Point operator+(const Point& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Point operator*(double scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
};

struct Segment {
    Point p1{};
    Point p2{};
};

struct Triangle {
    Point a{};
    Point b{};
    Point c{};
};

struct Plane {
    double a{};
    double b{};
    double c{};
    double d{};

    Plane(double a, double b, double c, double d);
};

struct Mesh {
    std::vector<Point> vertices;
    std::vector<std::array<int,3>> triangles;

    Mesh() = default;
    explicit Mesh(const std::filesystem::path& filename);

    [[nodiscard]] auto begin() const { return triangles.begin(); }
    [[nodiscard]] auto end() const { return triangles.end(); }
    [[nodiscard]] auto begin() { return triangles.begin(); }
    [[nodiscard]] auto end() { return triangles.end(); }
};

using Edge = std::array<int, 2>;
using TrianglePlaneIntersection = std::array<Edge, 2>;

[[nodiscard]] Point segmentPlaneIntersection(const Point& p1, const Point& p2, const Plane& pl);
[[nodiscard]] bool isPointInTriangle(const Point& p, const Triangle& t);
[[nodiscard]] double signedDistanceFromPointToPlane(const Point& p, const Plane& pl);
[[nodiscard]] double distanceFromPointToPlane(const Point& p, const Plane& pl);
[[nodiscard]] std::optional<TrianglePlaneIntersection> triangleIntersectingPlane(const Triangle& t, const Plane& p);
[[nodiscard]] bool isTriangleVertexOnPlane(const Triangle& t, const Plane& p);
[[nodiscard]] std::vector<Segment> cutMeshWithPlane(const Mesh& m, const Plane& p);
[[nodiscard]] std::vector<std::vector<Segment>> closedContours(std::span<const Segment> meshSegments);
