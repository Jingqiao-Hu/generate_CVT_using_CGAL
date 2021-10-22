// cdt_cgal.cpp: 定义控制台应用程序的入口点。
//
#define CGAL_USE_BASIC_VIEWER

#include "stdafx.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_mesher_2.h>
#include <CGAL/Delaunay_mesh_face_base_2.h>
#include <CGAL/Delaunay_mesh_vertex_base_2.h>
#include <CGAL/Delaunay_mesh_size_criteria_2.h>
#include <CGAL/lloyd_optimize_mesh_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Voronoi_diagram_2.h>
#include <CGAL/Delaunay_triangulation_adaptation_traits_2.h>
#include <CGAL/Delaunay_triangulation_adaptation_policies_2.h>
#include <CGAL/intersections.h>

#include <iostream>

#include <igl/opengl/glfw/Viewer.h>
#include <igl/cat.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Iso_rectangle_2									Iso_rectangle_2;
typedef K::Segment_2										Segment_2;
typedef K::Ray_2											Ray_2;
typedef K::Line_2											Line_2;
typedef K::Point_2											Point_2;
typedef CGAL::Delaunay_mesh_vertex_base_2<K>                Vb;
typedef CGAL::Delaunay_mesh_face_base_2<K>                  Fb;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb>        Tds;
typedef CGAL::Constrained_Delaunay_triangulation_2<K, Tds>  CDT;

typedef CGAL::Triangulation_vertex_base_with_info_2<unsigned int, K> Vb1;
typedef CGAL::Triangulation_data_structure_2<Vb1>			Tds1;
typedef CGAL::Delaunay_triangulation_2<K, Tds1>				DT;

typedef CGAL::Delaunay_mesh_size_criteria_2<CDT>            Criteria;
typedef CGAL::Delaunay_mesher_2<CDT, Criteria>              Mesher;
typedef CDT::Vertex_handle Vertex_handle;
typedef CDT::Point Point;
typedef CDT::Edge  Edge;
typedef CDT::Face_handle	Face_handle;
typedef CDT::Finite_vertices_iterator Finite_vertices_iterator;
typedef CDT::Edge_iterator  Edge_iterator;

typedef CGAL::Delaunay_triangulation_adaptation_traits_2<DT>                 AT;
typedef CGAL::Delaunay_triangulation_caching_degeneracy_removal_policy_2<DT> AP;
typedef CGAL::Voronoi_diagram_2<DT, AT, AP>                                  VD;
typedef AT::Site_2                    Site_2;
typedef VD::Locate_result             Locate_result;
typedef VD::Ccb_halfedge_circulator   Ccb_halfedge_circulator;

typedef std::vector<std::vector<double>> segments_lists;

void plot_DT(Eigen::MatrixXd & V, Eigen::MatrixXi & F, DT dt)
{
	int fi = 0;
	for (DT::Finite_faces_iterator f = dt.finite_faces_begin(); f != dt.finite_faces_end(); f++)
	{
		for (int i = 0; i < 3; ++i)
		{
			Point p = f->vertex(i)->point();
			int idx = f->vertex(i)->info();

			V(idx, 0) = p.x();
			V(idx, 1) = p.y();
			F(fi, i) = idx;
		}
		fi++;
	}
}

// assuming the center of the mesh is (0,0)
void Lloyd(CDT & cdt, double nelx, double nely)
{
	Vertex_handle va = cdt.insert(Point(-nelx, -nely));
	Vertex_handle vb = cdt.insert(Point(nelx, -nely));
	Vertex_handle vc = cdt.insert(Point(-nelx, nely));
	Vertex_handle vd = cdt.insert(Point(nelx, nely));

	cdt.insert(Point(0, 0));

	cdt.insert_constraint(va, vb);
	cdt.insert_constraint(va, vc);
	cdt.insert_constraint(vc, vd);
	cdt.insert_constraint(vb, vd);

	std::cout << "Number of vertices: " << cdt.number_of_vertices() << std::endl;
	std::cout << "Meshing..." << std::endl;

	Mesher mesher(cdt);
	mesher.set_criteria(Criteria(0.125, nely));
	mesher.refine_mesh();

	std::cout << "Number of vertices: " << cdt.number_of_vertices() << std::endl;

	std::cout << "Run Lloyd optimization...";
	CGAL::lloyd_optimize_mesh_2(cdt, CGAL::parameters::max_iteration_number = 10);
	std::cout << " done." << std::endl;
	std::cout << "CDT Number of vertices: " << cdt.number_of_vertices() << std::endl;
}

