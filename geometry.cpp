#include "geometry.hpp"
#include "threadpool.hpp"
#include "happly.h"

#include <cmath>
#include <deque>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <thread>

namespace {

constexpr double geometryEpsilon = 1e-8;

bool almostEqual(const Point& a, const Point& b, double epsilon = geometryEpsilon)
{
    return std::abs(a.x - b.x) <= epsilon &&
           std::abs(a.y - b.y) <= epsilon &&
           std::abs(a.z - b.z) <= epsilon;
}

double segmentLength(const Segment& segment)
{
    return std::hypot(
        segment.p2.x - segment.p1.x,
        segment.p2.y - segment.p1.y,
        segment.p2.z - segment.p1.z
    );
}

void addUniquePoint(std::vector<Point>& points, const Point& point)
{
    for (const Point& existing : points) {
        if (almostEqual(existing, point)) {
            return;
        }
    }
    points.push_back(point);
}

bool edgeIntersectsPlane(double d1, double d2)
{
    return (d1 <= geometryEpsilon && d2 >= -geometryEpsilon) ||
           (d1 >= -geometryEpsilon && d2 <= geometryEpsilon);
}

} // namespace

Plane::Plane(double a, double b, double c, double d) : a(a), b(b), c(c), d(d) {
    const double length = std::sqrt(a * a + b * b + c * c);
    if (length != 0) {
        this->a /= length;
        this->b /= length;
        this->c /= length;
        this->d /= length;
    }
}

Mesh::Mesh(const std::filesystem::path& filename)
{
    happly::PLYData plyIn(filename.string());
    vertices.reserve(plyIn.getVertexPositions().size());
    for (const auto& vertex : plyIn.getVertexPositions()) {
        vertices.push_back({vertex[0], vertex[1], vertex[2]});
    }
    triangles.reserve(plyIn.getFaceIndices().size());
    for (const auto& face : plyIn.getFaceIndices()) {
        if (face.size() == 3) {
            triangles.push_back({static_cast<int>(face[0]), static_cast<int>(face[1]), static_cast<int>(face[2])});
        }
    }
}

Point segmentPlaneIntersection(const Point &p1, const Point &p2, const Plane &pl)
{
    const double d1 = signedDistanceFromPointToPlane(p1, pl);
    const double d2 = signedDistanceFromPointToPlane(p2, pl);
    return p1 * (d2 / (d2 - d1)) + p2 * (-d1 / (d2 - d1));
}


bool isPointInTriangle(const Point &p, const Triangle &t)
{
    (void)p;
    (void)t;
    return true;
}

double signedDistanceFromPointToPlane(const Point& p, const Plane& pl) {
    return pl.a * p.x + pl.b * p.y + pl.c * p.z + pl.d;
}

double distanceFromPointToPlane(const Point& p, const Plane& pl) {
    return std::abs(signedDistanceFromPointToPlane(p, pl));
}

std::optional<TrianglePlaneIntersection> triangleIntersectingPlane(const Triangle& t, const Plane& p) {
    const double da = signedDistanceFromPointToPlane(t.a, p);
    const double db = signedDistanceFromPointToPlane(t.b, p);
    const double dc = signedDistanceFromPointToPlane(t.c, p);

    if ((da <= 0 && db >= 0) || (da >= 0 && db <= 0)) {
        if( (da <= 0 && dc >= 0) || (da >= 0 && dc <= 0))
            return TrianglePlaneIntersection{Edge{0, 1}, Edge{0, 2}};
        else if ((db <= 0 && dc >= 0) || (db >= 0 && dc <= 0)) 
            return TrianglePlaneIntersection{Edge{0, 1}, Edge{1, 2}};
        
    }
    if ((da <= 0 && dc >= 0) || (da >= 0 && dc <= 0)) {
        if ((db <= 0 && dc >= 0) || (db >= 0 && dc <= 0)) 
            return TrianglePlaneIntersection{Edge{0, 2}, Edge{1, 2}};
        else        
             return TrianglePlaneIntersection{Edge{0, 1}, Edge{0, 2}};
    }
    if ((db <= 0 && dc >= 0) || (db >= 0 && dc <= 0)) 
        return TrianglePlaneIntersection{Edge{1, 2}, Edge{0, 1}};

    return std::nullopt;
}

