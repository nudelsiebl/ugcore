// created by Sebastian Reiter
// s.b.reiter@gmail.com
// march 2014

#ifndef __H__UG_NORMAL_CALCULATION__
#define __H__UG_NORMAL_CALCULATION__

namespace ug{

/**
 * \brief	contains methods for the calculation of normals of
 * 			edges, faces and volume elements.
 *
 * \defgroup lib_grid_algorithms_normal_calculation normal_calculation
 * \ingroup lib_grid_algorithms
 * \{
 */

///	Calculates the outer normal of the i-th side of the given grid object
/**	\{ */
template <class TAAPos>
inline typename TAAPos::ValueType
CalculateOuterNormal(Vertex* v, int sideIndex, TAAPos aaPos);

template <class TAAPos>
inline typename TAAPos::ValueType
CalculateOuterNormal(Edge* v, int sideIndex, TAAPos aaPos);

template <class TAAPos>
inline typename TAAPos::ValueType
CalculateOuterNormal(Face* v, int sideIndex, TAAPos aaPos);

template <class TAAPos>
inline typename TAAPos::ValueType
CalculateOuterNormal(Volume* v, int sideIndex, TAAPos aaPos);

template <class TAAPos>
inline typename TAAPos::ValueType
CalculateOuterNormal(GridObject* v, int sideIndex, TAAPos aaPos);

/** \} */

/** \} */	//	end of docu-group normal_calculation
}//	end of namespace


////////////////////////////////////////
//	include implementation
#include "normal_calculation_impl.h"

#endif