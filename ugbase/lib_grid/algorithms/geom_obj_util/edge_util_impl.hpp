// created by Sebastian Reiter
// s.b.reiter@googlemail.com
// y08 m12 d16

#ifndef __H__LIB_GRID__EDGE_UTIL_IMPL__
#define __H__LIB_GRID__EDGE_UTIL_IMPL__

//#include "edge_util.h"
#include "lib_grid/grid/grid_util.h"

namespace ug
{
////////////////////////////////////////////////////////////////////////
//	SplitEdge
//	see edge_operations.h for detailed description
template<class TVertex>
TVertex* SplitEdge(Grid& grid, EdgeBase* e, bool bConservative)
{
	return SplitEdge<TVertex>(grid, grid, e, NULL, bConservative);
}

////////////////////////////////////////////////////////////////////////
//	SplitEdge
//	see edge_operations.h for detailed description
template<class TVertex>
TVertex* SplitEdge(Grid& destGrid, Grid& srcGrid, EdgeBase* e,
						AVertexBase* paAssociatedVertices,
						bool bConservative)
{
	TVertex* newVertex;
	if(&destGrid == &srcGrid)
		newVertex = *destGrid.create<TVertex>(e);
	else
		newVertex = *destGrid.create<TVertex>();

	if(CreateEdgeSplitGeometry(destGrid, srcGrid, e, newVertex, paAssociatedVertices))
	{
		if(!bConservative)
		{
		//	erase unused elements.
			if(!srcGrid.option_is_enabled(FACEOPT_AUTOGENERATE_EDGES))
			{
			//	we have to erase the faces manually
			//	collect them
				std::vector<Face*> vFaces;
				CollectFaces(vFaces, srcGrid, e, false);

			//	erase them
				for(std::vector<Face*>::iterator iter = vFaces.begin();
					iter != vFaces.end(); ++iter)
				{
					srcGrid.erase(*iter);
				}
			}

			if((!srcGrid.option_is_enabled(VOLOPT_AUTOGENERATE_EDGES)) &&
				(!srcGrid.option_is_enabled(VOLOPT_AUTOGENERATE_FACES)))
			{
			//	we have to erase them manually
			//	collect them
				std::vector<Volume*> vVolumes;
				CollectVolumes(vVolumes, srcGrid, e, false);

			//	erase them
				for(std::vector<Volume*>::iterator iter = vVolumes.begin();
					iter != vVolumes.end(); ++iter)
				{
					srcGrid.erase(*iter);
				}
			}

		//	erase the edge
			srcGrid.erase(e);
		}

	//	return the new vertex
		return newVertex;
	}

//	something went wrong in CreateEdgeSplitGeometry.
//	erase the new vertex and return NULL
	destGrid.erase(newVertex);
	return NULL;
}


template <class TEdgeIterator>
void MarkCreaseEdges(Grid& grid, ISubsetHandler& sh,
					TEdgeIterator edgesBegin, TEdgeIterator edgesEnd,
					int subsetIndex, number angle,
					APosition& aPos,
					ANormal* paFaceNormal)
{

//	get the position accessor
	if(!grid.has_vertex_attachment(aPos))
		return;
	
	Grid::VertexAttachmentAccessor<APosition> aaPos(grid, aPos);
	
//	we'll store face normals in those vectors:
	vector3 n[2];
	
//	associated faces are stored in this array
	Face* f[2];
	
//	all dot-products between normals lower than minDot mark a crease.
	number minDot = cos(angle * 3.14159265 / 180.f);
	
//	iterate through the edges
	for(TEdgeIterator iter = edgesBegin; iter != edgesEnd; ++iter)
	{
		EdgeBase* e = *iter;
	//	get the associated faces
	//	all edges that do not have exactly 2 associated edges
	//	are regarded as seam-edges
		if(GetAssociatedFaces(f, grid, e, 2) == 2){
		//	get the normals of the associated faces
			CalculateNormal(n[0], f[0], aaPos);
			CalculateNormal(n[1], f[1], aaPos);
		//	if the dot-product is lower than minDot, then the edge is a crease edge.
			if(VecDot(n[0], n[1]) < minDot)
				sh.assign_subset(e, subsetIndex);
		}
		else{
			sh.assign_subset(e, subsetIndex);
		}
	}
}

template<class TVertexPositionAttachmentAccessor>
typename TVertexPositionAttachmentAccessor::ValueType
CalculateCenter(EdgeBase* e, TVertexPositionAttachmentAccessor& aaPosVRT)
{
	typename TVertexPositionAttachmentAccessor::ValueType v;
//	init v with 0.
	VecSet(v, 0);

//	sum up
	VecAdd(v, aaPosVRT[e->vertex(0)], aaPosVRT[e->vertex(1)]);

//	average
	VecScale(v, v, 0.5);

	return v;
}

}//	end of namespace

#endif