bool isTriangleVertexOnPlane(const Triangle& t, const Plane& p) {
    return distanceFromPointToPlane(t.a, p) == 0 ||
           distanceFromPointToPlane(t.b, p) == 0 ||
           distanceFromPointToPlane(t.c, p) == 0;
}

std::vector<Segment> cutMeshWithPlane(const Mesh &m, const Plane &p)
{
    std::vector<Segment> intersectionSegments;
    std::mutex intersectionMutex;
    const auto workerCount = std::max(1u, std::thread::hardware_concurrency());
    ThreadPool pool(workerCount);
    std::vector<std::future<void>> futures;
    futures.reserve(m.triangles.size());

    for (const auto& triangleIndices: m)
    {
        auto triangleIntersectingPlane = [&](Triangle t)
         {
            const std::array vertices{t.a, t.b, t.c};
            const std::array distances{
                signedDistanceFromPointToPlane(t.a, p),
                signedDistanceFromPointToPlane(t.b, p),
                signedDistanceFromPointToPlane(t.c, p),
            };
            constexpr std::array<Edge, 3> edges{Edge{0, 1}, Edge{1, 2}, Edge{0, 2}};

            std::vector<Point> intersectionPoints;
            for (const Edge& edge : edges) {
                const int i = edge[0];
                const int j = edge[1];
                const double di = distances[i];
                const double dj = distances[j];

                if (!edgeIntersectsPlane(di, dj)) {
                    continue;
                }

                if (std::abs(di) <= geometryEpsilon && std::abs(dj) <= geometryEpsilon) {
                    addUniquePoint(intersectionPoints, vertices[i]);
                    addUniquePoint(intersectionPoints, vertices[j]);
                } else if (std::abs(di) <= geometryEpsilon) {
                    addUniquePoint(intersectionPoints, vertices[i]);
                } else if (std::abs(dj) <= geometryEpsilon) {
                    addUniquePoint(intersectionPoints, vertices[j]);
                } else {
                    addUniquePoint(intersectionPoints, segmentPlaneIntersection(vertices[i], vertices[j], p));
                }
            }

            if (intersectionPoints.size() == 2) {
                Segment segment{intersectionPoints[0], intersectionPoints[1]};
                if (segmentLength(segment) > geometryEpsilon) {
                    std::lock_guard<std::mutex> lock(intersectionMutex);
                    intersectionSegments.push_back(segment);
                }
            }
        };
        Triangle tr ={m.vertices[triangleIndices[0]], m.vertices[triangleIndices[1]], m.vertices[triangleIndices[2]]};
        futures.push_back(pool.enqueue(triangleIntersectingPlane, tr));
    }

    for (auto& future : futures) {
        future.get();
    }
        
    return intersectionSegments;
}