//ps is the source of the ray, pt is the target of the ray
Point_2 VoronoiD_CrossPoint(Ray_2 ray, double nelx, double nely) {

	Segment_2 up(Point_2(nelx, nely), Point_2(-nelx, nely));
	Segment_2 down(Point_2(-nelx, -nely), Point_2(nelx, -nely));
	Segment_2 left(Point_2(-nelx, -nely), Point_2(-nelx, nely));
	Segment_2 right(Point_2(nelx, nely), Point_2(nelx, -nely));

	std::vector<Segment_2> boundarys;
	boundarys.push_back(up);
	boundarys.push_back(down);
	boundarys.push_back(left);
	boundarys.push_back(right);

	for (int i = 0; i < 4; ++i)
	{
		const auto result = intersection(ray, boundarys[i]);
		if (result) {
			if (const Point_2* p = boost::get<Point_2 >(&*result))
				return *p;
		}
	}	
	std::cout << "Error!no cross point!";
	Point_2 result(0, 0);
	return result;
}

// redivide the segments & rays into boundary
std::vector<Segment_2> VoronoiD_UpdateEdge(double nelx, double nely, segments_lists & rayEdge, segments_lists & halfEdge)
{
	segments_lists halfEdge_r, rayEdge_r;

	for (int i = 0; i < halfEdge.size(); i++) {
		double x1 = halfEdge[i][0];
		double y1 = halfEdge[i][1];
		double x2 = halfEdge[i][2];
		double y2 = halfEdge[i][3];
		std::vector<double> halfEdage_i;
		if (abs(x1) <= nelx && abs(y1) <= nely && abs(x2) <= nelx && abs(y2) <= nely) {
			halfEdage_i.insert(halfEdage_i.end(), halfEdge[i].begin(), halfEdge[i].end());
			halfEdge_r.push_back(halfEdage_i);
		}
		else if (abs(x1) <= nelx && abs(y1) <= nely) {
			halfEdage_i.push_back(x1);
			halfEdage_i.push_back(y1);
			halfEdage_i.push_back(x2);
			halfEdage_i.push_back(y2);
			rayEdge_r.push_back(halfEdage_i);
		}
		else if (abs(x2) <= nelx && abs(y2) <= nely) {
			halfEdage_i.push_back(x2);
			halfEdage_i.push_back(y2);
			halfEdage_i.push_back(x1);
			halfEdage_i.push_back(y1);
			rayEdge_r.push_back(halfEdage_i);
		}
		else {
			continue;
		}
	}

	for (int i = 0; i < rayEdge.size(); i++) {
		if (abs(rayEdge[i][0]) < nelx && abs(rayEdge[i][1]) < nely) {
			rayEdge_r.push_back(rayEdge[i]);
		}
		else {
			continue;
		}
	}

	halfEdge.clear(); //half Edage
	rayEdge.clear();  //ray Edage
	halfEdge = halfEdge_r;
	rayEdge = rayEdge_r;

	std::vector<Segment_2> rayEdgeBound;
	for (int i = 0; i < rayEdge.size(); i++) {
		Point_2 ps(rayEdge[i][0], rayEdge[i][1]);
		Point_2 pt(rayEdge[i][2], rayEdge[i][3]);
		Ray_2 ray(ps, pt);
		Point_2 ptn = VoronoiD_CrossPoint(ray, nelx, nely); // intersection of the ray with the boundary
		Segment_2 rayEdge_i(ps, ptn);
		/*std::vector<double> rayEdge_i;
		rayEdge_i.push_back(ps.x());
		rayEdge_i.push_back(ps.y());
		rayEdge_i.push_back(ptn.x());
		rayEdge_i.push_back(ptn.y());*/
		rayEdgeBound.push_back(rayEdge_i);
	}
	return rayEdgeBound;
}

void CDT2DT_VD(DT & dt, VD & vd, CDT cdt)
{
	std::vector<std::pair<Point, unsigned>> points;
	int index = 0;
	for (Vertex_handle vh : cdt.finite_vertex_handles()) {
		//index++;
		//if (index < 5) continue; // CHECK: why <5 continue?
		points.push_back(std::make_pair(vh->point(), index++));

		Site_2 t(vh->point().x(), vh->point().y());
		vd.insert(t);
	}
	dt = DT(points.begin(), points.end());
}

