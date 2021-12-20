// cdt_cgal.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include "cdt_Header.h"

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_mesher_2.h>
#include <CGAL/Delaunay_mesh_face_base_2.h>
#include <CGAL/Delaunay_mesh_vertex_base_2.h>
#include <CGAL/Delaunay_mesh_size_criteria_2.h>
#include <CGAL/lloyd_optimize_mesh_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Voronoi_diagram_2.h>
//#include <CGAL/draw_voronoi_diagram_2.h>
#include <CGAL/Delaunay_triangulation_adaptation_traits_2.h>
#include <CGAL/Delaunay_triangulation_adaptation_policies_2.h>
#include <CGAL/Polygon_2.h>

typedef K::Iso_rectangle_2									Iso_rectangle_2;
typedef K::Segment_2										Segment_2;
typedef K::Ray_2											Ray_2;
typedef K::Line_2											Line_2;
typedef K::Point_2											Point_2;
typedef CGAL::Polygon_2<K>									Polygon_2;
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
	CGAL::lloyd_optimize_mesh_2(cdt, CGAL::parameters::max_iteration_number = 20);
	std::cout << " done." << std::endl;
	std::cout << "CDT Number of vertices: " << cdt.number_of_vertices() << std::endl;
}

//ps is the source of the ray, pt is the target of the ray
Point_2 VoronoiD_CrossPoint(Ray_2 ray, double nelx, double nely, std::vector<Segment_2> boundarys) {

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
std::vector<Segment_2> VoronoiD_UpdateEdge(double nelx, double nely, 
	segments_lists & rayEdge, segments_lists & halfEdge, std::vector<Segment_2> boundarys)
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
		Point_2 ptn = VoronoiD_CrossPoint(ray, nelx, nely, boundarys); // intersection of the ray with the boundary
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


std::vector<Point_2> reorder_edges(std::vector<Point_2> pnts, std::vector<std::vector<int>> & edges_idx)
{
	// reorder the edge_idx of end to end
	std::vector<std::vector<int>> new_edges = edges_idx;
	edges_idx.erase(edges_idx.begin());

	for (int i = 0; i < new_edges.size() - 1; ++i)
	{
		std::vector<int> ei = new_edges[i];

		for (int j = 0; j < edges_idx.size(); ++j)
		{
			std::vector<int> ej = edges_idx[j];
			if (ei[1] == ej[0])
			{
				new_edges[i + 1] = ej;
				edges_idx.erase(edges_idx.begin() + j);
				break;
			}
			else
				if ((ei[1] == ej[1]))
				{
					new_edges[i + 1][0] = ej[1];
					new_edges[i + 1][1] = ej[0];
					edges_idx.erase(edges_idx.begin() + j);
					break;
				}
		}
	}
	edges_idx.clear();
	edges_idx = new_edges;

	// reorder the new_pnts of order to order
	// note the pnts remain unchanged for edges_idx
	std::vector<Point_2> new_pnts;
	for (int i = 0; i < edges_idx.size()-1; ++i)
	{
		int n1 = edges_idx[i][1];
		if (i == 0)
		{
			int n0 = edges_idx[i][0];
			new_pnts.push_back(pnts[n0]);
			new_pnts.push_back(pnts[n1]);
		}
		else
			new_pnts.push_back(pnts[n1]);
	}
	return new_pnts;
}

std::vector<Point_2> preprocess_vcell(std::vector<Segment_2> edges, std::vector<Point_2> & pnts,
	std::vector<std::vector<int>> & edges_idx, std::string path)
{	
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
	Point_2 center = CGAL::centroid(pnts.begin(), pnts.end(), CGAL::Dimension_tag<0>());

	std::vector<Point_2> new_pnts = reorder_edges(pnts, edges_idx);

	// write to file:
	// 1. pnts number: n
	// 2. center-coordinates: cx cy
	// 3. each pnt coord: px py
	// 4. edge connection: pi pj (begin with 0)
	std::ofstream outfile;
	outfile.open(path);
	outfile << pnts.size() << std::endl;
	outfile << center.x() << ' ' << center.y() << std::endl;
	for (int i = 0; i < pnts.size(); ++i)
		outfile << pnts[i].x() << ' ' << pnts[i].y() << std::endl;
	for (int i = 0; i < edges_idx.size(); ++i)
		outfile << edges_idx[i][0] << ' ' << edges_idx[i][1] << std::endl;

	outfile.close();

	return new_pnts;
}


void generate_CDT_fine(std::vector<Point_2> pnts, std::vector<std::vector<int>> edges_idx, CDT & cdt)
{
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
}

// assuming the center of the mesh is (0,0)
void Lloyd_each(std::vector<Point_2> pnts, std::vector<std::vector<int>> edges_idx, Eigen::MatrixXd & V, Eigen::MatrixXi & F, double x)
{
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

	CGAL::lloyd_optimize_mesh_2(cdt, CGAL::parameters::max_iteration_number = 20);

	// step.2 transform to DT
	DT dt;
	VD vd;
	CDT2DT_VD(dt, vd, cdt);

	V = Eigen::MatrixXd(dt.number_of_vertices(), 2);
	F = Eigen::MatrixXi(dt.number_of_faces(), 3);
	plot_DT(V, F, dt);
}

void add_boundary_edge(std::vector<Segment_2> added_rays, std::vector<Segment_2> & boundary_edges, 
	double halfx, double halfy)
{
	if (added_rays.size() != 2)
		throw "The size of added_rays is not 2!";

	// add the edge on the bounding box
	Segment_2 seg1 = added_rays[0];
	Segment_2 seg2 = added_rays[1];

	K::Vector_2 dir1 = seg1.point(1) - seg1.point(0);
	K::Vector_2 dir2 = seg2.point(1) - seg2.point(0);

	// two situations:
	if (abs(dir1.x() * dir2.y() - dir1.y() * dir2.x()) < 1e-5)
	{
		Segment_2 s;
		for (int i = 0; i < 2; ++i)
		{
			Point_2 p1 = seg1.point(i);
			for (int j = 0; j < 2; ++j)
			{
				Point_2 p2 = seg2.point(j);

				if ((abs(abs(p1.x()) - halfx) < 1e-5 && abs(abs(p2.x()) - halfx) < 1e-5) ||
					(abs(abs(p1.y()) - halfy) < 1e-5 && abs(abs(p2.y()) - halfy) < 1e-5))
				{
					s = Segment_2(p2, p1);
					break;
				}
			}
		}
		boundary_edges.push_back(s);
	}
	else
	{
		Point_2 pf1, pf2;
		for (int i = 0; i < 2; ++i)
		{
			Point_2 p1 = seg1.point(i);
			if (abs(abs(p1.x()) - halfx) < 1e-5 || abs(abs(p1.y()) - halfy) < 1e-5)
				pf1 = p1;

			Point_2 p2 = seg2.point(i);
			if (abs(abs(p2.x()) - halfx) < 1e-5 || abs(abs(p2.y()) - halfy) < 1e-5)
				pf2 = p2;
		}

		Point_2 cand1(pf1.x(), pf2.y());
		Segment_2 candidate11(cand1, pf1);
		Segment_2 candidate12(cand1, pf2);
		double dist1 = 0;		

		Point_2 cand2(pf2.x(), pf1.y());
		Segment_2 candidate21(cand2, pf1);
		Segment_2 candidate22(cand2, pf2);
		double dist2 = 0;

		for (int i = 0; i < boundary_edges.size(); ++i)
		{
			dist1 += CGAL::squared_distance(boundary_edges[i], cand1);
			dist2 += CGAL::squared_distance(boundary_edges[i], cand2);
		}

		if (dist1 > dist2)
		{
			boundary_edges.push_back(candidate11);
			boundary_edges.push_back(candidate12);
		}
		else
		{
			boundary_edges.push_back(candidate21);
			boundary_edges.push_back(candidate22);
		}
	}
}

void add_ray(Point_2 p, Ccb_halfedge_circulator ec, std::vector<Segment_2> rayEdgeBound, 
	std::vector<Segment_2> & boundary_edges, std::vector<Segment_2> & added_rays)
{
	VD::Delaunay_vertex_handle v1 = ec->up();
	VD::Delaunay_vertex_handle v2 = ec->down();

	K::Vector_2 direction(v1->point().y() - v2->point().y(),
		v2->point().x() - v1->point().x()); 
										

	for (int i = 0; i < rayEdgeBound.size(); ++i)
	{
		K::Vector_2 r1(rayEdgeBound[i]);
		Point_2 p0 = rayEdgeBound[i].point(0);
		Point_2 p1 = rayEdgeBound[i].point(1);

		if ((abs(r1.y() * direction.x() - r1.x() * direction.y()) < 1e-5) &&
			(point_equal(p, p1) || point_equal(p, p0)))
		{
			boundary_edges.push_back(rayEdgeBound[i]);
			added_rays.push_back(rayEdgeBound[i]);
			break;
		}
	}
}

void add_segment(Ccb_halfedge_circulator ec, std::vector<Segment_2> & boundary_edges, bool & flag,
	std::vector<Segment_2> & added_rays, double halfx, double halfy, std::vector<Segment_2> boundarys)
{
	double xs = ec->source()->point()[0];
	double ys = ec->source()->point()[1];
	double xt = ec->target()->point()[0];
	double yt = ec->target()->point()[1];
	Segment_2 s(ec->source()->point(), ec->target()->point());

	// the segment maybe out of the box
	if (abs(xs) <= halfx + 1e-5 && abs(ys) <= halfy + 1e-5 &&
		abs(xt) <= halfx + 1e-5 && abs(yt) <= halfy + 1e-5)
	{
		boundary_edges.push_back(s);
	}
	else
	{
		// find the intersection point
		Point_2 p1(0,0);
		for (int i = 0; i < boundarys.size(); ++i)
		{
			const auto result = intersection(s, boundarys[i]);
			if (result) {
				if (const Point_2* p = boost::get<Point_2 >(&*result))
				{
					p1 = *p;
					break;
				}
			}
		}

		// find another point to build the cut segment
		if (abs(xs) <= halfx + 1e-5 && abs(ys) <= halfy + 1e-5)
		{
			Segment_2 s1(ec->source()->point(), p1);
			boundary_edges.push_back(s1);
			added_rays.push_back(s1);
		}
		else if (abs(xt) <= halfx + 1e-5 && abs(yt) <= halfy + 1e-5)
		{
			Segment_2 s1(ec->target()->point(), p1);
			boundary_edges.push_back(s1);
			added_rays.push_back(s1);
		}
		flag = 1;
	}
}

void voronoi_single(Point_2 p, VD vd, double halfx, double halfy, std::vector<Segment_2> rayEdgeBound, 
	std::vector<Segment_2> & boundary_edges, std::vector<Segment_2> boundarys)
{
	std::vector<Segment_2> added_rays;

	Locate_result lr = vd.locate(p); // the face of p located
	VD::Face_handle* f = boost::get<VD::Face_handle>(&lr);

	Ccb_halfedge_circulator ec_start = (*f)->ccb(); // traversing the halfedges on the boundary of f
	Ccb_halfedge_circulator ec = ec_start;

	bool flag = 0;
	do {
		//add_segments_and_update_bounding_box(ec);
		if (ec->is_bisector())
		{
			// TODO:
			throw "Bisector!";
		}
		else
		{
			if (ec->is_ray())
			{
				if (ec->has_source())
				{
					Point_2 p1 = ec->source()->point();
					add_ray(p1, ec, rayEdgeBound, boundary_edges, added_rays);
					flag = 1;
				}
				if (ec->has_target())
				{
					Point_2 p1 = ec->target()->point();
					add_ray(p1, ec, rayEdgeBound, boundary_edges, added_rays);
					flag = 1;
				}
			}
			else if (ec->is_segment())
			{
				add_segment(ec, boundary_edges, flag, added_rays, halfx, halfy, boundarys);
			}
		}
	} while (++ec != ec_start);

	// add the edge on the bounding box
	if (flag) // including boundary rays
		add_boundary_edge(added_rays, boundary_edges, halfx, halfy);
}

void write_cdt(std::string path, Eigen::MatrixXd V, Eigen::MatrixXi F)
{
	// write to file:
	// 1. pnts & faces number: pn, fn
	// 2. each pnt coord: px py
	// 3. face connection: pi pj pk (begin with 0)
	std::ofstream outfile;
	outfile.open(path);
	outfile << V.rows() << ' ' << F.rows() << std::endl;

	for (int i = 0; i < V.rows(); ++i)
		outfile << V(i,0) << ' ' << V(i, 1) << std::endl;

	for (int i = 0; i < F.rows(); ++i)
		outfile << F(i, 0) << ' ' << F(i, 1) << ' ' << F(i, 2) << std::endl;

	outfile.close();
}

struct Cropped_voronoi_from_delaunay {
	std::list<Segment_2> m_cropped_vd;
	Iso_rectangle_2 m_bbox;
	Cropped_voronoi_from_delaunay(const Iso_rectangle_2& bbox) :m_bbox(bbox) {}
	template <class RSL>
	void crop_and_extract_segment(const RSL& rsl) {
		CGAL::Object obj = CGAL::intersection(rsl, m_bbox);
		const Segment_2* s = CGAL::object_cast<Segment_2>(&obj);
		if (s) m_cropped_vd.push_back(*s);
	}
	void operator<<(const Ray_2& ray) { crop_and_extract_segment(ray); }
	void operator<<(const Line_2& line) { crop_and_extract_segment(line); }
	void operator<<(const Segment_2& seg) { crop_and_extract_segment(seg); }
};

int main()
{
	int nelx = 4 * 64;
	int nely = 2 * 64;
	double halfx = nelx / 2.;
	double halfy = nely / 2.;

	// step.1 generate DT & VD
	std::ifstream in("F:\\project\\7-current_opt\\generate-cdt\\cdt_cgal\\data_2d\\seeds.cin");
	std::istream_iterator<Point> begin(in);
	std::istream_iterator<Point> end;

	DT dt;
	VD vd;
	std::vector<std::pair<Point, unsigned>> points;
	int index = 0;
	for (; begin != end; ++begin)
	{
		points.push_back(std::make_pair(*begin, index++));
		Site_2 t(begin->x(), begin->y());
		vd.insert(t);
	}
	dt = DT(points.begin(), points.end());

	// get the ray-edge and segment-edge of the voronoi diagram from the DT
	segments_lists rayEdge, halfEdge;
	DT2VD_edgs(dt, rayEdge, halfEdge);

	// cut the ray-edge under the boundary of [-halfx,-halfy] & [halfx, halfy]
	Segment_2 up(Point_2(halfx , halfy), Point_2(-halfx, halfy));
	Segment_2 down(Point_2(-halfx, -halfy), Point_2(halfx, -halfy));
	Segment_2 left(Point_2(-halfx, -halfy), Point_2(-halfx, halfy));
	Segment_2 right(Point_2(halfx, halfy), Point_2(halfx, -halfy));

	std::vector<Segment_2> boundarys;
	boundarys.push_back(up);
	boundarys.push_back(down);
	boundarys.push_back(left);
	boundarys.push_back(right);
	std::vector<Segment_2> rayEdgeBound = VoronoiD_UpdateEdge(halfx, halfy, rayEdge, halfEdge, boundarys);

	std::string path1 = "F:\\project\\7-current_opt\\generate-cdt\\cdt_cgal\\data_2d\\cvt\\vcell";
	std::string path2 = "F:\\project\\7-current_opt\\generate-cdt\\cdt_cgal\\data_2d\\cvt\\cdt";

	igl::opengl::glfw::Viewer viewer;

	// step.3 get voronoi
	int idx = 0;
	DT::Finite_vertices_iterator vit = dt.finite_vertices_begin();
	for (; vit != dt.finite_vertices_end(); vit++)
	{
		Point_2 p = vit->point();

		//if (idx == 5)
		//{
		//	int a = 2333;
		//}
		//else
		//	int a = 233;

		/// step.0 get the v_edges of each cell
		std::vector<Segment_2> voronoi_edges;
		voronoi_single(p, vd, halfx, halfy, rayEdgeBound, voronoi_edges, boundarys);

		///// step.1 get points and connection from edges
		//std::vector<Point_2> pnts;
		//std::vector<std::vector<int>> edges_idx(voronoi_edges.size());
		//std::string path_v = path1 + std::to_string(idx);

		//// the new_pnts is ordered to assemble polygon
		//std::vector<Point_2> new_pnts = preprocess_vcell(voronoi_edges, pnts, edges_idx, path_v);

		//std::cout << "write voronoi cell-" << idx << " done." << std::endl;

		///// step.2 compute the area of this cell, to decide the local trimesh criterion
		////Polygon_2 poly;
		////for (int i = 0; i < new_pnts.size(); ++i)
		////	poly.push_back(new_pnts[i]);
		////
		////double area;
		////if (poly.is_simple() && poly.is_convex())
		////{
		////	area = abs(poly.area());
		////	//std::cout << area << std::endl;
		////}
		//double x = 0.5;

		//Eigen::MatrixXd V;
		//Eigen::MatrixXi F;
		//Lloyd_each(pnts, edges_idx, V, F, x);

		////igl::opengl::glfw::Viewer viewer;
		////viewer.data().set_mesh(V, F);

		//std::string path_cdt = path2 + std::to_string(idx);
		//write_cdt(path_cdt, V, F);
		//std::cout << "write fine cdt-" << idx << "done." << std::endl;

		for (int i = 0; i < voronoi_edges.size(); ++i)
		{
			Eigen::RowVector2d P1(voronoi_edges[i].point(0).x(), voronoi_edges[i].point(0).y());
			Eigen::RowVector2d P2(voronoi_edges[i].point(1).x(), voronoi_edges[i].point(1).y());
			viewer.data().add_edges(P1, P2, Eigen::RowVector3d(1, 0, 0));
		}
		//viewer.launch();

		idx++;
	}
	//for (int i = 0; i < rayEdgeBound.size(); ++i)
	//{
	//	Eigen::RowVector2d P1(rayEdgeBound[i].point(0).x(), rayEdgeBound[i].point(0).y());
	//	Eigen::RowVector2d P2(rayEdgeBound[i].point(1).x(), rayEdgeBound[i].point(1).y());
	//	viewer.data().add_edges(P1, P2, Eigen::RowVector3d(0, 1, 0));
	//}
	viewer.launch();
	//std::cout << area << std::endl;
	//viewer.launch();

	getchar();
	return 1;
}
