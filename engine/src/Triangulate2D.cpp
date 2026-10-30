#include "k2d/Triangulate2D.h"

#include <cassert>
#include <cmath>

namespace k2d
{

    namespace tri
    {

        namespace
        {

            const double kPi = 3.14159265358979323846;
            const double kPiOver2 = 1.57079632679489661923;
            const double kPi3Div4 = 3.0 * kPi / 4.0;
            const double kTriEpsilon = 1e-15;

            enum Orientation
            {
                CW,
                CCW,
                COLLINEAR
            };

            Orientation Orient2d(Point &pa, Point &pb, Point &pc)
            {
                double detleft = (pa.x - pc.x) * (pb.y - pc.y);
                double detright = (pa.y - pc.y) * (pb.x - pc.x);
                double val = detleft - detright;
                if (val > -kTriEpsilon && val < kTriEpsilon)
                    return COLLINEAR;
                if (val > 0)
                    return CCW;
                return CW;
            }

            bool InScanArea(Point &pa, Point &pb, Point &pc, Point &pd)
            {
                double pdx = pd.x;
                double pdy = pd.y;
                double adx = pa.x - pdx;
                double ady = pa.y - pdy;
                double bdx = pb.x - pdx;
                double bdy = pb.y - pdy;

                double adxbdy = adx * bdy;
                double bdxady = bdx * ady;
                double oabd = adxbdy - bdxady;

                if (oabd <= kTriEpsilon)
                    return false;

                double cdx = pc.x - pdx;
                double cdy = pc.y - pdy;

                double cdxady = cdx * ady;
                double adxcdy = adx * cdy;
                double ocad = cdxady - adxcdy;

                if (ocad <= kTriEpsilon)
                    return false;

                return true;
            }

            Point sFailPoint(0.0, 0.0);

            void SortPoints(ct::Vector<Point *> &points)
            {
                for (size_t i = 1; i < points.size(); ++i)
                {
                    Point *key = points[i];
                    size_t j = i;
                    while (j > 0 && cmp(key, points[j - 1]))
                    {
                        points[j] = points[j - 1];
                        --j;
                    }
                    points[j] = key;
                }
            }

        } 

        Edge::Edge(Point &p1, Point &p2) : p(&p1), q(&p2), valid(true)
        {
            if (p1.y > p2.y)
            {
                q = &p1;
                p = &p2;
            }
            else if (p1.y == p2.y)
            {
                if (p1.x > p2.x)
                {
                    q = &p1;
                    p = &p2;
                }
                else if (p1.x == p2.x)
                {
                    valid = false;
                    return;
                }
            }

            q->edge_list.push_back(this);
        }

        Triangle::Triangle(Point &a, Point &b, Point &c)
        {
            points_[0] = &a;
            points_[1] = &b;
            points_[2] = &c;
            neighbors_[0] = nullptr;
            neighbors_[1] = nullptr;
            neighbors_[2] = nullptr;
            constrained_edge[0] = constrained_edge[1] = constrained_edge[2] = false;
            delaunay_edge[0] = delaunay_edge[1] = delaunay_edge[2] = false;
            interior_ = false;
        }

        void Triangle::MarkNeighbor(Point *p1, Point *p2, Triangle *t)
        {
            if ((p1 == points_[2] && p2 == points_[1]) || (p1 == points_[1] && p2 == points_[2]))
                neighbors_[0] = t;
            else if ((p1 == points_[0] && p2 == points_[2]) || (p1 == points_[2] && p2 == points_[0]))
                neighbors_[1] = t;
            else if ((p1 == points_[0] && p2 == points_[1]) || (p1 == points_[1] && p2 == points_[0]))
                neighbors_[2] = t;
            else
                assert(0);
        }

        void Triangle::MarkNeighbor(Triangle &t)
        {
            if (t.Contains(points_[1], points_[2]))
            {
                neighbors_[0] = &t;
                t.MarkNeighbor(points_[1], points_[2], this);
            }
            else if (t.Contains(points_[0], points_[2]))
            {
                neighbors_[1] = &t;
                t.MarkNeighbor(points_[0], points_[2], this);
            }
            else if (t.Contains(points_[0], points_[1]))
            {
                neighbors_[2] = &t;
                t.MarkNeighbor(points_[0], points_[1], this);
            }
        }

        void Triangle::Clear()
        {
            for (int i = 0; i < 3; ++i)
            {
                Triangle *t = neighbors_[i];
                if (t != nullptr)
                    t->ClearNeighbor(this);
            }
            ClearNeighbors();
            points_[0] = points_[1] = points_[2] = nullptr;
        }

        void Triangle::ClearNeighbor(Triangle *triangle)
        {
            if (neighbors_[0] == triangle)
                neighbors_[0] = nullptr;
            else if (neighbors_[1] == triangle)
                neighbors_[1] = nullptr;
            else
                neighbors_[2] = nullptr;
        }