void DT2VD_edgs(DT dt, segments_lists & rayEdge, segments_lists & halfEdge)
{
	// pre-process to get rayEdgeBound for voronoi diagram
	//segments_lists rayEdge, halfEdge;
	for (DT::Edge_iterator eit = dt.edges_begin(); eit != dt.edges_end(); eit++) {
		CGAL::Object o = dt.dual(eit);
		std::vector<double> epi;
		if (CGAL::object_cast<K::Segment_2>(&o)) //如果这条边是线段，则绘制线段
		{
			epi.push_back(CGAL::object_cast<K::Segment_2>(&o)->source().x());
			epi.push_back(CGAL::object_cast<K::Segment_2>(&o)->source().y());
			epi.push_back(CGAL::object_cast<K::Segment_2>(&o)->target().x());
			epi.push_back(CGAL::object_cast<K::Segment_2>(&o)->target().y());
			halfEdge.push_back(epi);

			//Eigen::RowVector2d P1(epi[0], epi[1]);
			//Eigen::RowVector2d P2(epi[2], epi[3]);
			//viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 1, 1));
		}
		else if (CGAL::object_cast<K::Ray_2>(&o))//如果这条边是射线，则绘制射线
		{
			epi.push_back(CGAL::object_cast<K::Ray_2>(&o)->source().x());
			epi.push_back(CGAL::object_cast<K::Ray_2>(&o)->source().y());
			epi.push_back(CGAL::object_cast<K::Ray_2>(&o)->point(1).x());
			epi.push_back(CGAL::object_cast<K::Ray_2>(&o)->point(1).y());
			rayEdge.push_back(epi);

	/*		Eigen::RowVector2d P1(epi[0], epi[1]);
			Eigen::RowVector2d P2(epi[2], epi[3]);
			viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 1, 1));*/
		}
	}
}

bool point_equal(Point_2 p1, Point_2 p2)
{
	if (abs(p1.x() - p2.x()) < 1e-5 && abs(p1.y() - p2.y()) < 1e-5)
		return 1;
	else
		return 0;
}

// assuming the center of the mesh is (0,0)
void Lloyd_each(std::vector<Segment_2> edges, Eigen::MatrixXd & V, Eigen::MatrixXi & F, double x)
{
	// step.0 get points and connection from edges
	std::vector<Point_2> pnts;
	std::vector<std::vector<int>> edges_idx(edges.size());
	int p1_idx = 0, p2_idx = 1;
	for (int i = 0; i < edges.size(); ++i)
	{
		Point p1(edges[i].point(0).x(), edges[i].point(0).y());
		Point p2(edges[i].point(1).x(), edges[i].point(1).y());

		bool flag1 = 1, flag2 = 1;
		for (int j = 0; j < pnts.size(); ++j)
		{
			Point_2 p0 = pnts[j];
			if (point_equal(p0, p1)) {
				flag1 = 0;
				p1_idx = j;
			}
			if (point_equal(p0, p2)) {
				flag2 = 0;
				p2_idx = j;
			}
		}
		if (flag1) {
			pnts.push_back(p1);
			p1_idx = pnts.size() - 1;
		}
		if (flag2) {
			pnts.push_back(p2);
			p2_idx = pnts.size() - 1;
		}
		edges_idx[i] = std::vector<int>{ p1_idx, p2_idx };
	}

	// step.1 lloyd opt
	CDT cdt;
	std::vector<Vertex_handle> vecs;
	for (int i = 0; i < pnts.size(); ++i)
	{
		Vertex_handle va = cdt.insert(pnts[i]);
		vecs.push_back(va);
	}
	for (int i = 0; i < edges_idx.size(); ++i)
	{
		Vertex_handle va = vecs[edges_idx[i][0]];
		Vertex_handle vb = vecs[edges_idx[i][1]];
		cdt.insert_constraint(va, vb);
	}
	//double dis = CGAL::squared_distance(Point(edges[0].point(0).x(), edges[0].point(0).y()), Point(edges[0].point(1).x(), edges[0].point(1).x()));

	Mesher mesher(cdt);
	mesher.set_criteria(Criteria(0.125, x));
	mesher.refine_mesh();

	CGAL::lloyd_optimize_mesh_2(cdt, CGAL::parameters::max_iteration_number = 10);

	// step.2 transform to DT
	DT dt;
	VD vd;
	CDT2DT_VD(dt, vd, cdt);

	V = Eigen::MatrixXd(dt.number_of_vertices(), 2);
	F = Eigen::MatrixXi(dt.number_of_faces(), 3);
	plot_DT(V, F, dt);
}

