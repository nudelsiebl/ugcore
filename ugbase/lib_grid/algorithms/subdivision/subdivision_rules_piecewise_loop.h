//	created by Sebastian Reiter
//	s.b.reiter@googlemail.com
//	y10 m10 d9

#ifndef __H__UG__SUBDIVISION_RULES_PIECEWISE_LOOP__
#define __H__UG__SUBDIVISION_RULES_PIECEWISE_LOOP__

#include <vector>
#include <cassert>
#include "lib_grid/lg_base.h"

namespace ug
{

///	\addtogroup lib_grid_algorithms_refinement_subdivision
///	@{

///	A singleton that stores all rules for a piecewise-loop subdivision surface.
class SubdivRules_PLoop
{
	public:
		struct NeighborInfo{
			NeighborInfo()	{}
			NeighborInfo(VertexBase* n, size_t cval) :
				nbr(n), creaseValence(cval)	{}
				
			VertexBase* nbr;
		/**	0 means that the neighbor is not a crease vertex.
		 *	> 0: The valence of the crease regarding only the
		 *	part on the side of the center-vertex.*/
			size_t creaseValence;
		};
		
	public:
	///	returns the only instance to this singleton.
		static SubdivRules_PLoop& inst()
		{
			static SubdivRules_PLoop subdivRules;
			return subdivRules;
		}

	////////////////////////////////
	//	WEIGHTS
		number ref_even_inner_center_weight(size_t valence)
			{return 1. - (number)valence * get_beta(valence);}
		
		number ref_even_inner_nbr_weight(size_t valence)
			{return get_beta(valence);}
		
	///	returns weights for center, nbr1 and nbr2.
		vector3 ref_even_crease_weights()
			{return vector3(0.75, 0.125, 0.125);}
			
		vector4 ref_odd_inner_weights()
			{return vector4(0.375, 0.375, 0.125, 0.125);}
		
	///	weights of an odd vertex on an inner edge that is connected to a crease.
	/**	The weight for the vertex on the crease is in v.x(), the inner edge vertex
	 *	in v.y() and the two indirectly connected vertex weights are in v.z() and v.w.
	 *	creaseValence specifies the number of associated edges of the crease vertex.
	 *
	 *	Rules are taken from:
	 *	"Piecewise Smooth Subdivision Surfaces with Normal Control",
	 *	H. Biermann, Adi Levin, Denis Zorin.*/
		vector4 ref_odd_inner_weights(size_t creaseValence)
		{
			assert(creaseValence > 2 && "Bad crease valence. Underlying grid is not a surface grid.");
			if(creaseValence == 4)
				return ref_odd_inner_weights();
			number gamma = 0.5 - 0.25 * cos(PI / (number)(creaseValence - 1));
			return vector4(0.75 - gamma, gamma, 0.125, 0.125);
			
			//number c = 0.25 * cos((2.*PI) / (number)(creaseValence - 1));
			//return vector4(0.5 - c, 0.25 + c, 0.125, 0.125);
		}
		
		vector2 ref_odd_crease_weights()
			{return vector2(0.5, 0.5);}

		number proj_inner_center_weight(size_t valence)
			{return 1.0 - (number)valence / (0.375 / get_beta(valence) + valence);}
		
		number proj_inner_nbr_weight(size_t valence)
			{return 1.0 / (0.375 / get_beta(valence) + valence);}
			
		vector3 proj_crease_weights()
			{return vector3(2./3., 1./6., 1./6.);}