        void Triangle::ClearNeighbors()
        {
            neighbors_[0] = nullptr;
            neighbors_[1] = nullptr;
            neighbors_[2] = nullptr;
        }

        void Triangle::ClearDelunayEdges()
        {
            delaunay_edge[0] = delaunay_edge[1] = delaunay_edge[2] = false;
        }

        Point *Triangle::OppositePoint(Triangle &t, Point &p)
        {
            Point *cw = t.PointCW(p);
            return PointCW(*cw);
        }

        void Triangle::Legalize(Point &point)
        {
            points_[1] = points_[0];
            points_[0] = points_[2];
            points_[2] = &point;
        }

        void Triangle::Legalize(Point &opoint, Point &npoint)
        {
            if (&opoint == points_[0])
            {
                points_[1] = points_[0];
                points_[0] = points_[2];
                points_[2] = &npoint;
            }
            else if (&opoint == points_[1])
            {
                points_[2] = points_[1];
                points_[1] = points_[0];
                points_[0] = &npoint;
            }
            else if (&opoint == points_[2])
            {
                points_[0] = points_[2];
                points_[2] = points_[1];
                points_[1] = &npoint;
            }
            else
            {
                assert(0);
            }
        }

        int Triangle::Index(const Point *p)
        {
            if (p == points_[0])
                return 0;
            if (p == points_[1])
                return 1;
            if (p == points_[2])
                return 2;
            assert(0);
            return 0;
        }

        int Triangle::EdgeIndex(const Point *p1, const Point *p2)
        {
            if (points_[0] == p1)
            {
                if (points_[1] == p2)
                    return 2;
                if (points_[2] == p2)
                    return 1;
            }
            else if (points_[1] == p1)
            {
                if (points_[2] == p2)
                    return 0;
                if (points_[0] == p2)
                    return 2;
            }
            else if (points_[2] == p1)
            {
                if (points_[0] == p2)
                    return 1;
                if (points_[1] == p2)
                    return 0;
            }
            return -1;
        }

        void Triangle::MarkConstrainedEdge(const int index)
        {
            constrained_edge[index] = true;
        }

        void Triangle::MarkConstrainedEdge(Edge &edge)
        {
            MarkConstrainedEdge(edge.p, edge.q);
        }

        void Triangle::MarkConstrainedEdge(Point *p, Point *q)
        {
            if ((q == points_[0] && p == points_[1]) || (q == points_[1] && p == points_[0]))
                constrained_edge[2] = true;
            else if ((q == points_[0] && p == points_[2]) || (q == points_[2] && p == points_[0]))
                constrained_edge[1] = true;
            else if ((q == points_[1] && p == points_[2]) || (q == points_[2] && p == points_[1]))
                constrained_edge[0] = true;
        }

        Point *Triangle::PointCW(Point &point)
        {
            if (&point == points_[0])
                return points_[2];
            if (&point == points_[1])
                return points_[0];
            if (&point == points_[2])
                return points_[1];
            assert(0);
            return nullptr;
        }

        Point *Triangle::PointCCW(Point &point)
        {
            if (&point == points_[0])
                return points_[1];
            if (&point == points_[1])
                return points_[2];
            if (&point == points_[2])
                return points_[0];
            assert(0);
            return nullptr;
        }

        Triangle *Triangle::NeighborCW(Point &point)
        {
            if (&point == points_[0])
                return neighbors_[1];
            if (&point == points_[1])
                return neighbors_[2];
            return neighbors_[0];
        }

        Triangle *Triangle::NeighborCCW(Point &point)
        {
            if (&point == points_[0])
                return neighbors_[2];
            if (&point == points_[1])
                return neighbors_[0];
            return neighbors_[1];
        }

        bool Triangle::GetConstrainedEdgeCCW(Point &p)
        {
            if (&p == points_[0])
                return constrained_edge[2];
            if (&p == points_[1])
                return constrained_edge[0];
            return constrained_edge[1];
        }

        bool Triangle::GetConstrainedEdgeCW(Point &p)
        {
            if (&p == points_[0])
                return constrained_edge[1];
            if (&p == points_[1])
                return constrained_edge[2];
            return constrained_edge[0];
        }

        void Triangle::SetConstrainedEdgeCCW(Point &p, bool ce)
        {
            if (&p == points_[0])
                constrained_edge[2] = ce;
            else if (&p == points_[1])
                constrained_edge[0] = ce;
            else
                constrained_edge[1] = ce;
        }

        void Triangle::SetConstrainedEdgeCW(Point &p, bool ce)
        {
            if (&p == points_[0])
                constrained_edge[1] = ce;
            else if (&p == points_[1])
                constrained_edge[2] = ce;
            else
                constrained_edge[0] = ce;
        }

        bool Triangle::GetDelunayEdgeCCW(Point &p)
        {
            if (&p == points_[0])
                return delaunay_edge[2];
            if (&p == points_[1])
                return delaunay_edge[0];
            return delaunay_edge[1];
        }