// find two boundary edges with minimal dists to point p
void voronoi_boundary_edge(std::vector<Segment_2> rayEdgeBound, Point_2 p, std::vector<Segment_2> & boundary_edges)
{
	// step.1 find the minimal dist & idx
	std::vector<double> dist(rayEdgeBound.size());
	double min_d1 = 9999;
	int idx1 = 0;
	for (int i = 0; i < rayEdgeBound.size(); ++i)
	{
		Segment_2 seg = rayEdgeBound[i];
		dist[i] = CGAL::squared_distance(seg, p);

		if (dist[i] < min_d1) {
			min_d1 = dist[i];
			idx1 = i;
		}
	}
	boundary_edges.push_back(rayEdgeBound[idx1]);

	// step.2 find the second minimal dist & idx
	double min_d2 = 9999;
	int idx2 = 0;
	for (int i = 0; i < idx1; ++i)
	{
		if (dist[i] < min_d2) {
			min_d2 = dist[i];
			idx2 = i;
		}
	}
	for (int i = idx1 + 1; i < rayEdgeBound.size(); ++i)
	{
		if (dist[i] < min_d2) {
			min_d2 = dist[i];
			idx2 = i;
		}
	}
	boundary_edges.push_back(rayEdgeBound[idx2]);

	// step.3 add the edge on the bounding box
	Segment_2 seg1 = rayEdgeBound[idx1];
	Segment_2 seg2 = rayEdgeBound[idx2];
	double x1 = seg1.point(1).x();
	double y1 = seg1.point(1).y();
	double x2 = seg2.point(1).x();
	double y2 = seg2.point(1).y();
	Point_2 p1(x1, y1);
	Point_2 p2(x2, y2);

	// only check end_point(1), causing it is the point on the box
	if ((abs(x1 - x2) < 1e-5) || (abs(y1 - y2) < 1e-5))
	{
		Segment_2 seg(p1, p2);
		boundary_edges.push_back(seg);
	}
	else
	{
		Segment_2 candidate11(Point_2(x1, y2), p1);
		Segment_2 candidate12(Point_2(x1, y2), p2);
		double dis1 = CGAL::squared_distance(candidate11, p) + CGAL::squared_distance(candidate12, p);

		Segment_2 candidate21(Point_2(x2, y1), p1);
		Segment_2 candidate22(Point_2(x2, y1), p2);
		double dis2 = CGAL::squared_distance(candidate21, p) + CGAL::squared_distance(candidate22, p);

		if (dis1 < dis2) {
			boundary_edges.push_back(candidate11);
			boundary_edges.push_back(candidate12);
		}
		else {
			boundary_edges.push_back(candidate21);
			boundary_edges.push_back(candidate22);
		}
	}
}

std::vector<Segment_2> voronoi_single(Point_2 p, VD vd, double halfx, double halfy, std::vector<Segment_2> rayEdgeBound)
{
	std::vector<Segment_2> boundary_edges;

	Locate_result lr = vd.locate(p); // the face of p located
	VD::Face_handle* f = boost::get<VD::Face_handle>(&lr);

	Ccb_halfedge_circulator ec_start = (*f)->ccb(); // traversing the halfedges on the boundary of f
	Ccb_halfedge_circulator ec = ec_start;


	bool flag = 0;
	do {
		if ( ec->is_ray()) 
		{
			flag = 1;
		}
		else{
			double xs = ec->source()->point()[0];
			double ys = ec->source()->point()[1];
			double xt = ec->target()->point()[0];
			double yt = ec->target()->point()[1];
			if (abs(xs) <= halfx+1e-5 && abs(ys) <= halfy + 1e-5 && abs(xt) <= halfx + 1e-5 && abs(yt) <= halfy + 1e-5)
			{
				Point_2 p0(xs, ys);
				Point_2 p1(xt, yt);
				Segment_2 halfedgei(p0, p1);

				boundary_edges.push_back(halfedgei);
			}
			else
				flag = 1;
		}
	} while (++ec != ec_start);

	if (flag) // including boundary rays
		voronoi_boundary_edge(rayEdgeBound, p, boundary_edges);

	return boundary_edges;
}

