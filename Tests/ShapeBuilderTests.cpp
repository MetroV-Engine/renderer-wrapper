/*
    Created on: 11/9/26
    Filename: ShapeBuilderTests.cpp
    Description: Unit tests for ShapeBuilder: shapes to triangles, polygon ear clipping, mesh validation.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/ShapeBuilder/ShapeBuilder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

using namespace gfx;

namespace {

    double area(const TriangleMesh& mesh)
    {
        double total = 0.0;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const Vertex& a = mesh.vertices[mesh.indices[i]];
            const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
            const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
            const double cross = (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y)
                               - (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
            total += std::abs(cross) * 0.5;
        }
        return total;
    }

    const Color Tint{ 10, 20, 30, 40 };

    bool hasTint(const Vertex& v)
    {
        return v.r == 10 && v.g == 20 && v.b == 30 && v.a == 40;
    }

    LineDraw lineOf(const float length, const float thickness)
    {
        LineDraw draw;
        draw.from = { 0.0f, 0.0f };
        draw.to = { length, 0.0f };
        draw.thickness = thickness;
        draw.color = Tint;
        return draw;
    }

    CircleDraw circleOf(const float radius, const int segments)
    {
        CircleDraw draw;
        draw.center = { 5.0f, -3.0f };
        draw.radius = radius;
        draw.segments = segments;
        draw.color = Tint;
        return draw;
    }

    TriangleDraw triangleAt(const float x)
    {
        TriangleDraw draw;
        draw.points = { Vec2{ x, 0.0f }, Vec2{ x + 1.0f, 0.0f }, Vec2{ x, 1.0f } };
        draw.color = Tint;
        return draw;
    }

    bool polygon(TriangleMesh& mesh, const std::vector<Vec2>& points)
    {
        PolygonDraw draw;
        draw.points = points;
        draw.color = Tint;
        return appendPolygon(mesh, draw);
    }

    const std::vector<Vec2> Square{ { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };

}

TEST(ShapeBuilder, LineIsAQuadAroundTheSegment)
{
    TriangleMesh mesh;
    appendLine(mesh, lineOf(10.0f, 2.0f));
    ASSERT_EQ(mesh.vertices.size(), 4u);
    EXPECT_EQ(mesh.indices.size(), 6u);
    for (const Vertex& v : mesh.vertices) {
        EXPECT_NEAR(std::abs(v.y), 1.0f, 1e-5f);
        EXPECT_TRUE(hasTint(v));
    }
    EXPECT_NEAR(area(mesh), 20.0, 1e-4);
}

TEST(ShapeBuilder, LineIgnoresZeroLengthAndBadThickness)
{
    TriangleMesh mesh;
    appendLine(mesh, lineOf(0.0f, 2.0f));
    appendLine(mesh, lineOf(10.0f, 0.0f));
    appendLine(mesh, lineOf(10.0f, -1.0f));
    EXPECT_TRUE(mesh.vertices.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST(ShapeBuilder, TriangleAndQuadCarryColour)
{
    TriangleMesh tri;
    TriangleDraw t;
    t.points = { Vec2{ 0, 0 }, Vec2{ 1, 0 }, Vec2{ 0, 1 } };
    t.color = Tint;
    appendTriangle(tri, t);
    EXPECT_EQ(tri.vertices.size(), 3u);
    EXPECT_EQ(tri.indices.size(), 3u);
    for (const Vertex& v : tri.vertices) EXPECT_TRUE(hasTint(v));

    TriangleMesh quad;
    QuadDraw q;
    q.corners = { Vec2{ 0, 0 }, Vec2{ 10, 0 }, Vec2{ 10, 10 }, Vec2{ 0, 10 } };
    q.color = Tint;
    appendQuad(quad, q);
    EXPECT_EQ(quad.vertices.size(), 4u);
    EXPECT_EQ(quad.indices.size(), 6u);
    for (const Vertex& v : quad.vertices) EXPECT_TRUE(hasTint(v));
    EXPECT_NEAR(area(quad), 100.0, 1e-4);
}

TEST(ShapeBuilder, CircleSegmentsFromRadius)
{
    TriangleMesh small;
    appendCircle(small, circleOf(1.0f, 0));
    EXPECT_EQ(small.vertices.size(), 13u);
    EXPECT_EQ(small.indices.size(), 36u);

    TriangleMesh huge;
    appendCircle(huge, circleOf(100000.0f, 0));
    EXPECT_EQ(huge.vertices.size(), 129u);
    EXPECT_EQ(huge.indices.size(), 384u);
}

TEST(ShapeBuilder, CircleExplicitSegmentsAreClamped)
{
    const std::array<std::pair<int, size_t>, 4> cases{ { { 1, 3 }, { 2, 3 }, { 5, 5 }, { 1000000, 1024 } } };
    for (const auto& [requested, expected] : cases) {
        TriangleMesh mesh;
        appendCircle(mesh, circleOf(10.0f, requested));
        EXPECT_EQ(mesh.vertices.size(), expected + 1) << requested;
        EXPECT_EQ(mesh.indices.size(), expected * 3) << requested;
    }
}

TEST(ShapeBuilder, CircleIgnoresNonPositiveRadius)
{
    TriangleMesh mesh;
    appendCircle(mesh, circleOf(0.0f, 0));
    appendCircle(mesh, circleOf(-1.0f, 0));
    EXPECT_TRUE(mesh.vertices.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST(ShapeBuilder, CircleRimIsAtRadius)
{
    TriangleMesh mesh;
    appendCircle(mesh, circleOf(7.0f, 0));
    ASSERT_GT(mesh.vertices.size(), 1u);
    for (size_t i = 1; i < mesh.vertices.size(); ++i) {
        const double dx = static_cast<double>(mesh.vertices[i].x) - 5.0;
        const double dy = static_cast<double>(mesh.vertices[i].y) + 3.0;
        EXPECT_NEAR(std::hypot(dx, dy), 7.0, 1e-3);
    }
}

TEST(ShapeBuilder, PolygonFillsConvex)
{
    TriangleMesh mesh;
    EXPECT_TRUE(polygon(mesh, Square));
    EXPECT_EQ(mesh.vertices.size(), 4u);
    EXPECT_EQ(mesh.indices.size(), 6u);
    EXPECT_NEAR(area(mesh), 100.0, 1e-4);
    for (const Vertex& v : mesh.vertices) EXPECT_TRUE(hasTint(v));
}

TEST(ShapeBuilder, PolygonFillsConcaveInBothWindings)
{
    std::vector<Vec2> shape{ { 0, 0 }, { 4, 0 }, { 4, 2 }, { 2, 2 }, { 2, 4 }, { 0, 4 } };
    TriangleMesh forward;
    EXPECT_TRUE(polygon(forward, shape));
    EXPECT_NEAR(area(forward), 12.0, 1e-4);

    std::vector<Vec2> reversed(shape.rbegin(), shape.rend());
    TriangleMesh backward;
    EXPECT_TRUE(polygon(backward, reversed));
    EXPECT_NEAR(area(backward), 12.0, 1e-4);
}

TEST(ShapeBuilder, PolygonToleratesDuplicatesAndCollinear)
{
    const std::vector<Vec2> shape{ { 0, 0 }, { 5, 0 }, { 10, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    TriangleMesh mesh;
    EXPECT_TRUE(polygon(mesh, shape));
    EXPECT_NEAR(area(mesh), 100.0, 1e-4);
}

TEST(ShapeBuilder, PolygonRejectsTooFewPoints)
{
    for (size_t count = 0; count <= 2; ++count) {
        const std::vector<Vec2> points(Square.begin(), Square.begin() + static_cast<std::ptrdiff_t>(count));
        TriangleMesh mesh;
        EXPECT_FALSE(polygon(mesh, points)) << count;
        EXPECT_TRUE(mesh.vertices.empty());
    }
}

TEST(ShapeBuilder, PolygonFailureLeavesMeshUnchanged)
{
    TriangleMesh mesh;
    appendTriangle(mesh, triangleAt(0.0f));

    EXPECT_FALSE(polygon(mesh, { { 0, 0 }, { 2, 2 }, { 2, 0 }, { 0, 2 } }));
    EXPECT_EQ(mesh.vertices.size(), 3u);
    EXPECT_EQ(mesh.indices.size(), 3u);

    EXPECT_FALSE(polygon(mesh, { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } }));
    EXPECT_EQ(mesh.vertices.size(), 3u);
    EXPECT_EQ(mesh.indices.size(), 3u);

    // Area 0.5: one ear is emitted, then no ear is left, so the rollback path runs.
    EXPECT_FALSE(polygon(mesh, { { 3, 6 }, { 4, 4 }, { 5, 6 }, { 1, 5 }, { 0, 5 } }));
    EXPECT_EQ(mesh.vertices.size(), 3u);
    EXPECT_EQ(mesh.indices.size(), 3u);
}

TEST(ShapeBuilder, AppendsOffsetIndices)
{
    TriangleMesh mesh;
    appendTriangle(mesh, triangleAt(0.0f));
    appendTriangle(mesh, triangleAt(5.0f));
    ASSERT_EQ(mesh.indices.size(), 6u);
    EXPECT_EQ(mesh.indices[3], 3);
    EXPECT_EQ(mesh.indices[4], 4);
    EXPECT_EQ(mesh.indices[5], 5);
}

TEST(ShapeBuilder, IsValidMesh)
{
    const std::vector<Vertex> three(3);
    const std::vector<uint16_t> good{ 0, 1, 2 };
    const std::vector<uint16_t> shortList{ 0, 1 };
    const std::vector<uint16_t> outOfRange{ 0, 1, 3 };
    const std::vector<Vertex> tooMany(MaxMeshVertices + 1);

    EXPECT_TRUE(isValidMesh({}, {}));
    EXPECT_TRUE(isValidMesh(three, good));
    EXPECT_FALSE(isValidMesh(three, shortList));
    EXPECT_FALSE(isValidMesh(three, outOfRange));
    EXPECT_FALSE(isValidMesh(tooMany, {}));
}