        bool Triangle::GetDelunayEdgeCW(Point &p)
        {
            if (&p == points_[0])
                return delaunay_edge[1];
            if (&p == points_[1])
                return delaunay_edge[2];
            return delaunay_edge[0];
        }

        void Triangle::SetDelunayEdgeCCW(Point &p, bool e)
        {
            if (&p == points_[0])
                delaunay_edge[2] = e;
            else if (&p == points_[1])
                delaunay_edge[0] = e;
            else
                delaunay_edge[1] = e;
        }

        void Triangle::SetDelunayEdgeCW(Point &p, bool e)
        {
            if (&p == points_[0])
                delaunay_edge[1] = e;
            else if (&p == points_[1])
                delaunay_edge[2] = e;
            else
                delaunay_edge[0] = e;
        }

        Triangle &Triangle::NeighborAcross(Point &opoint)
        {
            if (&opoint == points_[0])
                return *neighbors_[0];
            if (&opoint == points_[1])
                return *neighbors_[1];
            return *neighbors_[2];
        }

        AdvancingFront::AdvancingFront(Node &head, Node &tail)
        {
            head_ = &head;
            tail_ = &tail;
            search_node_ = &head;
        }

        Node *AdvancingFront::LocateNode(const double &x)
        {
            Node *node = search_node_;

            if (x < node->value)
            {
                while ((node = node->prev) != nullptr)
                {
                    if (x >= node->value)
                    {
                        search_node_ = node;
                        return node;
                    }
                }
            }
            else
            {
                while ((node = node->next) != nullptr)
                {
                    if (x < node->value)
                    {
                        search_node_ = node->prev;
                        return node->prev;
                    }
                }
            }
            return nullptr;
        }

        Node *AdvancingFront::FindSearchNode(const double &x)
        {
            (void)x;
            return search_node_;
        }

        Node *AdvancingFront::LocatePoint(const Point *point)
        {
            const double px = point->x;
            Node *node = FindSearchNode(px);
            const double nx = node->point->x;

            if (px == nx)
            {
                if (point != node->point)
                {
                    if (point == node->prev->point)
                        node = node->prev;
                    else if (point == node->next->point)
                        node = node->next;
                    else
                        assert(0);
                }
            }
            else if (px < nx)
            {
                while ((node = node->prev) != nullptr)
                {
                    if (point == node->point)
                        break;
                }
            }
            else
            {
                while ((node = node->next) != nullptr)
                {
                    if (point == node->point)
                        break;
                }
            }
            if (node)
                search_node_ = node;
            return node;
        }

        SweepContext::SweepContext(ct::Vector<Point *> polyline)
            : ok(true), front_(nullptr), head_(nullptr), tail_(nullptr),
              af_head_(nullptr), af_middle_(nullptr), af_tail_(nullptr)
        {
            points_ = polyline;
            InitEdges(points_);
        }

        void SweepContext::InitTriangulation()
        {
            double xmax(points_[0]->x), xmin(points_[0]->x);
            double ymax(points_[0]->y), ymin(points_[0]->y);

            for (size_t i = 0; i < points_.size(); ++i)
            {
                Point &p = *points_[i];
                if (p.x > xmax)
                    xmax = p.x;
                if (p.x < xmin)
                    xmin = p.x;
                if (p.y > ymax)
                    ymax = p.y;
                if (p.y < ymin)
                    ymin = p.y;
            }

            double dx = 0.3 * (xmax - xmin);
            double dy = 0.3 * (ymax - ymin);
            head_ = new Point(xmax + dx, ymin - dy);
            tail_ = new Point(xmin - dx, ymin - dy);

            SortPoints(points_);
        }

        void SweepContext::InitEdges(ct::Vector<Point *> &polyline)
        {
            int num_points = (int)polyline.size();
            for (int i = 0; i < num_points; ++i)
            {
                int j = i < num_points - 1 ? i + 1 : 0;
                Edge *edge = new Edge(*polyline[i], *polyline[j]);
                if (!edge->valid)
                {
                    ok = false;
                    delete edge;
                    return;
                }
                edge_list.push_back(edge);
            }
        }

        void SweepContext::AddToMap(Triangle *triangle)
        {
            map_.push_back(triangle);
        }

        void SweepContext::AddHole(ct::Vector<Point *> polyline)
        {
            InitEdges(polyline);
            for (size_t i = 0; i < polyline.size(); ++i)
                points_.push_back(polyline[i]);
        }

        Node &SweepContext::LocateNode(Point &point)
        {
            return *front_->LocateNode(point.x);
        }