int main()
{
	int nelx = 10;
	int nely = 6;
	double halfx = nelx / 2;
	double halfy = nely / 2;


	// step.1 generate CDT.
	CDT cdt;
	Lloyd(cdt, halfx, halfy);
	
	// step.2 change to DT & voronoi diagram
	DT dt;
	VD vd;
	CDT2DT_VD(dt, vd, cdt);

	segments_lists rayEdge, halfEdge;
	DT2VD_edgs(dt, rayEdge, halfEdge);

	std::vector<Segment_2> rayEdgeBound = VoronoiD_UpdateEdge(halfx, halfy, rayEdge, halfEdge);

	igl::opengl::glfw::Viewer viewer;
	//for (int i = 0; i < rayEdgeBound.size(); i++) 
	//{
	//	//int i = 7;
	//	Eigen::RowVector2d P1(rayEdgeBound[i].point(0).x(), rayEdgeBound[i].point(0).y());
	//	Eigen::RowVector2d P2(rayEdgeBound[i].point(1).x(), rayEdgeBound[i].point(1).y());
	//	viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 0, 0));
	//	//viewer.data().add_points(P1, Eigen::RowVector3d(0, 1, 0));
	//	//viewer.data().add_points(P2, Eigen::RowVector3d(1, 0, 0));
	//}
	//for (int i = 0; i < rayEdge.size(); i++)
	//{
	//	Eigen::RowVector2d P1(rayEdge[i][0], rayEdge[i][1]);
	//	Eigen::RowVector2d P2(rayEdge[i][2], rayEdge[i][3]);
	//	viewer.data().add_edges(P1, P2, Eigen::RowVector3d(0, 0, 1));
	//}
	//for (int i = 0; i < halfEdge.size(); i++)
	//{
	//	Eigen::RowVector2d P1(halfEdge[i][0], halfEdge[i][1]);
	//	Eigen::RowVector2d P2(halfEdge[i][2], halfEdge[i][3]);
	//	viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 1, 1));
	//}
	//Eigen::RowVector2d P11(-halfx, -halfy);
	//Eigen::RowVector2d P21(halfx, -halfy);
	//viewer.data().add_edges(P11, P21, Eigen::RowVector3d(0, 1, 0));
	//Eigen::RowVector2d P12(halfx, -halfy);
	//Eigen::RowVector2d P22(halfx, halfy);
	//viewer.data().add_edges(P12, P22, Eigen::RowVector3d(0, 1, 0));
	//Eigen::RowVector2d P13(halfx, halfy);
	//Eigen::RowVector2d P23(-halfx, halfy);
	//viewer.data().add_edges(P13, P23, Eigen::RowVector3d(0, 1, 0));
	//Eigen::RowVector2d P14(-halfx, halfy);
	//Eigen::RowVector2d P24(-halfx, -halfy);
	//viewer.data().add_edges(P14, P24, Eigen::RowVector3d(0, 1, 0));
	//viewer.launch();

	// step.3 get voronoi
	Eigen::MatrixXd V1;
	Eigen::MatrixXi F1;
	int idx = 0;

	double x = halfx / 30; // the sampling criterion for one cell
	for (Vertex_handle vh : cdt.finite_vertex_handles()) {
		Point_2 p = vh->point();
		std::vector<Segment_2> v_edges = voronoi_single(p, vd, halfx, halfy, rayEdgeBound);

		Eigen::MatrixXd V;
		Eigen::MatrixXi F;
		Lloyd_each(v_edges, V, F, x);

		if (idx < 1) {
			V1 = V;
			F1 = F;
		}
		else {
			Eigen::MatrixXd Vsum;
			Eigen::MatrixXi Fsum;
			igl::cat(1, V1, V, Vsum);
			igl::cat(1, F1, Eigen::MatrixXi(F.array() + V1.rows()), Fsum);			
			
			V1 = Vsum;
			F1 = Fsum;
		}

		//std::cout << V1 << std::endl;
		//std::cout << F1 << std::endl;

		idx++;
		//igl::opengl::glfw::Viewer viewer;
		//viewer.data().set_mesh(V, F);
		//viewer.launch();

		//Eigen::RowVector2d P(p.x(), p.y());
		//viewer.data().add_points(P, Eigen::RowVector3d(1, 0, 0));
		//
		for (int i = 0; i < v_edges.size(); ++i)
		{
			Eigen::RowVector2d P1(v_edges[i].point(0).x(), v_edges[i].point(0).y());
			Eigen::RowVector2d P2(v_edges[i].point(1).x(), v_edges[i].point(1).y());
			viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 0, 0));
		}
	}
	viewer.data().set_mesh(V1, F1);
	viewer.launch();

	getchar();
	return 0;
}