std::vector<std::vector<Segment>> closedContours(std::span<const Segment> meshSegments)
{
    struct GraphEdge {
        int a{};
        int b{};
        Segment segment{};
        bool used{};
    };

    constexpr double epsilon = 1e-4;
    const auto makeKey = [](const Point& point) {
        return std::tuple{
            static_cast<long long>(std::llround(point.x / epsilon)),
            static_cast<long long>(std::llround(point.y / epsilon)),
            static_cast<long long>(std::llround(point.z / epsilon)),
        };
    };

    std::map<std::tuple<long long, long long, long long>, int> nodeIds;
    std::vector<Point> nodePoints;
    auto nodeFor = [&](const Point& point) mutable {
        const auto key = makeKey(point);
        if (const auto it = nodeIds.find(key); it != nodeIds.end()) {
            return it->second;
        }
        const int id = static_cast<int>(nodePoints.size());
        nodeIds.emplace(key, id);
        nodePoints.push_back(point);
        return id;
    };

    std::vector<GraphEdge> edges;
    edges.reserve(meshSegments.size());
    for (const Segment& segment : meshSegments) {
        if (segmentLength(segment) <= epsilon) {
            continue;
        }

        const int a = nodeFor(segment.p1);
        const int b = nodeFor(segment.p2);
        if (a == b) {
            continue;
        }

        edges.push_back({a, b, segment, false});
    }

    std::vector<std::vector<int>> adjacency(nodePoints.size());
    for (int edgeIndex = 0; edgeIndex < static_cast<int>(edges.size()); ++edgeIndex) {
        adjacency[edges[edgeIndex].a].push_back(edgeIndex);
        adjacency[edges[edgeIndex].b].push_back(edgeIndex);
    }

    int nonManifoldOrBoundaryNodes = 0;
    for (int nodeIndex = 0; nodeIndex < static_cast<int>(adjacency.size()); ++nodeIndex) {
        const auto& incidentEdges = adjacency[nodeIndex];
        if (incidentEdges.size() != 2) {
            ++nonManifoldOrBoundaryNodes;
            std::cout << "Contour node degree " << incidentEdges.size() << " at {"
                      << nodePoints[nodeIndex].x << ',' << nodePoints[nodeIndex].y << ','
                      << nodePoints[nodeIndex].z << '}' << std::endl;
        }
    }

    std::cout << "Number of segments to process: " << meshSegments.size() << std::endl;
    std::cout << "Number of graph segments after filtering: " << edges.size() << std::endl;
    std::cout << "Number of non-degree-2 contour nodes: " << nonManifoldOrBoundaryNodes << std::endl;

    std::vector<std::vector<Segment>> contours;

    for (int firstEdgeIndex = 0; firstEdgeIndex < static_cast<int>(edges.size()); ++firstEdgeIndex) {
        if (edges[firstEdgeIndex].used) {
            continue;
        }

        std::vector<Segment> contour;
        GraphEdge& firstEdge = edges[firstEdgeIndex];
        firstEdge.used = true;
        contour.push_back(firstEdge.segment);

        const int startNode = firstEdge.a;
        int currentNode = firstEdge.b;

        while (currentNode != startNode) {
            int nextEdgeIndex = -1;
            for (const int candidateEdgeIndex : adjacency[currentNode]) {
                if (!edges[candidateEdgeIndex].used) {
                    nextEdgeIndex = candidateEdgeIndex;
                    break;
                }
            }

            if (nextEdgeIndex == -1) {
                std::cout << "Could not find a matching segment to close the contour. Size: "
                          << contour.size() << " Start: {" << contour.front().p1.x << ','
                          << contour.front().p1.y << ',' << contour.front().p1.z << "}, End: {"
                          << contour.back().p2.x << ',' << contour.back().p2.y << ','
                          << contour.back().p2.z << '}' << std::endl;
                contour.clear();
                break;
            }

            GraphEdge& nextEdge = edges[nextEdgeIndex];
            nextEdge.used = true;
            if (nextEdge.a == currentNode) {
                contour.push_back(nextEdge.segment);
                currentNode = nextEdge.b;
            } else {
                contour.push_back({nextEdge.segment.p2, nextEdge.segment.p1});
                currentNode = nextEdge.a;
            }
        }

        if (!contour.empty()) {
            contours.push_back(std::move(contour));
        }
    }

    if(contours.empty())
    {
        std::cout << "No closed contours found." << std::endl;
    }
    else
    {
        std::cout << "Found " << contours.size() << " closed contours." << std::endl;
        for (size_t i = 0; i < contours.size(); ++i) {
            std::cout << "Contour " << i << " segments: " << contours[i].size() << '\n';
        }    
    }   
    return contours;
}
