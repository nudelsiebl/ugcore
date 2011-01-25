// created by Sebastian Reiter
// s.b.reiter@googlemail.com
// 11.01.2011 (m,d,y)

#ifndef __H__UG__HANGING_NODE_REFINER_GRID__
#define __H__UG__HANGING_NODE_REFINER_GRID__

#include "hanging_node_refiner_base.h"

namespace ug
{

class HangingNodeRefiner_Grid : public HangingNodeRefinerBase
{
	public:
		HangingNodeRefiner_Grid(IRefinementCallback* refCallback = NULL);
		HangingNodeRefiner_Grid(Grid& grid,
								IRefinementCallback* refCallback = NULL);

		virtual ~HangingNodeRefiner_Grid();

		virtual void grid_to_be_destroyed(Grid* grid);

		void assign_grid(Grid& grid);
		virtual Grid* get_associated_grid()		{return m_pGrid;}

	protected:
	///	performs registration and deregistration at a grid.
	/**	Initializes all grid related variables.
	 *  call set_grid(NULL) to unregister the observer from a grid.
	 *
	 * 	Please note that though the base grid features a set_grid method,
	 *  it is not declared virtual. This is because we want to call it
	 *  during construction and destruction.*/
		void set_grid(Grid* grid);

	///	erases unused refined elements
		virtual void post_refine();

		virtual void refine_constraining_edge(ConstrainingEdge* cge);
		virtual void refine_edge_with_normal_vertex(EdgeBase* e,
											VertexBase** newCornerVrts = NULL);

		virtual void refine_face_with_normal_vertex(Face* f,
											VertexBase** newCornerVrts = NULL);
		virtual void refine_constraining_face(ConstrainingFace* cgf);

		virtual void refine_volume_with_normal_vertex(Volume* v,
											VertexBase** newVolumeVrts = NULL);

	///	Returns the vertex associated with the edge
		virtual VertexBase* get_center_vertex(EdgeBase* e);

	///	Associates a vertex with the edge.
		virtual void set_center_vertex(EdgeBase* e, VertexBase* v);

	///	Returns the vertex associated with the face
		virtual VertexBase* get_center_vertex(Face* f);

	///	Associates a vertex with the face.
		virtual void set_center_vertex(Face* f, VertexBase* v);

	private:
		Grid* 			m_pGrid;
		AVertexBase		m_aVertex;
		Grid::EdgeAttachmentAccessor<AVertexBase>		m_aaVertexEDGE;
		Grid::FaceAttachmentAccessor<AVertexBase>		m_aaVertexFACE;
};

}//	end of namespace

#endif