        void SweepContext::CreateAdvancingFront()
        {
            Triangle *triangle = new Triangle(*points_[0], *tail_, *head_);

            map_.push_back(triangle);

            af_head_ = new Node(*triangle->GetPoint(1), *triangle);
            af_middle_ = new Node(*triangle->GetPoint(0), *triangle);
            af_tail_ = new Node(*triangle->GetPoint(2));
            front_ = new AdvancingFront(*af_head_, *af_tail_);

            af_head_->next = af_middle_;
            af_middle_->next = af_tail_;
            af_middle_->prev = af_head_;
            af_tail_->prev = af_middle_;
        }

        void SweepContext::RemoveNode(Node *node)
        {
            delete node;
        }

        void SweepContext::MapTriangleToNodes(Triangle &t)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!t.GetNeighbor(i))
                {
                    Node *n = front_->LocatePoint(t.PointCW(*t.GetPoint(i)));
                    if (n)
                        n->triangle = &t;
                }
            }
        }

        void SweepContext::RemoveFromMap(Triangle *triangle)
        {
            for (size_t i = 0; i < map_.size(); ++i)
            {
                if (map_[i] == triangle)
                {
                    map_.erase(map_.begin() + (ptrdiff_t)i);
                    return;
                }
            }
        }

        void SweepContext::MeshClean(Triangle &triangle)
        {
            if (!triangle.IsInterior())
            {
                triangle.IsInterior(true);
                triangles_.push_back(&triangle);
                for (int i = 0; i < 3; ++i)
                {
                    if (!triangle.constrained_edge[i])
                    {
                        Triangle *neighbor = triangle.GetNeighbor(i);
                        if (neighbor)
                            MeshClean(*neighbor);
                    }
                }
            }
        }

        SweepContext::~SweepContext()
        {
            delete head_;
            delete tail_;
            delete front_;
            delete af_head_;
            delete af_middle_;
            delete af_tail_;

            for (size_t i = 0; i < map_.size(); ++i)
                delete map_[i];

            for (size_t i = 0; i < edge_list.size(); ++i)
                delete edge_list[i];
        }

        bool Sweep::Triangulate(SweepContext &tcx)
        {
            mFailed = false;

            if (!tcx.ok)
                return false;

            tcx.InitTriangulation();
            tcx.CreateAdvancingFront();

            SweepPoints(tcx);
            if (mFailed)
                return false;

            FinalizationPolygon(tcx);
            return true;
        }

        void Sweep::SweepPoints(SweepContext &tcx)
        {
            for (int i = 1; i < tcx.point_count(); ++i)
            {
                Point &point = *tcx.GetPoint(i);
                Node *node = &PointEvent(tcx, point);
                for (size_t k = 0; k < point.edge_list.size(); ++k)
                {
                    EdgeEvent(tcx, point.edge_list[k], node);
                    if (mFailed)
                        return;
                }
            }
        }

        void Sweep::FinalizationPolygon(SweepContext &tcx)
        {
            Triangle *t = tcx.front()->head()->next->triangle;
            Point *p = tcx.front()->head()->next->point;
            while (!t->GetConstrainedEdgeCW(*p))
                t = t->NeighborCCW(*p);

            tcx.MeshClean(*t);
        }

        Node &Sweep::PointEvent(SweepContext &tcx, Point &point)
        {
            Node &node = tcx.LocateNode(point);
            Node &new_node = NewFrontTriangle(tcx, point, node);

            if (point.x <= node.point->x + kTriEpsilon)
                Fill(tcx, node);

            FillAdvancingFront(tcx, new_node);
            return new_node;
        }

        void Sweep::EdgeEvent(SweepContext &tcx, Edge *edge, Node *node)
        {
            tcx.edge_event.constrained_edge = edge;
            tcx.edge_event.right = (edge->p->x > edge->q->x);

            if (IsEdgeSideOfTriangle(*node->triangle, *edge->p, *edge->q))
                return;

            FillEdgeEvent(tcx, edge, node);
            if (mFailed)
                return;
            EdgeEvent(tcx, *edge->p, *edge->q, node->triangle, *edge->q);
        }

        void Sweep::EdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle *triangle, Point &point)
        {
            if (mFailed)
                return;

            if (IsEdgeSideOfTriangle(*triangle, ep, eq))
                return;

            Point *p1 = triangle->PointCCW(point);
            Orientation o1 = Orient2d(eq, *p1, ep);
            if (o1 == COLLINEAR)
            {
                mFailed = true;
                return;
            }

            Point *p2 = triangle->PointCW(point);
            Orientation o2 = Orient2d(eq, *p2, ep);
            if (o2 == COLLINEAR)
            {
                mFailed = true;
                return;
            }

            if (o1 == o2)
            {
                if (o1 == CW)
                    triangle = triangle->NeighborCCW(point);
                else
                    triangle = triangle->NeighborCW(point);
                EdgeEvent(tcx, ep, eq, triangle, point);
            }
            else
            {
                FlipEdgeEvent(tcx, ep, eq, triangle, point);
            }
        }

        bool Sweep::IsEdgeSideOfTriangle(Triangle &triangle, Point &ep, Point &eq)
        {
            int index = triangle.EdgeIndex(&ep, &eq);

            if (index != -1)
            {
                triangle.MarkConstrainedEdge(index);
                Triangle *t = triangle.GetNeighbor(index);
                if (t)
                    t->MarkConstrainedEdge(&ep, &eq);
                return true;
            }
            return false;
        }

        Node &Sweep::NewFrontTriangle(SweepContext &tcx, Point &point, Node &node)
        {
            Triangle *triangle = new Triangle(point, *node.point, *node.next->point);

            triangle->MarkNeighbor(*node.triangle);
            tcx.AddToMap(triangle);

            Node *new_node = new Node(point);
            nodes_.push_back(new_node);

            new_node->next = node.next;
            new_node->prev = &node;
            node.next->prev = new_node;
            node.next = new_node;

            if (!Legalize(tcx, *triangle))
                tcx.MapTriangleToNodes(*triangle);

            return *new_node;
        }

        void Sweep::Fill(SweepContext &tcx, Node &node)
        {
            Triangle *triangle = new Triangle(*node.prev->point, *node.point, *node.next->point);

            triangle->MarkNeighbor(*node.prev->triangle);
            triangle->MarkNeighbor(*node.triangle);

            tcx.AddToMap(triangle);

            node.prev->next = node.next;
            node.next->prev = node.prev;

            if (!Legalize(tcx, *triangle))
                tcx.MapTriangleToNodes(*triangle);
        }

        void Sweep::FillAdvancingFront(SweepContext &tcx, Node &n)
        {
            Node *node = n.next;

            while (node->next)
            {
                double angle = HoleAngle(*node);
                if (angle > kPiOver2 || angle < -kPiOver2)
                    break;
                Fill(tcx, *node);
                node = node->next;
            }

            node = n.prev;

            while (node->prev)
            {
                double angle = HoleAngle(*node);
                if (angle > kPiOver2 || angle < -kPiOver2)
                    break;
                Fill(tcx, *node);
                node = node->prev;
            }

            if (n.next && n.next->next)
            {
                double angle = BasinAngle(n);
                if (angle < kPi3Div4)
                    FillBasin(tcx, n);
            }
        }

        double Sweep::BasinAngle(Node &node)
        {
            double ax = node.point->x - node.next->next->point->x;
            double ay = node.point->y - node.next->next->point->y;
            return atan2(ay, ax);
        }

        double Sweep::HoleAngle(Node &node)
        {
            double ax = node.next->point->x - node.point->x;
            double ay = node.next->point->y - node.point->y;
            double bx = node.prev->point->x - node.point->x;
            double by = node.prev->point->y - node.point->y;
            return atan2(ax * by - ay * bx, ax * bx + ay * by);
        }

        bool Sweep::Legalize(SweepContext &tcx, Triangle &t)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (t.delaunay_edge[i])
                    continue;

                Triangle *ot = t.GetNeighbor(i);

                if (ot)
                {
                    Point *p = t.GetPoint(i);
                    Point *op = ot->OppositePoint(t, *p);
                    int oi = ot->Index(op);

                    if (ot->constrained_edge[oi] || ot->delaunay_edge[oi])
                    {
                        t.constrained_edge[i] = ot->constrained_edge[oi];
                        continue;
                    }

                    bool inside = Incircle(*p, *t.PointCCW(*p), *t.PointCW(*p), *op);

                    if (inside)
                    {
                        t.delaunay_edge[i] = true;
                        ot->delaunay_edge[oi] = true;

                        RotateTrianglePair(t, *p, *ot, *op);

                        bool not_legalized = !Legalize(tcx, t);
                        if (not_legalized)
                            tcx.MapTriangleToNodes(t);

                        not_legalized = !Legalize(tcx, *ot);
                        if (not_legalized)
                            tcx.MapTriangleToNodes(*ot);

                        t.delaunay_edge[i] = false;
                        ot->delaunay_edge[oi] = false;

                        return true;
                    }
                }
            }
            return false;
        }

        bool Sweep::Incircle(Point &pa, Point &pb, Point &pc, Point &pd)
        {
            double adx = pa.x - pd.x;
            double ady = pa.y - pd.y;
            double bdx = pb.x - pd.x;
            double bdy = pb.y - pd.y;

            double adxbdy = adx * bdy;
            double bdxady = bdx * ady;
            double oabd = adxbdy - bdxady;

            if (oabd <= 0)
                return false;

            double cdx = pc.x - pd.x;
            double cdy = pc.y - pd.y;

            double cdxady = cdx * ady;
            double adxcdy = adx * cdy;
            double ocad = cdxady - adxcdy;

            if (ocad <= 0)
                return false;

            double bdxcdy = bdx * cdy;
            double cdxbdy = cdx * bdy;

            double alift = adx * adx + ady * ady;
            double blift = bdx * bdx + bdy * bdy;
            double clift = cdx * cdx + cdy * cdy;

            double det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

            return det > 0;
        }

        void Sweep::RotateTrianglePair(Triangle &t, Point &p, Triangle &ot, Point &op)
        {
            Triangle *n1, *n2, *n3, *n4;
            n1 = t.NeighborCCW(p);
            n2 = t.NeighborCW(p);
            n3 = ot.NeighborCCW(op);
            n4 = ot.NeighborCW(op);

            bool ce1, ce2, ce3, ce4;
            ce1 = t.GetConstrainedEdgeCCW(p);
            ce2 = t.GetConstrainedEdgeCW(p);
            ce3 = ot.GetConstrainedEdgeCCW(op);
            ce4 = ot.GetConstrainedEdgeCW(op);

            bool de1, de2, de3, de4;
            de1 = t.GetDelunayEdgeCCW(p);
            de2 = t.GetDelunayEdgeCW(p);
            de3 = ot.GetDelunayEdgeCCW(op);
            de4 = ot.GetDelunayEdgeCW(op);

            t.Legalize(p, op);
            ot.Legalize(op, p);

            ot.SetDelunayEdgeCCW(p, de1);
            t.SetDelunayEdgeCW(p, de2);
            t.SetDelunayEdgeCCW(op, de3);
            ot.SetDelunayEdgeCW(op, de4);

            ot.SetConstrainedEdgeCCW(p, ce1);
            t.SetConstrainedEdgeCW(p, ce2);
            t.SetConstrainedEdgeCCW(op, ce3);
            ot.SetConstrainedEdgeCW(op, ce4);

            t.ClearNeighbors();
            ot.ClearNeighbors();
            if (n1)
                ot.MarkNeighbor(*n1);
            if (n2)
                t.MarkNeighbor(*n2);
            if (n3)
                t.MarkNeighbor(*n3);
            if (n4)
                ot.MarkNeighbor(*n4);
            t.MarkNeighbor(ot);
        }

        void Sweep::FillBasin(SweepContext &tcx, Node &node)
        {
            if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW)
                tcx.basin.left_node = node.next->next;
            else
                tcx.basin.left_node = node.next;

            tcx.basin.bottom_node = tcx.basin.left_node;
            while (tcx.basin.bottom_node->next && tcx.basin.bottom_node->point->y >= tcx.basin.bottom_node->next->point->y)
                tcx.basin.bottom_node = tcx.basin.bottom_node->next;
            if (tcx.basin.bottom_node == tcx.basin.left_node)
                return;

            tcx.basin.right_node = tcx.basin.bottom_node;
            while (tcx.basin.right_node->next && tcx.basin.right_node->point->y < tcx.basin.right_node->next->point->y)
                tcx.basin.right_node = tcx.basin.right_node->next;
            if (tcx.basin.right_node == tcx.basin.bottom_node)
                return;

            tcx.basin.width = tcx.basin.right_node->point->x - tcx.basin.left_node->point->x;
            tcx.basin.left_highest = tcx.basin.left_node->point->y > tcx.basin.right_node->point->y;

            FillBasinReq(tcx, tcx.basin.bottom_node);
        }

        void Sweep::FillBasinReq(SweepContext &tcx, Node *node)
        {
            if (IsShallow(tcx, *node))
                return;

            Fill(tcx, *node);

            if (node->prev == tcx.basin.left_node && node->next == tcx.basin.right_node)
                return;

            if (node->prev == tcx.basin.left_node)
            {
                Orientation o = Orient2d(*node->point, *node->next->point, *node->next->next->point);
                if (o == CW)
                    return;
                node = node->next;
            }
            else if (node->next == tcx.basin.right_node)
            {
                Orientation o = Orient2d(*node->point, *node->prev->point, *node->prev->prev->point);
                if (o == CCW)
                    return;
                node = node->prev;
            }
            else
            {
                if (node->prev->point->y < node->next->point->y)
                    node = node->prev;
                else
                    node = node->next;
            }

            FillBasinReq(tcx, node);
        }

        bool Sweep::IsShallow(SweepContext &tcx, Node &node)
        {
            double height;

            if (tcx.basin.left_highest)
                height = tcx.basin.left_node->point->y - node.point->y;
            else
                height = tcx.basin.right_node->point->y - node.point->y;

            return tcx.basin.width > height;
        }

        void Sweep::FillEdgeEvent(SweepContext &tcx, Edge *edge, Node *node)
        {
            if (tcx.edge_event.right)
                FillRightAboveEdgeEvent(tcx, edge, node);
            else
                FillLeftAboveEdgeEvent(tcx, edge, node);
        }

        void Sweep::FillRightAboveEdgeEvent(SweepContext &tcx, Edge *edge, Node *node)
        {
            while (node->next->point->x < edge->p->x)
            {
                if (Orient2d(*edge->q, *node->next->point, *edge->p) == CCW)
                    FillRightBelowEdgeEvent(tcx, edge, *node);
                else
                    node = node->next;
            }
        }

        void Sweep::FillRightBelowEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            if (node.point->x < edge->p->x)
            {
                if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW)
                {
                    FillRightConcaveEdgeEvent(tcx, edge, node);
                }
                else
                {
                    FillRightConvexEdgeEvent(tcx, edge, node);
                    FillRightBelowEdgeEvent(tcx, edge, node);
                }
            }
        }

        void Sweep::FillRightConcaveEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            Fill(tcx, *node.next);
            if (node.next->point != edge->p)
            {
                if (Orient2d(*edge->q, *node.next->point, *edge->p) == CCW)
                {
                    if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW)
                        FillRightConcaveEdgeEvent(tcx, edge, node);
                }
            }
        }

        void Sweep::FillRightConvexEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            if (Orient2d(*node.next->point, *node.next->next->point, *node.next->next->next->point) == CCW)
            {
                FillRightConcaveEdgeEvent(tcx, edge, *node.next);
            }
            else
            {
                if (Orient2d(*edge->q, *node.next->next->point, *edge->p) == CCW)
                    FillRightConvexEdgeEvent(tcx, edge, *node.next);
            }
        }

        void Sweep::FillLeftAboveEdgeEvent(SweepContext &tcx, Edge *edge, Node *node)
        {
            while (node->prev->point->x > edge->p->x)
            {
                if (Orient2d(*edge->q, *node->prev->point, *edge->p) == CW)
                    FillLeftBelowEdgeEvent(tcx, edge, *node);
                else
                    node = node->prev;
            }
        }

        void Sweep::FillLeftBelowEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            if (node.point->x > edge->p->x)
            {
                if (Orient2d(*node.point, *node.prev->point, *node.prev->prev->point) == CW)
                {
                    FillLeftConcaveEdgeEvent(tcx, edge, node);
                }
                else
                {
                    FillLeftConvexEdgeEvent(tcx, edge, node);
                    FillLeftBelowEdgeEvent(tcx, edge, node);
                }
            }
        }

        void Sweep::FillLeftConvexEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            if (Orient2d(*node.prev->point, *node.prev->prev->point, *node.prev->prev->prev->point) == CW)
            {
                FillLeftConcaveEdgeEvent(tcx, edge, *node.prev);
            }
            else
            {
                if (Orient2d(*edge->q, *node.prev->prev->point, *edge->p) == CW)
                    FillLeftConvexEdgeEvent(tcx, edge, *node.prev);
            }
        }

        void Sweep::FillLeftConcaveEdgeEvent(SweepContext &tcx, Edge *edge, Node &node)
        {
            Fill(tcx, *node.prev);
            if (node.prev->point != edge->p)
            {
                if (Orient2d(*edge->q, *node.prev->point, *edge->p) == CW)
                {
                    if (Orient2d(*node.point, *node.prev->point, *node.prev->prev->point) == CW)
                        FillLeftConcaveEdgeEvent(tcx, edge, node);
                }
            }
        }

        void Sweep::FlipEdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle *t, Point &p)
        {
            if (mFailed)
                return;

            Triangle &ot = t->NeighborAcross(p);
            Point &op = *ot.OppositePoint(*t, p);

            if (InScanArea(p, *t->PointCCW(p), *t->PointCW(p), op))
            {
                RotateTrianglePair(*t, p, ot, op);
                tcx.MapTriangleToNodes(*t);
                tcx.MapTriangleToNodes(ot);

                if (p == eq && op == ep)
                {
                    if (eq == *tcx.edge_event.constrained_edge->q && ep == *tcx.edge_event.constrained_edge->p)
                    {
                        t->MarkConstrainedEdge(&ep, &eq);
                        ot.MarkConstrainedEdge(&ep, &eq);
                        Legalize(tcx, *t);
                        Legalize(tcx, ot);
                    }
                }
                else
                {
                    Orientation o = Orient2d(eq, op, ep);
                    t = &NextFlipTriangle(tcx, (int)o, *t, ot, p, op);
                    if (mFailed)
                        return;
                    FlipEdgeEvent(tcx, ep, eq, t, p);
                }
            }
            else
            {
                Point &newP = NextFlipPoint(ep, eq, ot, op);
                if (mFailed)
                    return;
                FlipScanEdgeEvent(tcx, ep, eq, *t, ot, newP);
                if (mFailed)
                    return;
                EdgeEvent(tcx, ep, eq, t, p);
            }
        }

        Triangle &Sweep::NextFlipTriangle(SweepContext &tcx, int o, Triangle &t, Triangle &ot, Point &p, Point &op)
        {
            if (o == CCW)
            {
                int edge_index = ot.EdgeIndex(&p, &op);
                ot.delaunay_edge[edge_index] = true;
                Legalize(tcx, ot);
                ot.ClearDelunayEdges();
                return t;
            }

            int edge_index = t.EdgeIndex(&p, &op);

            t.delaunay_edge[edge_index] = true;
            Legalize(tcx, t);
            t.ClearDelunayEdges();
            return ot;
        }

        Point &Sweep::NextFlipPoint(Point &ep, Point &eq, Triangle &ot, Point &op)
        {
            Orientation o2d = Orient2d(eq, op, ep);
            if (o2d == CW)
                return *ot.PointCCW(op);
            if (o2d == CCW)
                return *ot.PointCW(op);

            mFailed = true;
            return sFailPoint;
        }

        void Sweep::FlipScanEdgeEvent(SweepContext &tcx, Point &ep, Point &eq, Triangle &flip_triangle, Triangle &t, Point &p)
        {
            if (mFailed)
                return;

            Triangle &ot = t.NeighborAcross(p);
            Point &op = *ot.OppositePoint(t, p);

            if (InScanArea(eq, *flip_triangle.PointCCW(eq), *flip_triangle.PointCW(eq), op))
            {
                FlipEdgeEvent(tcx, eq, op, &ot, op);
            }
            else
            {
                Point &newP = NextFlipPoint(ep, eq, ot, op);
                if (mFailed)
                    return;
                FlipScanEdgeEvent(tcx, ep, eq, flip_triangle, ot, newP);
            }
        }

        Sweep::~Sweep()
        {
            for (size_t i = 0; i < nodes_.size(); ++i)
                delete nodes_[i];
        }

    } 

    namespace
    {
        double SignedArea2(const Math::Vec2 *points, int count)
        {
            double area2 = 0.0;
            for (int i = 0; i < count; ++i)
            {
                int j = (i + 1) % count;
                area2 += (double)points[i].x * (double)points[j].y - (double)points[j].x * (double)points[i].y;
            }
            return area2;
        }

        ct::Vector<tri::Point *> MakePolyline(const Math::Vec2 *points, int count, bool reverse)
        {
            ct::Vector<tri::Point *> polyline;
            polyline.reserve((size_t)count);
            if (reverse)
            {
                for (int i = count - 1; i >= 0; --i)
                    polyline.push_back(new tri::Point((double)points[i].x, (double)points[i].y));
            }
            else
            {
                for (int i = 0; i < count; ++i)
                    polyline.push_back(new tri::Point((double)points[i].x, (double)points[i].y));
            }
            return polyline;
        }
    }

    int Triangulate(const Math::Vec2 *outline, int outlineCount,
                     const Math::Vec2 *const *holes, const int *holeCounts, int holeCount,
                     Math::Vec2 *outTriangles, int maxTriangles)
    {
        if (outlineCount < 3 || !outline || !outTriangles || maxTriangles <= 0)
            return 0;

        const double outlineArea2 = SignedArea2(outline, outlineCount);
        ct::Vector<tri::Point *> polyline = MakePolyline(outline, outlineCount, outlineArea2 < 0.0);

        ct::Vector<ct::Vector<tri::Point *>> holePolylines;
        for (int h = 0; h < holeCount; ++h)
        {
            if (!holes || !holes[h] || !holeCounts || holeCounts[h] < 3)
                continue;
            const double holeArea2 = SignedArea2(holes[h], holeCounts[h]);
            holePolylines.push_back(MakePolyline(holes[h], holeCounts[h], holeArea2 >= 0.0));
        }

        tri::SweepContext context(polyline);
        for (size_t h = 0; h < holePolylines.size(); ++h)
            context.AddHole(holePolylines[h]);

        tri::Sweep sweep;
        bool ok = sweep.Triangulate(context);

        int written = 0;
        if (ok)
        {
            const ct::Vector<tri::Triangle *> &triangles = context.GetTriangles();
            for (size_t i = 0; i < triangles.size() && written < maxTriangles; ++i)
            {
                tri::Triangle *t = triangles[i];
                outTriangles[written * 3 + 0] = Math::Vec2((float)t->GetPoint(0)->x, (float)t->GetPoint(0)->y);
                outTriangles[written * 3 + 1] = Math::Vec2((float)t->GetPoint(1)->x, (float)t->GetPoint(1)->y);
                outTriangles[written * 3 + 2] = Math::Vec2((float)t->GetPoint(2)->x, (float)t->GetPoint(2)->y);
                ++written;
            }
        }

        for (size_t i = 0; i < polyline.size(); ++i)
            delete polyline[i];
        for (size_t h = 0; h < holePolylines.size(); ++h)
            for (size_t i = 0; i < holePolylines[h].size(); ++i)
                delete holePolylines[h][i];

        return written;
    }

    int Triangulate(const Math::Vec2 *outline, int count, Math::Vec2 *outTriangles, int maxTriangles)
    {
        return Triangulate(outline, count, nullptr, nullptr, 0, outTriangles, maxTriangles);
    }

} 