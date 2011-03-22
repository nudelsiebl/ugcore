// created by Sebastian Reiter
// y10 m12 d13
// s.b.reiter@googlemail.com

#ifndef __H__LIB_GRID__CALLBACK_DEFINITIONS__
#define __H__LIB_GRID__CALLBACK_DEFINITIONS__

#include <boost/function.hpp>
#include "lib_grid/lg_base.h"

namespace ug
{

/**
 * Callbacks that allow algorithms to query whether they should consider
 * an element in their computations.
 *
 * \defgroup lib_grid_algorithms_callbacks callbacks
 * \ingroup lib_grid_algorithms
 * \{
 */
 
////////////////////////////////////////////////////////////////////////
/**
 *\{
 *	\brief Callback definition associating a boolean with a geometric object
 *
 *	Allows to apply algorithms on arbitrary parts of a mesh.
 *	You may implement your own callbacks, which have to return true
 *	if the given geometric object should be considered in the algorithm.
 */
typedef boost::function<bool (VertexBase*)> CB_ConsiderVertex;
typedef boost::function<bool (EdgeBase*)>	CB_ConsiderEdge;
typedef boost::function<bool (Face*)>		CB_ConsiderFace;
typedef boost::function<bool (Volume*)>		CB_ConsiderVolume;
/** \} */

////////////////////////////////////////////////////////////////////////
/**
 *\{
 *	\brief A callback that returns true for all given objects.
 */
inline bool ConsiderAllVertices(VertexBase*)	{return true;}
inline bool ConsiderAllEdges(EdgeBase*)			{return true;}
inline bool ConsiderAllFaces(Face*)				{return true;}
inline bool ConsiderAllVolumes(Volume*)			{return true;}
/** \} */


////////////////////////////////////////////////////////////////////////
/**
 *\{
 *	\brief Callback definition for writing data associated with a
 *			geometric object to a binary stream.
 */
typedef boost::function<void (std::ostream&, VertexBase*)> CB_SerializeVertexData;
typedef boost::function<void (std::ostream&, EdgeBase*)> CB_SerializeEdgeData;
typedef boost::function<void (std::ostream&, Face*)> 	CB_SerializeFaceData;
typedef boost::function<void (std::ostream&, Volume*)>	CB_SerializeVolumeData;
///	\}

////////////////////////////////////////////////////////////////////////
/**
 *\{
 *	\brief Callback definition for reading data associated with a
 *			geometric object from a binary stream.
 */
typedef boost::function<void (std::istream&, VertexBase*)> CB_DeserializeVertexData;
typedef boost::function<void (std::istream&, EdgeBase*)> CB_DeserializeEdgeData;
typedef boost::function<void (std::istream&, Face*)> 	CB_DeserializeFaceData;
typedef boost::function<void (std::istream&, Volume*)>	CB_DeserializeVolumeData;
///	\}

/** \} */	// end of group definition

}//	end of namespace

#endif
