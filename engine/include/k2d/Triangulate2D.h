#pragma once

#include <mathc.h>
#include <cstdint>

#include <ct/vector.hpp>

namespace k2d
{

    namespace tri
    {

        struct Edge;

        struct Point
        {
            double x, y;

            Point() : x(0.0), y(0.0) {}
            Point(double x_, double y_) : x(x_), y(y_) {}

            ct::Vector<Edge *> edge_list;

            void set_zero()
            {
                x = 0.0;
                y = 0.0;
            }
            void set(double x_, double y_)
            {
                x = x_;
                y = y_;
            }

            Point operator-() const
            {
                Point v;
                v.set(-x, -y);
                return v;
            }
            void operator+=(const Point &v)
            {
                x += v.x;
                y += v.y;
            }
            void operator-=(const Point &v)
            {
                x -= v.x;
                y -= v.y;
            }
            void operator*=(double a)
            {
                x *= a;
                y *= a;
            }
        };

        inline bool cmp(const Point *a, const Point *b)
        {
            if (a->y < b->y)
                return true;
            if (a->y == b->y && a->x < b->x)
                return true;
            return false;
        }

        inline Point operator+(const Point &a, const Point &b) { return Point(a.x + b.x, a.y + b.y); }
        inline Point operator-(const Point &a, const Point &b) { return Point(a.x - b.x, a.y - b.y); }
        inline Point operator*(double s, const Point &a) { return Point(s * a.x, s * a.y); }
        inline bool operator==(const Point &a, const Point &b) { return a.x == b.x && a.y == b.y; }
        inline bool operator!=(const Point &a, const Point &b) { return a.x != b.x || a.y != b.y; }
        inline double Dot(const Point &a, const Point &b) { return a.x * b.x + a.y * b.y; }
        inline double Cross(const Point &a, const Point &b) { return a.x * b.y - a.y * b.x; }
        inline Point Cross(const Point &a, double s) { return Point(s * a.y, -s * a.x); }
        inline Point Cross(double s, const Point &a) { return Point(-s * a.y, s * a.x); }

        struct Edge
        {
            Point *p, *q;
            bool valid;

            Edge(Point &p1, Point &p2);
        };

        class Triangle
        {
        public:
            Triangle(Point &a, Point &b, Point &c);

            bool constrained_edge[3];
            bool delaunay_edge[3];

            Point *GetPoint(const int &index) { return points_[index]; }
            Point *PointCW(Point &point);
            Point *PointCCW(Point &point);
            Point *OppositePoint(Triangle &t, Point &p);

            Triangle *GetNeighbor(const int &index) { return neighbors_[index]; }
            void MarkNeighbor(Point *p1, Point *p2, Triangle *t);
            void MarkNeighbor(Triangle &t);

            void MarkConstrainedEdge(const int index);
            void MarkConstrainedEdge(Edge &edge);
            void MarkConstrainedEdge(Point *p, Point *q);

            int Index(const Point *p);
            int EdgeIndex(const Point *p1, const Point *p2);

            Triangle *NeighborCW(Point &point);
            Triangle *NeighborCCW(Point &point);
            bool GetConstrainedEdgeCCW(Point &p);
            bool GetConstrainedEdgeCW(Point &p);
            void SetConstrainedEdgeCCW(Point &p, bool ce);
            void SetConstrainedEdgeCW(Point &p, bool ce);
            bool GetDelunayEdgeCCW(Point &p);
            bool GetDelunayEdgeCW(Point &p);
            void SetDelunayEdgeCCW(Point &p, bool e);
            void SetDelunayEdgeCW(Point &p, bool e);

            bool Contains(Point *p) { return p == points_[0] || p == points_[1] || p == points_[2]; }
            bool Contains(const Edge &e) { return Contains(e.p) && Contains(e.q); }
            bool Contains(Point *p, Point *q) { return Contains(p) && Contains(q); }
            void Legalize(Point &point);
            void Legalize(Point &opoint, Point &npoint);

            void Clear();
            void ClearNeighbor(Triangle *triangle);
            void ClearNeighbors();
            void ClearDelunayEdges();

            bool IsInterior() { return interior_; }
            void IsInterior(bool b) { interior_ = b; }

            Triangle &NeighborAcross(Point &opoint);

        private:
            Point *points_[3];
            Triangle *neighbors_[3];
            bool interior_;
        };

        struct Node
        {
            Point *point;
            Triangle *triangle;

            Node *next;
            Node *prev;

            double value;

            Node(Point &p) : point(&p), triangle(nullptr), next(nullptr), prev(nullptr), value(p.x) {}
            Node(Point &p, Triangle &t) : point(&p), triangle(&t), next(nullptr), prev(nullptr), value(p.x) {}
        };

        class AdvancingFront
        {
        public:
            AdvancingFront(Node &head, Node &tail);