	/**	nbrInfos have to be specified in clockwise or counter-clockwise order.*/
		void proj_inner_crease_nbr_weights(number& centerWgtOut, number* nbrWgtsOut,
										   NeighborInfo* nbrInfos, size_t numNbrs)
		{
			number wcntrProj = proj_inner_center_weight(numNbrs);
			number wnbrProj = proj_inner_nbr_weight(numNbrs);
			
		//	initialize all weights with 0
			centerWgtOut = 0;
			for(size_t i = 0; i < numNbrs; ++i)
				nbrWgtsOut[i] = 0;
				
			for(size_t i = 0; i < numNbrs; ++i){
				NeighborInfo& nbrInfo = nbrInfos[i];
				vector4 oddWeights(0.5, 0.5, 0, 0);
				
				if(nbrInfo.creaseValence == 0){
				//	Calculate the weights for odd-refinement on the given edge
					oddWeights = ref_odd_inner_weights();
				}
				else{
					oddWeights = ref_odd_inner_weights(nbrInfo.creaseValence);
				}
				
				nbrWgtsOut[i] += oddWeights.x() * wnbrProj;
				centerWgtOut += oddWeights.y() * wnbrProj;
				nbrWgtsOut[next_ind(i, numNbrs)] += oddWeights.z() * wnbrProj;
				nbrWgtsOut[prev_ind(i, numNbrs)] += oddWeights.w() * wnbrProj;
			}
			
			
		//	add scaled weights for evem refinement mask
			centerWgtOut += wcntrProj * ref_even_inner_center_weight(numNbrs);
			for(size_t i = 0; i < numNbrs; ++i)
				nbrWgtsOut[i] += wcntrProj * ref_even_inner_nbr_weight(numNbrs);
		}
/*			
		number proj_inner_crease_nbr_center_weight(size_t valence);
		number proj_inner_crease_nbr_nbr_weight(size_t valence);
		number proj_inner_crease_nbr_nbr_weight(size_t valence, size_t cValence);
*/
	///	returns beta as it is used in the subdivision masks.
	/**	performs a lookup if the valency is small enough.
	 *	calculates a fresh beta else.*/
		number get_beta(size_t valency);
/*
	////////////////////////////////
	//	EVEN MASKS
		template <class TAAPos>
		typename TAAPos::ValueType
		apply_even_mask(Grid& grid, VertexBase* center,
						TAAPos& aaPos);

		template <class TAAPos>
		typename TAAPos::ValueType
		apply_even_crease_mask(VertexBase* center, VertexBase* nbr1,
							   VertexBase* nbr2, TAAPos& aaPos);

	////////////////////////////////
	//	ODD MASKS
		template <class TAAPos>
		typename TAAPos::ValueType
		apply_odd_mask(VertexBase* vrt, EdgeBase* parent,
					   TAAPos& aaPos);

		template <class TAAPos>
		typename TAAPos::ValueType
		apply_odd_crease_mask(VertexBase* vrt, EdgeBase* parent,
							  TAAPos& aaPos);

		template <class TAAPos>
		typename TAAPos::ValueType
		apply_odd_crease_nbr_mask(Grid& grid, VertexBase* vrt,
								  EdgeBase* parent, TAAPos& aaPos);

	////////////////////////////////
	//	PROJECTION
		template <class TAAPos>
		typename TAAPos::ValueType
		project_inner_vertex(Grid& grid, VertexBase* vrt,
							 TAAPos& aaPos);

		template <class TAAPos>
		typename TAAPos::ValueType
		project_inner_vertex(VertexBase* vrt, VertexBase* nbrs,
							 int* nbrCreaseValencies, int numNbrs,
							 TAAPos& aaPos);

		template <class TAAPos>
		typename TAAPos::ValueType
		project_crease_vertex(VertexBase* vrt, VertexBase* nbr1,
							  VertexBase* nbr2, TAAPos& aaPos);
*/
	private:
	///	calculates beta as it is used in the subdivision masks.
		number calculate_beta(size_t valency);
		
	private:
	///	private constructor prohibits multiple instantiation.
		SubdivRules_PLoop();
		
	///	private copy constructor prohibits copy-construction.
		SubdivRules_PLoop(const SubdivRules_PLoop& src);
		
	///	private assignment operator prohibits assignment.
		SubdivRules_PLoop& operator=(const SubdivRules_PLoop& src);
		
	///	returns the next index in a cyclic index set
		inline size_t next_ind(size_t ind, size_t numInds)	{return (ind + 1) % numInds;}
	///	returns the previous index in a cyclic index set
		inline size_t prev_ind(size_t ind, size_t numInds)	{return (ind + numInds - 1) % numInds;}
		
		
	private:
		std::vector<number>	m_betas;//< precalculated betas.
};

/// @}	//	end of add_to_group

}//	end of namespace

#endif
