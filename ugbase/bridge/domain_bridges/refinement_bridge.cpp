// created by Sebastian Reiter
// s.b.reiter@googlemail.com
// 14.07.2011 (m,d,y)
 
#include <vector>
#include <string>
#include <sstream>

// include bridge
#include "bindings/lua/lua_user_data.h"
#include "bridge/bridge.h"
#include "bridge/util.h"
#include "bridge/util_domain_dependent.h"
#include "lib_disc/domain.h"
#include "lib_disc/domain_traits.h"
#include "lib_grid/lib_grid.h"
//todo: include this in algorithms.h
#include "lib_grid/algorithms/refinement/adaptive_regular_mg_refiner.h"
#include "lib_grid/algorithms/refinement/refinement_projectors/refinement_projection_handler.h"

using namespace std;

namespace ug{

/**
 * \defgroup refinement_bridge Refinement Bridge
 * \ingroup domain_bridge
 * \{
 */

////////////////////////////////////////////////////////////////////////////////
///	Creates a global domain refiner.
/**	Automatically chooses whether a parallel refiner is required.*/
template <typename TDomain>
static SmartPtr<IRefiner> GlobalDomainRefiner(TDomain* dom)
{
//todo: support normal grids, too!

	#ifdef UG_PARALLEL
		if(pcl::GetNumProcesses() > 1){
			return SmartPtr<IRefiner>(new ParallelGlobalRefiner_MultiGrid(*dom->distributed_grid_manager()));
		}
	#endif

	return SmartPtr<IRefiner>(new GlobalMultiGridRefiner(*dom->grid()));
}

////////////////////////////////////////////////////////////////////////////////
///	Creates an adaptive hanging node domain refiner.
/**	Automatically chooses whether a parallel refiner is required.*/
template <typename TDomain>
static SmartPtr<IRefiner> HangingNodeDomainRefiner(TDomain* dom)
{
	if(!dom->is_adaptive()){
		UG_THROW("Can't create an adaptive refiner for the given domain. "
				 	   "Construct the domain with isAdaptive enabled.");
	}

//todo: support normal grids, too!
	#ifdef UG_PARALLEL
		if(pcl::GetNumProcesses() > 1){
			return SmartPtr<IRefiner>(new ParallelHangingNodeRefiner_MultiGrid(*dom->distributed_grid_manager()));
		}
	#endif

	return SmartPtr<IRefiner>(new HangingNodeRefiner_MultiGrid(*dom->grid()));
}

////////////////////////////////////////////////////////////////////////////////
///	Creates an adaptive regular domain refiner.
/**	Automatically chooses whether a parallel refiner is required.*/
template <typename TDomain>
static SmartPtr<IRefiner> CreateAdaptiveRegularDomainRefiner(TDomain* dom)
{
	if(!dom->is_adaptive()){
		UG_THROW("Can't create an adaptive refiner for the given domain. "
				 	   "Construct the domain with isAdaptive enabled.");
	}

//todo: support normal grids, too!
//todo: support parallelism, too!
	/*#ifdef UG_PARALLEL
		if(pcl::GetNumProcesses() > 1){
			return SmartPtr<IRefiner>(new ParallelHangingNodeRefiner_MultiGrid(*dom->distributed_grid_manager()));
		}
	#endif*/

	return SmartPtr<IRefiner>(new AdaptiveRegularRefiner_MultiGrid(*dom->grid()));
}

////////////////////////////////////////////////////////////////////////////////
///	Creates a global fractured domain refiner.
template <class TDomain>
static SmartPtr<GlobalFracturedMediaRefiner>
CreateGlobalFracturedDomainRefiner(TDomain* dom)
{
	if(!dom->is_adaptive()){
		UG_THROW("Can't create an fractured domain refiner for the given domain. "
				 	   "Construct the domain with isAdaptive enabled.");
	}

	GlobalFracturedMediaRefiner* ref = NULL;
	#ifdef UG_PARALLEL
		if(pcl::GetNumProcesses() > 1){
			ref = new ParallelGlobalFracturedMediaRefiner(*dom->distributed_grid_manager());
		}
	#endif

	if(!ref)
		ref = new GlobalFracturedMediaRefiner(*dom->grid());

	ref->set_subset_handler(*dom->subset_handler());

	return SmartPtr<GlobalFracturedMediaRefiner>(ref);
}

////////////////////////////////////////////////////////////////////////////////
/// marks face for anisotropic refinement, if it contains edges below given sizeRatio
/**
 * marks face and for anisotropic refinement, if it contains edges
 * below given sizeRatio. These edges are also marked.
 * @return true, if face has been marked for anisotropic refinement.
 * This is the case, when one of its edges has been marked for refinement.*/
template <class TAAPos> bool MarkIfAnisotropic(Face* f, IRefiner* refiner, number sizeRatio, TAAPos& aaPos)
{
	bool marked = false;
	uint num_edges = f->num_edges();
	vector<EdgeBase*> edges(num_edges);
	// collect associated edges
	CollectAssociated(edges, *refiner->grid(), f);

	//	find the shortest edge
	EdgeBase* minEdge = FindShortestEdge(edges.begin(), edges.end(), aaPos);
	UG_ASSERT(minEdge,
			"Associated edges of each face have to exist at this point.");
	number minLen = EdgeLength(minEdge, aaPos);

	//	compare all associated edges of f against minEdge (even minEdge itself,
	//	if somebody sets edgeRatio to 1 or higher)
	for (uint i_edge = 0; i_edge < num_edges; ++i_edge) {
		EdgeBase* e = edges[i_edge];
		number len = EdgeLength(e, aaPos);
		//	to avoid division by 0, we only consider edges with a length > 0
		if(len > 0) {
			if(minLen / len <= sizeRatio) {
				//	the edge will be refined
				refiner->mark(e);
				marked = true;
			}
		}
	}

	if(marked) {
		//	if a vertex has been marked, also mark the face, or else just a hanging
		//	node would be inserted.
		//	We do not mark other associated objects here since this would
		//	cause creation of a closure and would also behave differently
		//	in a parallel environment, compared to a serial environment.
		//	By using RM_ANISOTROPIC, we avoid that associated edges of
		//	the marked face will automatically be marked, too.
		refiner->mark(f, RM_ANISOTROPIC);
	}

	return marked;
}

////////////////////////////////////////////////////////////////////////////////
///	Marks all elements from refinement.
/**	If used in a parallel environment only elements on the calling procs
 * are marked.
 */
static void MarkForRefinement_All(SmartPtr<IRefiner> ref)
{
	PROFILE_FUNC_GROUP("grid");
	Grid* g = ref->get_associated_grid();
	if(!g){
		UG_LOG("Refiner is not registered at a grid. Aborting.\n");
		return;
	}
	ref->mark(g->vertices_begin(), g->vertices_end());
	ref->mark(g->edges_begin(), g->edges_end());
	ref->mark(g->faces_begin(), g->faces_end());
	ref->mark(g->volumes_begin(), g->volumes_end());
}

////////////////////////////////////////////////////////////////////////////////
///	Marks all vertices in the given d-dimensional sphere.
template <class TDomain>
void MarkForRefinement_VerticesInSphere(TDomain& dom, SmartPtr<IRefiner> refiner,
									const typename TDomain::position_type& center,
									number radius)
{
	PROFILE_FUNC_GROUP("grid");
	typedef typename TDomain::position_type 			position_type;
	typedef typename TDomain::position_accessor_type	position_accessor_type;

//	make sure that the refiner was created for the given domain
	if(refiner->get_associated_grid() != dom.grid().get()){
		throw(UGError("ERROR in MarkForRefinement_VerticesInSphere: "
					"Refiner was not created for the specified domain. Aborting."));
	}

	Grid& grid = *refiner->get_associated_grid();
	position_accessor_type& aaPos = dom.position_accessor();

//	we'll compare against the square radius.
	number radiusSq = radius * radius;

//	we'll store associated edges, faces and volumes in those containers
	vector<EdgeBase*> vEdges;
	vector<Face*> vFaces;
	vector<Volume*> vVols;

//	iterate over all vertices of the grid. If a vertex is inside the given sphere,
//	then we'll mark all associated elements.
	for(VertexBaseIterator iter = grid.begin<VertexBase>();
		iter != grid.end<VertexBase>(); ++iter)
	{
		if(VecDistanceSq(center, aaPos[*iter]) <= radiusSq){
			CollectAssociated(vEdges, grid, *iter);
			CollectAssociated(vFaces, grid, *iter);
			CollectAssociated(vVols, grid, *iter);

			refiner->mark(vEdges.begin(), vEdges.end());
			refiner->mark(vFaces.begin(), vFaces.end());
			refiner->mark(vVols.begin(), vVols.end());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
///	Marks all elements which lie completely in the given d-dimensional sphere.
/**	Valid parameters for TElem are EdgeBase, Face, Volume.*/
template <class TDomain, class TElem>
void MarkForRefinement_ElementsInSphere(TDomain& dom, SmartPtr<IRefiner> refiner,
									const typename TDomain::position_type& center,
									number radius)
{
	PROFILE_FUNC_GROUP("grid");
	typedef typename TDomain::position_type 			position_type;
	typedef typename TDomain::position_accessor_type	position_accessor_type;
	typedef typename geometry_traits<TElem>::iterator	ElemIter;

//	make sure that the refiner was created for the given domain
	if(refiner->get_associated_grid() != dom.grid().get()){
		throw(UGError("ERROR in MarkForRefinement_VerticesInCube: "
					"Refiner was not created for the specified domain. Aborting."));
	}

	Grid& grid = *refiner->get_associated_grid();
	position_accessor_type& aaPos = dom.position_accessor();

//	we'll compare against the square radius.
	number radiusSq = radius * radius;

//	we'll store associated edges, faces and volumes in those containers
	vector<EdgeBase*> vEdges;
	vector<Face*> vFaces;
	vector<Volume*> vVols;

//	iterate over all elements of the grid. If all vertices of an element are inside
//	the given sphere, then we'll mark those elements.
	for(ElemIter iter = grid.begin<TElem>();
		iter != grid.end<TElem>(); ++iter)
	{
	//	get element
		TElem* elem = *iter;

	//	bool flag to check whether all vertices are in the sphere
		bool bInSphere = true;

	//	loop all vertices
		for(size_t i = 0; i < elem->num_vertices(); ++i)
		{
		//	check if vertex is in sphere
			if(VecDistanceSq(center, aaPos[elem->vertex(i)]) > radiusSq)
				bInSphere = false;
		}

	//	mark the element
		if(bInSphere)
			refiner->mark(elem);
	}
}

////////////////////////////////////////////////////////////////////////////////
///	Marks all elements which have vertices in the given d-dimensional cube.
/**	Make sure that TAPos is an attachment of vector_t position types.*/
template <class TDomain>
void MarkForRefinement_VerticesInCube(TDomain& dom, SmartPtr<IRefiner> refiner,
									const typename TDomain::position_type& min,
									const typename TDomain::position_type& max)
{
	PROFILE_FUNC_GROUP("grid");
	typedef typename TDomain::position_type 			position_type;
	typedef typename TDomain::position_accessor_type	position_accessor_type;

//	make sure that the refiner was created for the given domain
	if(refiner->get_associated_grid() != dom.grid().get()){
		throw(UGError("ERROR in MarkForRefinement_VerticesInCube: "
					"Refiner was not created for the specified domain. Aborting."));
	}

	Grid& grid = *refiner->get_associated_grid();
	position_accessor_type& aaPos = dom.position_accessor();

//	we'll store associated edges, faces and volumes in those containers
	vector<EdgeBase*> vEdges;
	vector<Face*> vFaces;
	vector<Volume*> vVols;

//	iterate over all vertices of the grid. If a vertex is inside the given cube,
//	then we'll mark all associated elements.
	for(VertexBaseIterator iter = grid.begin<VertexBase>();
		iter != grid.end<VertexBase>(); ++iter)
	{
	//	Position
		position_type& pos = aaPos[*iter];

	//	check flag
		bool bRefine = true;

	//	check node
		for(size_t d = 0; d < pos.size(); ++d)
			if(pos[d] < min[d] || max[d] < pos[d])
				bRefine = false;

		if(bRefine)
		{
			CollectAssociated(vEdges, grid, *iter);
			CollectAssociated(vFaces, grid, *iter);
			CollectAssociated(vVols, grid, *iter);

			refiner->mark(vEdges.begin(), vEdges.end());
			refiner->mark(vFaces.begin(), vFaces.end());
			refiner->mark(vVols.begin(), vVols.end());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
///	Marks the long edges in anisotropic faces and faces with a big area in anisotropic volumes.
/**
 * THE NOT HIGHLY SUCCESSFUL IMPLEMENTATION OF THIS METHOD LEAD TO NEW INSIGHT.
 * A NEW ANISOTROPIC REFINEMENT STRATEGY WILL BE IMPLEMENTED, WHICH WILL ALSO
 * LEAD TO A REIMPLEMENTATION OF THIS METHOD. BEST NOT TO USE IT UNTIL THEN.
 *
 * The sizeRatio determines Whether a face or a volume is considered anisotropic.
 * Make sure that the sizeRatio is in the interval [0, 1]. If the
 * ratio of the shortest edge to an other edge falls below the given threshold,
 * then the associated face is considered anisotropic and the longer edge will be
 * marked. The face itself will then be marked for anisotropic refinement.
 * The same technique is applied to volumes, this time however the ratio between
 * face-areas is considered when judging whether a volume is anisotropic.
 *
 * VOLUME MARKS ARE CURRENTLY DISABLED
 *
 * Note that this algorithm only really works for a serial environment.
 * \todo	improve it so that it works in parallel, too. A specialization of
 * 			HangingNodeRefiner may be required for this.
 *
 * \todo	activate and improve volume marks
 **/
template <class TDomain>
void MarkForRefinement_AnisotropicElements(TDomain& dom, SmartPtr<IRefiner> refiner,
											number sizeRatio)
{
	PROFILE_FUNC_GROUP("grid");
	typedef typename TDomain::position_type 			position_type;
	typedef typename TDomain::position_accessor_type	position_accessor_type;

//	make sure that the refiner was created for the given domain
	if(refiner->get_associated_grid() != dom.grid().get()){
		throw(UGError("ERROR in MarkForRefinement_VerticesInCube: "
					"Refiner was not created for the specified domain. Aborting."));
	}

//	access the grid and the position attachment
	Grid& grid = *refiner->get_associated_grid();
	position_accessor_type& aaPos = dom.position_accessor();

//	If the grid is a multigrid, we want to avoid marking of elements, which do
//	have children
	MultiGrid* pmg = dynamic_cast<MultiGrid*>(&grid);

//	make sure that the grid automatically generates sides for each element
	if(!grid.option_is_enabled(GRIDOPT_AUTOGENERATE_SIDES)){
		UG_LOG("WARNING in MarkForRefinement_AnisotropicElements: "
				"Enabling GRIDOPT_AUTOGENERATE_SIDES.\n");
		grid.enable_options(GRIDOPT_AUTOGENERATE_SIDES);
	}

//	we'll store associated edges and faces in those containers
	vector<EdgeBase*> edges;
	vector<Face*> faces;

//	iterate over all faces of the grid.
	for(FaceIterator iter = grid.begin<Face>();
		iter != grid.end<Face>(); ++iter)
	{
		Face* f = *iter;

	//	ignore faces with children
		if(pmg && pmg->has_children(f))
			continue;

	//	collect associated edges
		CollectAssociated(edges, grid, f);

	//	find the shortest edge
		EdgeBase* minEdge = FindShortestEdge(edges.begin(), edges.end(), aaPos);
		UG_ASSERT(minEdge, "Associated edges of each face have to exist at this point.");
		number minLen = EdgeLength(minEdge, aaPos);

	//	compare all associated edges of f against minEdge (even minEdge itself,
	//	if somebody sets edgeRatio to 1 or higher)
		for(size_t i_edge = 0; i_edge < edges.size(); ++i_edge){
			EdgeBase* e = edges[i_edge];
			number len = EdgeLength(e, aaPos);
		//	to avoid division by 0, we only consider edges with a length > 0
			if(len > 0){
				if(minLen / len <= sizeRatio){
				//	the edge will be refined
					refiner->mark(e);

				//	we'll also mark the current face, or else just a hanging
				//	node would be inserted.
				//	We do not mark other associated objects here since this would
				//	cause creation of a closure and would also behave differently
				//	in a parallel environment, compared to a serial environment.
				//	By using RM_ANISOTROPIC, we avoid that associated edges of
				//	the marked face will automatically be marked, too.
					refiner->mark(f, RM_ANISOTROPIC);
				}
			}
		}
	}

//	iterate over all faces again. We have to make sure that faces which have
//	a marked short edge are refined regular.
//	first push all marked faces into a queue
//	we're using grid::mark to make sure that each face lies only once on the queue.
//	Grid::mark has nothing to do with refinement. It is just a helper for us.
	grid.begin_marking();

	queue<Face*> queFaces;
	for(FaceIterator iter = grid.begin<Face>(); iter != grid.end<Face>(); ++iter){
		queFaces.push(*iter);
		grid.mark(*iter);
	}

	while(!queFaces.empty()){
		Face* f = queFaces.front();
		queFaces.pop();

		if(pmg && pmg->has_children(f)){
			grid.unmark(f);
			continue;
		}

	//	collect associated edges
		CollectAssociated(edges, grid, f);
/*
		if(refiner->get_mark(f) == RM_NONE){
			bool gotOne = false;
			for(size_t i_edge = 0; i_edge < edges.size(); ++i_edge){
				EdgeBase* e = edges[i_edge];
				if(refiner->get_mark(e) != RM_NONE){
					gotOne = true;
					break;
				}
			}

			if(gotOne){
				for(size_t i_edge = 0; i_edge < edges.size(); ++i_edge){
					EdgeBase* e = edges[i_edge];
					if(refiner->get_mark(e) == RM_NONE){
						refiner->mark(e);
						CollectFaces(faces, grid, e);
						for(size_t i_face = 0; i_face < faces.size(); ++i_face){
							Face* nbr = faces[i_face];
							if(!grid.is_marked(nbr)
							   && (refiner->get_mark(nbr) == RM_ANISOTROPIC))
							{
								grid.mark(nbr);
								queFaces.push(nbr);
							}
						}
					}
				}
				refiner->mark(f);
			}
		}
		else */if(refiner->get_mark(f) == RM_ANISOTROPIC){
		//	find the shortest edge
			EdgeBase* minEdge = FindShortestEdge(edges.begin(), edges.end(), aaPos);
			UG_ASSERT(minEdge, "Associated edges of each face have to exist at this point.");
			number minLen = EdgeLength(minEdge, aaPos);

		//	check if a short edge and a long edge is selected
			bool longEdgeSelected = false;
			bool shortEdgeSelected = false;

			for(size_t i_edge = 0; i_edge < edges.size(); ++i_edge){
				EdgeBase* e = edges[i_edge];
				if(refiner->get_mark(e) == RM_NONE)
					continue;

				number len = EdgeLength(e, aaPos);
			//	to avoid division by 0, we only consider edges with a length > 0
				if(len > 0){
					if(minLen / len <= sizeRatio){
						longEdgeSelected = true;
					}
					else{
						shortEdgeSelected = true;
					}
				}
			}

		//	if a short edge and a long edge was selected, we'll have to mark all
		//	edges and push associated faces with anisotropic mark to the queue
			if(longEdgeSelected && shortEdgeSelected){
				for(size_t i_edge = 0; i_edge < edges.size(); ++i_edge){
					EdgeBase* e = edges[i_edge];
					if(refiner->get_mark(e) == RM_NONE){
					//	mark it and push associated anisotropic faces to the queue
						refiner->mark(e);
	//!!!

	if(ConstrainingEdge::type_match(e)){
		UG_LOG("CONSTRAINING EDGE MARKED (2)\n");
	}

						CollectFaces(faces, grid, e);
						for(size_t i_face = 0; i_face < faces.size(); ++i_face){
							Face* nbr = faces[i_face];
							if(!grid.is_marked(nbr)
							   && (refiner->get_mark(nbr) == RM_ANISOTROPIC))
							{
								grid.mark(nbr);
								queFaces.push(nbr);
							}
						}
					}
				}
			}
		}
	//	now unmark the face
		grid.unmark(f);
	}

	grid.end_marking();

//	mark unmarked neighbors of marked edges for regular refinement
/*
	for(FaceIterator iter = grid.begin<Face>();
		iter != grid.end<Face>(); ++iter)
	{
		Face* f = *iter;

	//	if it is already marked, leave it as it is
		if(refiner->get_mark(f) != RM_NONE)
			continue;

	//	ignore faces with children
		if(pmg && pmg->has_children(f))
			continue;

		for(size_t i = 0; i < f->num_edges(); ++i){
			if(refiner->get_mark(grid.get_side(f, i)) != RM_NONE)
				refiner->mark(f);
		}
	}*/

/*
//	now that all faces are marked, we can process volumes. We consider a
//	volume which has an anisotropic side as an anisotropic volume
	for(VolumeIterator iter = grid.begin<Volume>();
		iter != grid.end<Volume>(); ++iter)
	{
		Volume* v = *iter;

	//	collect associated faces
		CollectAssociated(faces, grid, v);

	//	find the smallest face
		Face* minFace = FindSmallestFace(faces.begin(), faces.end(), aaPos);
		UG_ASSERT(minFace, "Associated faces of each volume have to exist at this point.");
		number minArea = FaceArea(minFace, aaPos);

	//	compare all associated faces of v against minArea
		for(size_t i_face = 0; i_face < faces.size(); ++i_face){
			Face* f = faces[i_face];
			number area = FaceArea(f, aaPos);
		//	avoid division by 0
			if(area > 0){
				if(minArea / area <= sizeRatio){
				//	the face will be refined. If it is already marked, we'll
				//	leave it at that, to keep the anisotropy.
				//	If it wasn't marked, we'll mark it for full refinement
				//	(all anisotropic faces have already been marked).
					if(refiner->get_mark(f) == RM_NONE)
						refiner->mark(f);

				//	the volume itself now has to be marked, too.
					refiner->mark(v, RM_ANISOTROPIC);
				}
			}
		}
	}*/
}

///////
/**
 *
 */
template <class TDomain>
void MarkForRefinement_AnisotropicElements2(TDomain& dom, SmartPtr<IRefiner> refiner,
												number sizeRatio)
{
	PROFILE_FUNC_GROUP("grid");
	typedef typename TDomain::position_type 			position_type;
	typedef typename TDomain::position_accessor_type	position_accessor_type;

//	make sure that the refiner was created for the given domain
	if(refiner->get_associated_grid() != dom.grid().get()){
		throw(UGError("ERROR in MarkForRefinement_VerticesInCube: "
					"Refiner was not created for the specified domain. Aborting."));
	}

//	access the grid and the position attachment
	Grid& grid = *refiner->get_associated_grid();
	position_accessor_type& aaPos = dom.position_accessor();
	IRefiner* ref = refiner.get_nonconst();

//	If the grid is a multigrid, we want to avoid marking of elements, which do
//	have children
	MultiGrid* pmg = dynamic_cast<MultiGrid*>(&grid);

//	make sure that the grid automatically generates sides for each element
	if(!grid.option_is_enabled(GRIDOPT_AUTOGENERATE_SIDES)){
		UG_LOG("WARNING in MarkForRefinement_AnisotropicElements: "
				"Enabling GRIDOPT_AUTOGENERATE_SIDES.\n");
		grid.enable_options(GRIDOPT_AUTOGENERATE_SIDES);
	}

//	we'll store associated edges, faces and volumes in those containers
	vector<EdgeBase*> edges;
	vector<Face*> faces;
	vector<Volume*> volumes;
//	iterate over all faces of the grid.
	for(FaceIterator iter = grid.begin<Face>();
		iter != grid.end<Face>(); ++iter)
	{
		Face* f = *iter;
		// ignore faces with children
		if(pmg && pmg->has_children(f))
			continue;

		// if faces has been marked, store it for later marking of its neighbours
		if(MarkIfAnisotropic(f, ref, sizeRatio, aaPos))
			faces.push_back(f);
		else // fixme: mark for regular refinement should not be needed!
			refiner->mark(f);
	}

// if a face is marked for anisotropic refinement (AR),
// also mark associated volumes for AR
	for(vector<Face*>::iterator iter = faces.begin(); iter != faces.end(); ++iter)
		CollectVolumes(volumes, grid, *iter, false);

	refiner->mark(volumes.begin(), volumes.end(), RM_ANISOTROPIC);
}


template <class TDomain>
void MarkForRefinement_ElementsByLuaCallback(TDomain& dom, SmartPtr<IRefiner> refiner,
											 double time, size_t maxLvl,
											 const char* luaCallbackName)
{
	PROFILE_FUNC();
	typedef typename TDomain::grid_type TGrid;
	typedef typename TDomain::subset_handler_type TSubsetHandler;
	typedef typename TDomain::position_type TPos;
	typedef typename TDomain::position_accessor_type TAAPos;
	typedef typename domain_traits<TDomain::dim>::element_type TElem;
	typedef typename TGrid::template traits<TElem>::iterator TIter;

	TGrid& g = *dom.grid();
	TSubsetHandler& sh = *dom.subset_handler();
	TAAPos aaPos = dom.position_accessor();

	LuaFunction<int, number>	callback;
//	we'll pass the following arguments: x, y, z, lvl, si, time
	callback.set_lua_callback(luaCallbackName, 6);

	for(TIter iter = g.template begin<TElem>(); iter != g.template end<TElem>(); ++iter)
	{
		TElem* e = *iter;
		size_t lvl = g.get_level(e);
		if(lvl >= maxLvl)
			continue;

		if(!g.has_children(e)){
			int refine = 0;
			TPos tpos = CalculateCenter(e, aaPos);
			vector3 pos;
			VecCopy(pos, tpos, 0);
			callback(refine, 6, pos.x(), pos.y(), pos.z(), (number)lvl,
					 (number)sh.get_subset_index(e), (number)time);
			if(refine){
				refiner->mark(e);
			}
		}
	}
}

template <class TDomain>
void MarkForCoarsen_ElementsByLuaCallback(TDomain& dom, SmartPtr<IRefiner> refiner,
										  double time, const char* luaCallbackName)
{
	PROFILE_FUNC();
	if(!refiner->coarsening_supported()){
		UG_LOG("WARNING in MarkForCoarsen_ElementsByLuaCallback: "
			   "Refiner doesn't support coarsening!\n");
		return;
	}

	typedef typename TDomain::grid_type TGrid;
	typedef typename TDomain::subset_handler_type TSubsetHandler;
	typedef typename TDomain::position_type TPos;
	typedef typename TDomain::position_accessor_type TAAPos;
	typedef typename domain_traits<TDomain::dim>::element_type TElem;
	typedef typename TGrid::template traits<TElem>::iterator TIter;

	TGrid& g = *dom.grid();
	TSubsetHandler& sh = *dom.subset_handler();
	TAAPos aaPos = dom.position_accessor();

	LuaFunction<int, number>	callback;
//	we'll pass the following arguments: x, y, z, lvl, si, time
	callback.set_lua_callback(luaCallbackName, 6);

	for(TIter iter = g.template begin<TElem>(); iter != g.template end<TElem>(); ++iter)
	{
		TElem* e = *iter;
		if(!g.has_children(e)){
			int coarsen = 0;
			TPos tpos = CalculateCenter(e, aaPos);
			vector3 pos;
			VecCopy(pos, tpos, 0);
			callback(coarsen, 6, pos.x(), pos.y(), pos.z(), (number)g.get_level(e),
					 (number)sh.get_subset_index(e), (number)time);
			if(coarsen){
				refiner->mark(e, RM_COARSEN);
			}
		}
	}
}

// end group refinement_bridge
/// \}

////////////////////////////////////////////////////////////////////////////////
//	REFINEMENT PROJECTORS
template <class TDomain>
SmartPtr<RefinementProjectionHandler<typename TDomain::position_attachment_type> >
DomainRefinementProjectionHandler(TDomain* dom)
{
	typedef RefinementProjectionHandler<typename TDomain::position_attachment_type>	TRefProj;
	return SmartPtr<TRefProj>(
			new TRefProj(dom->subset_handler(), dom->position_attachment()));
}

template <class TDomain>
SmartPtr<IRefinementCallback>
LinearProjector(TDomain* dom)
{
	typedef RefinementCallbackLinear<typename TDomain::position_attachment_type>	TRefProj;
	return SmartPtr<TRefProj>(
			new TRefProj(*dom->grid(), dom->position_attachment()));
}

///	Creates a refinement projector which projects new vertices onto a sphere
/** Specify a domain, the center of the sphere (cx, cy, cz), and the sphere's radius.
 */
template <class TDomain>
SmartPtr<IRefinementCallback>
SphereProjector(TDomain* dom, number x, number y, number z, number radius)
{
	typedef RefinementCallbackSphere<typename TDomain::position_attachment_type>	TRefProj;
	typename TDomain::position_type v;
	VecCopy(v, vector3(x, y, z), 0);
	return SmartPtr<TRefProj>(
			new TRefProj(*dom->grid(), dom->position_attachment(), v, radius));
}

///	Creates a refinement projector which projects new vertices onto a cylinder
/** Specify a domain, a point on the cylinder's axis (cx, cy, cz), the direction
 * of the axis (ax, ay, az) and the cylinder's radius.
 */
template <class TDomain>
SmartPtr<IRefinementCallback>
CylinderProjector(TDomain* dom, number cx, number cy, number cz,
				  number ax, number ay, number az, number radius)
{
	typedef RefinementCallbackCylinder<typename TDomain::position_attachment_type>	TRefProj;
	typename TDomain::position_type c, a;
	VecCopy(c, vector3(cx, cy, cz), 0);
	VecCopy(a, vector3(ax, ay, az), 0);
	return SmartPtr<TRefProj>(
			new TRefProj(*dom->grid(), dom->position_attachment(), c, a, radius));
}

template <class TDomain>
SmartPtr<IRefinementCallback>
SubdivisionLoopProjector(TDomain* dom)
{
	typedef RefinementCallbackSubdivisionLoop<typename TDomain::position_attachment_type>	TRefProj;
	return SmartPtr<TRefProj>(
			new TRefProj(*dom->grid(), dom->position_attachment(),
						 dom->position_attachment()));
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
namespace bridge{
namespace Refinement{

/// \addtogroup refinement_bridge
/// \{

/**
 * Class exporting the functionality. All functionality that is to
 * be used in scripts or visualization must be registered here.
 */
struct Functionality
{

/**
 * Function called for the registration of Domain and Algebra independent parts.
 * All Functions and Classes not depending on Domain and Algebra
 * are to be placed here when registering.
 *
 * @param reg				registry
 * @param parentGroup		group for sorting of functionality
 */
static void Common(Registry& reg, string grp)
{
//	register domain independent mark methods
	reg.add_function("MarkForRefinement_All", &MarkForRefinement_All, grp, "", "ref");
}

/**
 * Function called for the registration of Domain dependent parts.
 * All Functions and Classes depending on the Domain
 * are to be placed here when registering. The method is called for all
 * available Domain types, based on the current build options.
 *
 * @param reg				registry
 * @param parentGroup		group for sorting of functionality
 */
template <typename TDomain>
static void Domain(Registry& reg, string grp)
{
	typedef TDomain 							domain_type;
	typedef typename TDomain::position_type		pos_type;
	typedef typename TDomain::position_attachment_type apos_type;

	string suffix = GetDomainSuffix<TDomain>();
	string tag = GetDomainTag<TDomain>();

//	refiner factory-method registration
//	Note that the refiners themselfs have already been registered in lib_grid_bridge.
	reg.add_function("GlobalDomainRefiner",
					 &GlobalDomainRefiner<domain_type>, grp, "new GlobalDomainRefiner", "dom");
	reg.add_function("HangingNodeDomainRefiner",
					 &HangingNodeDomainRefiner<domain_type>, grp, "new HangingNodeDomainRefiner", "dom");
	reg.add_function("GlobalFracturedDomainRefiner",
					 &CreateGlobalFracturedDomainRefiner<domain_type>, grp, "new GlobalFracturedDomainRefiner", "dom");
	reg.add_function("AdaptiveRegularDomainRefiner",
					 &CreateAdaptiveRegularDomainRefiner<domain_type>, grp, "new AdaptiveRegularDomainRefiner", "dom");

//	register domain dependent mark methods
	reg.add_function("MarkForRefinement_VerticesInSphere",
				&MarkForRefinement_VerticesInSphere<domain_type>, grp,
				"", "dom#refiner#center#radius")
		.add_function("MarkForRefinement_EdgesInSphere",
				&MarkForRefinement_ElementsInSphere<domain_type, EdgeBase>, grp,
				"", "dom#refiner#center#radius")
		.add_function("MarkForRefinement_FacesInSphere",
				&MarkForRefinement_ElementsInSphere<domain_type, Face>, grp,
				"", "dom#refiner#center#radius")
		.add_function("MarkForRefinement_VolumesInSphere",
				&MarkForRefinement_ElementsInSphere<domain_type, Volume>, grp,
				"", "dom#refiner#center#radius")
		.add_function("MarkForRefinement_VerticesInCube",
				&MarkForRefinement_VerticesInCube<domain_type>, grp,
				"", "dom#refiner#min#max")
		.add_function("MarkForRefinement_AnisotropicElements",
				&MarkForRefinement_AnisotropicElements<domain_type>, grp,
				"", "dom#refiner#sizeRatio")
		.add_function("MarkForRefinement_AnisotropicElements2",
				&MarkForRefinement_AnisotropicElements2<domain_type>, grp,
				"", "dom#refiner#sizeRatio")
		.add_function("MarkForRefinement_ElementsByLuaCallback",
				&MarkForRefinement_ElementsByLuaCallback<domain_type>, grp,
				"", "dom#refiner#time#callbackName")
		.add_function("MarkForCoarsen_ElementsByLuaCallback",
				&MarkForCoarsen_ElementsByLuaCallback<domain_type>, grp,
				"", "dom#refiner#time#callbackName");

//	register refinement projection handler and factories
	{
		typedef RefinementProjectionHandler<apos_type> T;
		string name = string("RefinementProjectionHandler").append(suffix);
		reg.add_class_<T, IRefinementCallback>(name, grp)
				.add_method("set_default_callback", &T::set_default_callback, grp)
				.add_method("set_callback",
						static_cast<void (T::*)(int, SmartPtr<IRefinementCallback>) >
							(&T::set_callback), grp)
				.add_method("set_callback",
						static_cast<void (T::*)(std::string, SmartPtr<IRefinementCallback>) >
							(&T::set_callback), grp);
		reg.add_class_to_group(name, "RefinementProjectionHandler", tag);
	}

	reg.add_function("DomainRefinementProjectionHandler",
					&DomainRefinementProjectionHandler<TDomain>, grp,
					"RefinementProjectionHandler", "domain")
		.add_function("LinearProjector", &LinearProjector<TDomain>, grp,
					"IRefinementCallback", "domain")
		.add_function("SphereProjector", &SphereProjector<TDomain>, grp,
					"IRefinementCallback", "domain#centerX#centerY#centerZ#radius")
		.add_function("CylinderProjector", &CylinderProjector<TDomain>, grp,
					"IRefinementCallback", "domain#centerX#centerY#centerZ#axisX#axisY#axisZ#radius")
		.add_function("SubdivisionLoopProjector", &SubdivisionLoopProjector<TDomain>, grp,
					"IRefinementCallback", "domain");

}

}; // end Functionality

// end group refinement_bridge
/// \}

}// end Refinement

/// \addtogroup refinement_bridge
void RegisterBridge_Refinement(Registry& reg, string grp)
{
	grp.append("/Refinement");
	typedef Refinement::Functionality Functionality;

	try{
		RegisterCommon<Functionality>(reg,grp);
		RegisterDomainDependent<Functionality>(reg,grp);
	}
	UG_REGISTRY_CATCH_THROW(grp);
}

}// end of namespace bridge
}// end of namespace ug