            Node *head() { return head_; }
            void set_head(Node *node) { head_ = node; }
            Node *tail() { return tail_; }
            void set_tail(Node *node) { tail_ = node; }
            Node *search() { return search_node_; }
            void set_search(Node *node) { search_node_ = node; }

            Node *LocateNode(const double &x);
            Node *LocatePoint(const Point *point);

        private:
            Node *head_, *tail_, *search_node_;
            Node *FindSearchNode(const double &x);
        };

        class SweepContext
        {
        public:
            explicit SweepContext(ct::Vector<Point *> polyline);
            ~SweepContext();

            void set_head(Point *p1) { head_ = p1; }
            Point *head() { return head_; }
            void set_tail(Point *p1) { tail_ = p1; }
            Point *tail() { return tail_; }
            int point_count() { return (int)points_.size(); }

            Node &LocateNode(Point &point);
            void RemoveNode(Node *node);
            void CreateAdvancingFront();
            void MapTriangleToNodes(Triangle &t);
            void AddToMap(Triangle *triangle);
            Point *GetPoint(const int &index) { return points_[index]; }
            void RemoveFromMap(Triangle *triangle);
            void AddPoint(Point *point) { points_.push_back(point); }
            void AddHole(ct::Vector<Point *> polyline);

            AdvancingFront *front() { return front_; }

            void MeshClean(Triangle &triangle);

            const ct::Vector<Triangle *> &GetTriangles() { return triangles_; }
            const ct::Vector<Triangle *> &GetMap() { return map_; }

            ct::Vector<Edge *> edge_list;

            struct Basin
            {
                Node *left_node;
                Node *bottom_node;
                Node *right_node;
                double width;
                bool left_highest;

                Basin() : left_node(nullptr), bottom_node(nullptr), right_node(nullptr), width(0.0), left_highest(false) {}
            };

            struct EdgeEvent
            {
                Edge *constrained_edge;
                bool right;

                EdgeEvent() : constrained_edge(nullptr), right(false) {}
            };

            Basin basin;
            EdgeEvent edge_event;

            bool ok;

            void InitTriangulation();

        private:
            friend class Sweep;

            ct::Vector<Triangle *> triangles_;
            ct::Vector<Triangle *> map_;
            ct::Vector<Point *> points_;

            AdvancingFront *front_;
            Point *head_;
            Point *tail_;

            Node *af_head_, *af_middle_, *af_tail_;

            void InitEdges(ct::Vector<Point *> &polyline);
        };

        class Sweep
        {
        public:
            bool Triangulate(SweepContext &tcx);
            ~Sweep();

        private:
            void SweepPoints(SweepContext &tcx);
            Node &PointEvent(SweepContext &tcx, Point &point);
            void EdgeEvent(SweepContext &tcx, Edge *edge, Node *node);
            void EdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle *triangle, Point &point);
            Node &NewFrontTriangle(SweepContext &tcx, Point &point, Node &node);
            void Fill(SweepContext &tcx, Node &node);
            bool Legalize(SweepContext &tcx, Triangle &t);
            bool Incircle(Point &pa, Point &pb, Point &pc, Point &pd);
            void RotateTrianglePair(Triangle &t, Point &p, Triangle &ot, Point &op);
            void FillAdvancingFront(SweepContext &tcx, Node &n);
            double HoleAngle(Node &node);
            double BasinAngle(Node &node);
            void FillBasin(SweepContext &tcx, Node &node);
            void FillBasinReq(SweepContext &tcx, Node *node);
            bool IsShallow(SweepContext &tcx, Node &node);
            bool IsEdgeSideOfTriangle(Triangle &triangle, Point &ep, Point &eq);
            void FillEdgeEvent(SweepContext &tcx, Edge *edge, Node *node);
            void FillRightAboveEdgeEvent(SweepContext &tcx, Edge *edge, Node *node);
            void FillRightBelowEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FillRightConcaveEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FillRightConvexEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FillLeftAboveEdgeEvent(SweepContext &tcx, Edge *edge, Node *node);
            void FillLeftBelowEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FillLeftConcaveEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FillLeftConvexEdgeEvent(SweepContext &tcx, Edge *edge, Node &node);
            void FlipEdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle *t, Point &p);
            Triangle &NextFlipTriangle(SweepContext &tcx, int o, Triangle &t, Triangle &ot, Point &p, Point &op);
            Point &NextFlipPoint(Point &ep, Point &eq, Triangle &ot, Point &op);
            void FlipScanEdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle &flip_triangle, Triangle &t, Point &p);
            void FinalizationPolygon(SweepContext &tcx);

            ct::Vector<Node *> nodes_;
            bool mFailed = false;
        };

    } 

    int Triangulate(const Math::Vec2 *outline, int count, Math::Vec2 *outTriangles, int maxTriangles);

    int Triangulate(const Math::Vec2 *outline, int outlineCount,
                     const Math::Vec2 *const *holes, const int *holeCounts, int holeCount,
                     Math::Vec2 *outTriangles, int maxTriangles);

} 