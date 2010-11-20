//	created by Sebastian Reiter
//	s.b.reiter@googlemail.com
//	y10 m06 d16

#ifndef __H__LIB_GRID__FILE_IO_UGX_IMPL__
#define __H__LIB_GRID__FILE_IO_UGX_IMPL__

#include <sstream>
#include <cstring>

namespace ug
{

////////////////////////////////////////////////////////////////////////
template <class TAPosition>
bool SaveGridToUGX(Grid& grid, ISubsetHandler& sh, const char* filename,
				   TAPosition& aPos)
{
	GridWriterUGX ugxWriter;
	ugxWriter.add_grid(grid, "defGrid", aPos);
	ugxWriter.add_subset_handler(sh, "defSH", 0);
	return ugxWriter.write_to_file(filename);
};

////////////////////////////////////////////////////////////////////////
template <class TAPosition>
bool LoadGridFromUGX(Grid& grid, ISubsetHandler& sh, const char* filename,
					 TAPosition& aPos)
{
	GridReaderUGX ugxReader;
	if(!ugxReader.parse_file(filename)){
		UG_LOG("ERROR in LoadGridFromUGX: File not found: " << filename << std::endl);
		return false;
	}

	if(ugxReader.num_grids() < 1){
		UG_LOG("ERROR in LoadGridFromUGX: File contains no grid.\n");
		return false;
	}

	ugxReader.get_grid(grid, 0, aPos);

	if(ugxReader.num_subset_handlers(0) > 0)
		ugxReader.get_subset_handler(sh, 0, 0);

	return true;
}


////////////////////////////////////////////////////////////////////////
template <class TPositionAttachment>
bool GridWriterUGX::
add_grid(Grid& grid, const char* name,
		 TPositionAttachment& aPos)
{
	using namespace rapidxml;
	using namespace std;
//	access node data
	if(!grid.has_vertex_attachment(aPos)){
		UG_LOG("  position attachment missing in grid " << name << endl);
		return false;
	}

	Grid::VertexAttachmentAccessor<TPositionAttachment> aaPos(grid, aPos);

//	create a new grid-node
	xml_node<>* gridNode = m_doc.allocate_node(node_element, "grid");
	gridNode->append_attribute(m_doc.allocate_attribute("name", name));

//	store the grid and the node in an entry
	m_vEntries.push_back(Entry(&grid, gridNode));

//	append it to the document
	m_doc.append_node(gridNode);

//	initialize the grids attachments
	init_grid_attachments(grid);
	
//	access indices
	Grid::EdgeAttachmentAccessor<AInt> aaIndEDGE(grid, m_aInt);
	Grid::FaceAttachmentAccessor<AInt> aaIndFACE(grid, m_aInt);
	
//	write vertices
	if(grid.num<Vertex>() > 0)
		gridNode->append_node(create_vertex_node(grid.begin<Vertex>(),
										grid.end<Vertex>(), aaPos));

//	write constrained vertices
	if(grid.num<HangingVertex>() > 0)
		gridNode->append_node(create_constrained_vertex_node(
										grid.begin<HangingVertex>(),
										grid.end<HangingVertex>(),
										aaPos, aaIndEDGE, aaIndFACE));

//	add the remaining grid elements to the nodes
	add_elements_to_node(gridNode, grid);

	return true;
}

template <class TPositionAttachment>
void GridWriterUGX::
add_grid(MultiGrid& mg, const char* name,
		 TPositionAttachment& aPos)
{
}

template <class TAttachment>
void GridWriterUGX::
add_vertex_attachment(const TAttachment& attachment,
						const char* name,
						size_t refGridIndex)
{
}

template <class TAAPos>
rapidxml::xml_node<>*
GridWriterUGX::
create_vertex_node(VertexIterator vrtsBegin,
				  VertexIterator vrtsEnd,
				  TAAPos& aaPos)
{
	using namespace rapidxml;
	using namespace std;
//	the number of coordinates
	const int numCoords = (int)TAAPos::ValueType::Size;

//	write the vertices to a temporary stream
	stringstream ss;
	for(VertexIterator iter = vrtsBegin; iter != vrtsEnd; ++iter)
	{
		for(int i = 0; i < numCoords; ++i)
			ss << aaPos[*iter][i] << " ";
	}

//	create the node
	xml_node<>* node = NULL;

	if(ss.str().size() > 0){
	//	allocate a string and erase last character(' ')
		char* nodeData = m_doc.allocate_string(ss.str().c_str(), ss.str().size());
		nodeData[ss.str().size()-1] = 0;
	//	create a node with some data
		node = m_doc.allocate_node(node_element, "vertices", nodeData);
	}
	else{
	//	create an emtpy node
		node = m_doc.allocate_node(node_element, "vertices");
	}

	char* buff = m_doc.allocate_string(NULL, 10);
	sprintf(buff, "%d", numCoords);
	node->append_attribute(m_doc.allocate_attribute("coords", buff));

//	return the node
	return node;
}

template <class TAAPos>
rapidxml::xml_node<>*
GridWriterUGX::
create_constrained_vertex_node(HangingVertexIterator vrtsBegin,
								HangingVertexIterator vrtsEnd,
								TAAPos& aaPos,
								AAEdgeIndex aaIndEDGE,
							 	AAFaceIndex aaIndFACE)
{
	using namespace rapidxml;
	using namespace std;
//	the number of coordinates
	const int numCoords = (int)TAAPos::ValueType::Size;

//	write the vertices to a temporary stream
	stringstream ss;
	for(HangingVertexIterator iter = vrtsBegin; iter != vrtsEnd; ++iter)
	{
		for(int i = 0; i < numCoords; ++i)
			ss << aaPos[*iter][i] << " ";
			
	//	write index and local coordinate of associated constraining element
	//	codes:	-1: no constraining element
	//			0: vertex. index follows (not yet supported)
	//			1: edge. index and 1 local coordinate follow.
	//			2: face. index and 2 local coordinates follow.
	//			3: volume. index and 3 local coordinates follow. (not yet supported)
		EdgeBase* ce = dynamic_cast<EdgeBase*>((*iter)->get_parent());
		Face* cf = dynamic_cast<Face*>((*iter)->get_parent());
		if(ce)
			ss << "1 " << aaIndEDGE[ce] << " " << (*iter)->get_local_coordinate_1() << " ";
		else if(cf)
			ss << "2 " << aaIndFACE[cf] << " " << (*iter)->get_local_coordinate_1()
			   << " " << (*iter)->get_local_coordinate_2() << " ";
		else
			ss << "-1 ";
	}

//	create the node
	xml_node<>* node = NULL;

	if(ss.str().size() > 0){
	//	allocate a string and erase last character(' ')
		char* nodeData = m_doc.allocate_string(ss.str().c_str(), ss.str().size());
		nodeData[ss.str().size()-1] = 0;
	//	create a node with some data
		node = m_doc.allocate_node(node_element, "constrained_vertices", nodeData);
	}
	else{
	//	create an emtpy node
		node = m_doc.allocate_node(node_element, "constrained_vertices");
	}

	char* buff = m_doc.allocate_string(NULL, 10);
	sprintf(buff, "%d", numCoords);
	node->append_attribute(m_doc.allocate_attribute("coords", buff));

//	return the node
	return node;
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//	implementation of GridReaderUGX
template <class TPositionAttachment>
bool GridReaderUGX::
get_grid(Grid& gridOut, size_t index,
		  TPositionAttachment& aPos)
{
	using namespace rapidxml;
	using namespace std;

//	make sure that a node at the given index exists
	if(num_grids() <= index){
		UG_LOG("  GridReaderUGX::read: bad grid index!\n");
		return false;
	}

	Grid& grid = gridOut;

//	Since we have to create all elements in the correct order and
//	since we have to make sure that no elements are created in between,
//	we'll first disable all grid-options and reenable them later on
	uint gridopts = grid.get_options();
	grid.set_options(GRIDOPT_NONE);

//	access node data
	if(!grid.has_vertex_attachment(aPos)){
		grid.attach_to_vertices(aPos);
	}

	Grid::VertexAttachmentAccessor<TPositionAttachment> aaPos(grid, aPos);

//	store the grid in the grid-vector and assign indices to the vertices
	m_entries[index].grid = &grid;

//	get the grid-node and the vertex-vector
	xml_node<>* gridNode = m_entries[index].node;
	vector<VertexBase*>& vertices = m_entries[index].vertices;
	vector<EdgeBase*>& edges = m_entries[index].edges;
	vector<Face*>& faces = m_entries[index].faces;
	vector<Volume*>& volumes = m_entries[index].volumes;

//	we'll record constraining objects for constrained-vertices and constrained-edges
	std::vector<std::pair<int, int> > constrainingObjsVRT;
	std::vector<std::pair<int, int> > constrainingObjsEDGE;
	std::vector<std::pair<int, int> > constrainingObjsTRI;
	std::vector<std::pair<int, int> > constrainingObjsQUAD;
	
//	iterate through the nodes in the grid and create the entries
	xml_node<>* curNode = gridNode->first_node();
	for(;curNode; curNode = curNode->next_sibling()){
		bool bSuccess = true;
		const char* name = curNode->name();
		if(strcmp(name, "vertices") == 0)
			bSuccess = create_vertices(vertices, grid, curNode, aaPos);
		else if(strcmp(name, "constrained_vertices") == 0)
			bSuccess = create_constrained_vertices(vertices, constrainingObjsVRT,
												   grid, curNode, aaPos);
		else if(strcmp(name, "edges") == 0)
			bSuccess = create_edges(edges, grid, curNode, vertices);
		else if(strcmp(name, "constraining_edges") == 0)
			bSuccess = create_constraining_edges(edges, grid, curNode, vertices);
		else if(strcmp(name, "constrained_edges") == 0)
			bSuccess = create_constrained_edges(edges, constrainingObjsEDGE,
												grid, curNode, vertices);
		else if(strcmp(name, "triangles") == 0)
			bSuccess = create_triangles(faces, grid, curNode, vertices);
		else if(strcmp(name, "constraining_triangles") == 0)
			bSuccess = create_constraining_triangles(faces, grid, curNode, vertices);
		else if(strcmp(name, "constrained_triangles") == 0)
			bSuccess = create_constrained_triangles(faces, constrainingObjsTRI,
												grid, curNode, vertices);
		else if(strcmp(name, "quadrilaterals") == 0)
			bSuccess = create_quadrilaterals(faces, grid, curNode, vertices);
		else if(strcmp(name, "constraining_quadrilaterals") == 0)
			bSuccess = create_constraining_quadrilaterals(faces, grid, curNode, vertices);
		else if(strcmp(name, "constrained_quadrilaterals") == 0)
			bSuccess = create_constrained_quadrilaterals(faces, constrainingObjsQUAD,
												grid, curNode, vertices);
		else if(strcmp(name, "tetrahedrons") == 0)
			bSuccess = create_tetrahedrons(volumes, grid, curNode, vertices);
		else if(strcmp(name, "hexahedrons") == 0)
			bSuccess = create_hexahedrons(volumes, grid, curNode, vertices);
		else if(strcmp(name, "prisms") == 0)
			bSuccess = create_prisms(volumes, grid, curNode, vertices);
		else if(strcmp(name, "pyramids") == 0)
			bSuccess = create_pyramids(volumes, grid, curNode, vertices);

		if(!bSuccess){
			grid.set_options(gridopts);
			return false;
		}
	}
	
//	resolve constrained object relations
	if(!constrainingObjsVRT.empty()){
		//UG_LOG("num-edges: " << edges.size() << std::endl);
	//	iterate over the pairs.
	//	at the same time we'll iterate over the constrained vertices since
	//	they are synchronized.
		HangingVertexIterator hvIter = grid.begin<HangingVertex>();
		for(std::vector<std::pair<int, int> >::iterator iter = constrainingObjsVRT.begin();
			iter != constrainingObjsVRT.end(); ++iter, ++hvIter)
		{
			HangingVertex* hv = *hvIter;
			
			switch(iter->first){
				case 1:	// constraining object is an edge
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)edges.size()){
					//	get the edge
						ConstrainingEdge* edge = dynamic_cast<ConstrainingEdge*>(edges[iter->second]);
						if(edge){
							hv->set_parent(edge);
							edge->add_constrained_object(hv);
						}
						else{
							UG_LOG("WARNING: Type-ID / type mismatch. Ignoring edge " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad edge index in constrained vertex: " << iter->second << "\n");
					}
				}break;
				
				case 2:	// constraining object is an face
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)faces.size()){
					//	get the edge
						ConstrainingFace* face = dynamic_cast<ConstrainingFace*>(faces[iter->second]);
						if(face){
							hv->set_parent(face);
							face->add_constrained_object(hv);
						}
						else{
							UG_LOG("WARNING in GridReaderUGX: Type-ID / type mismatch. Ignoring face " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad face index in constrained vertex: " << iter->second << "\n");
					}
				}break;
				
				default:
				{
					UG_LOG("WARNING in GridReaderUGX: unsupported type-id of constraining element\n");
				}
			}
		}
	}

	if(!constrainingObjsEDGE.empty()){
	//	iterate over the pairs.
	//	at the same time we'll iterate over the constrained vertices since
	//	they are synchronized.
		ConstrainedEdgeIterator ceIter = grid.begin<ConstrainedEdge>();
		for(std::vector<std::pair<int, int> >::iterator iter = constrainingObjsEDGE.begin();
			iter != constrainingObjsEDGE.end(); ++iter, ++ceIter)
		{
			ConstrainedEdge* ce = *ceIter;
			
			switch(iter->first){
				case 1:	// constraining object is an edge
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)edges.size()){
					//	get the edge
						ConstrainingEdge* edge = dynamic_cast<ConstrainingEdge*>(edges[iter->second]);
						if(edge){
							ce->set_constraining_object(edge);
							edge->add_constrained_object(ce);
						}
						else{
							UG_LOG("WARNING in GridReaderUGX: Type-ID / type mismatch. Ignoring edge " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad edge index in constrained edge.\n");
					}
				}break;
				case 2:	// constraining object is an face
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)faces.size()){
					//	get the edge
						ConstrainingFace* face = dynamic_cast<ConstrainingFace*>(faces[iter->second]);
						if(face){
							ce->set_constraining_object(face);
							face->add_constrained_object(ce);
						}
						else{
							UG_LOG("WARNING in GridReaderUGX: Type-ID / type mismatch. Ignoring face " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad face index in constrained edge: " << iter->second << "\n");
					}
				}break;
				
				default:
				{
					UG_LOG("WARNING in GridReaderUGX: unsupported type-id of constraining element\n");
				}
			}
		}
	}

	if(!constrainingObjsTRI.empty()){
	//	iterate over the pairs.
	//	at the same time we'll iterate over the constrained vertices since
	//	they are synchronized.
		ConstrainedTriangleIterator cfIter = grid.begin<ConstrainedTriangle>();
		for(std::vector<std::pair<int, int> >::iterator iter = constrainingObjsTRI.begin();
			iter != constrainingObjsTRI.end(); ++iter, ++cfIter)
		{
			ConstrainedFace* cdf = *cfIter;
			
			switch(iter->first){
				case 2:	// constraining object is an face
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)faces.size()){
					//	get the edge
						ConstrainingFace* face = dynamic_cast<ConstrainingFace*>(faces[iter->second]);
						if(face){
							cdf->set_constraining_object(face);
							face->add_constrained_object(cdf);
						}
						else{
							UG_LOG("WARNING in GridReaderUGX: Type-ID / type mismatch. Ignoring face " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad face index in constrained face: " << iter->second << "\n");
					}
				}break;
				
				default:
				{
					UG_LOG("WARNING in GridReaderUGX: unsupported type-id of constraining element\n");
				}
			}
		}
	}

	if(!constrainingObjsQUAD.empty()){
	//	iterate over the pairs.
	//	at the same time we'll iterate over the constrained vertices since
	//	they are synchronized.
		ConstrainedQuadrilateralIterator cfIter = grid.begin<ConstrainedQuadrilateral>();
		for(std::vector<std::pair<int, int> >::iterator iter = constrainingObjsQUAD.begin();
			iter != constrainingObjsQUAD.end(); ++iter, ++cfIter)
		{
			ConstrainedFace* cdf = *cfIter;
			
			switch(iter->first){
				case 2:	// constraining object is an face
				{
				//	make sure that the index is valid
					if(iter->second >= 0 && iter->second < (int)faces.size()){
					//	get the edge
						ConstrainingFace* face = dynamic_cast<ConstrainingFace*>(faces[iter->second]);
						if(face){
							cdf->set_constraining_object(face);
							face->add_constrained_object(cdf);
						}
						else{
							UG_LOG("WARNING in GridReaderUGX: Type-ID / type mismatch. Ignoring face " << iter->second << ".\n");
						}
					}
					else{
						UG_LOG("ERROR in GridReaderUGX: Bad face index in constrained face: " << iter->second << "\n");
					}
				}break;
				
				default:
				{
					UG_LOG("WARNING in GridReaderUGX: unsupported type-id of constraining element\n");
				}
			}
		}
	}

//	reenable the grids options.
	grid.set_options(gridopts);

	return true;
}

template <class TAAPos>
bool GridReaderUGX::
create_vertices(std::vector<VertexBase*>& vrtsOut, Grid& grid,
				rapidxml::xml_node<>* vrtNode, TAAPos aaPos)
{
	using namespace rapidxml;
	using namespace std;

	int numSrcCoords = -1;
	xml_attribute<>* attrib = vrtNode->first_attribute("coords");
	if(attrib)
		numSrcCoords = atoi(attrib->value());

	int numDestCoords = (int)TAAPos::ValueType::Size;

	assert(numDestCoords > 0 && "bad position attachment type");

	if(numSrcCoords < 1 || numDestCoords < 1)
		return false;

//	create a buffer with which we can access the data
	string str(vrtNode->value(), vrtNode->value_size());
	stringstream ss(str, ios_base::in);

//	if numDestCoords == numSrcCoords parsing will be faster
	if(numSrcCoords == numDestCoords){
		while(!ss.eof()){
		//	read the data
			typename TAAPos::ValueType v;

			for(int i = 0; i < numSrcCoords; ++i)
				ss >> v[i];

		//	make sure that everything went right
			if(ss.fail())
				break;

		//	create a new vertex
			Vertex* vrt = *grid.create<Vertex>();
			vrtsOut.push_back(vrt);

		//	set the coordinates
			aaPos[vrt] = v;
		}
	}
	else{
	//	we have to be careful with reading.
	//	if numDestCoords < numSrcCoords we'll ignore some coords,
	//	in the other case we'll add some 0's.
		int minNumCoords = min(numSrcCoords, numDestCoords);
		typename TAAPos::ValueType::value_type dummy = 0;

		while(!ss.eof()){
		//	read the data
			typename TAAPos::ValueType v;

			int iMin;
			for(iMin = 0; iMin < minNumCoords; ++iMin)
				ss >> v[iMin];

		//	ignore unused entries in the input buffer
			for(int i = iMin; i < numSrcCoords; ++i)
				ss >> dummy;

		//	add 0's to the vector
			for(int i = iMin; i < numDestCoords; ++i)
				v[i] = 0;

		//	make sure that everything went right
			if(ss.fail())
				break;

		//	create a new vertex
			Vertex* vrt = *grid.create<Vertex>();
			vrtsOut.push_back(vrt);

		//	set the coordinates
			aaPos[vrt] = v;
		}
	}

	return true;
}

template <class TAAPos>
bool GridReaderUGX::
create_constrained_vertices(std::vector<VertexBase*>& vrtsOut,
							std::vector<std::pair<int, int> >& constrainingObjsOut,
							Grid& grid, rapidxml::xml_node<>* vrtNode, TAAPos aaPos)
{
	using namespace rapidxml;
	using namespace std;

	int numSrcCoords = -1;
	xml_attribute<>* attrib = vrtNode->first_attribute("coords");
	if(attrib)
		numSrcCoords = atoi(attrib->value());

	int numDestCoords = (int)TAAPos::ValueType::Size;

	assert(numDestCoords > 0 && "bad position attachment type");

	if(numSrcCoords < 1 || numDestCoords < 1)
		return false;

//	create a buffer with which we can access the data
	string str(vrtNode->value(), vrtNode->value_size());
	stringstream ss(str, ios_base::in);

//	we have to be careful with reading.
//	if numDestCoords < numSrcCoords we'll ignore some coords,
//	in the other case we'll add some 0's.
	int minNumCoords = min(numSrcCoords, numDestCoords);
	typename TAAPos::ValueType::value_type dummy = 0;

//todo: speed could be improved, if dest and src position types have the same dimension
	while(!ss.eof()){
	//	read the data
		typename TAAPos::ValueType v;

		int iMin;
		for(iMin = 0; iMin < minNumCoords; ++iMin)
			ss >> v[iMin];

	//	ignore unused entries in the input buffer
		for(int i = iMin; i < numSrcCoords; ++i)
			ss >> dummy;

	//	add 0's to the vector
		for(int i = iMin; i < numDestCoords; ++i)
			v[i] = 0;

	//	now read the type of the constraining object and its index
		int conObjType, conObjIndex;
		ss >> conObjType;
		
		if(conObjType != -1)
			ss >> conObjIndex;

	//	depending on the constrainings object type, we'll read local coordinates
		vector3 localCoords(0, 0, 0);
		switch(conObjType){
			case 1:	// an edge. read one local coord
				ss >> localCoords.x;
				break;
			case 2: // a face. read two local coords
				ss >> localCoords.x >> localCoords.y;
				break;
			default:
				break;
		}
				
	//	make sure that everything went right
		if(ss.fail())
			break;

	//	create a new vertex
		HangingVertex* vrt = *grid.create<HangingVertex>();
		vrtsOut.push_back(vrt);

	//	set the coordinates
		aaPos[vrt] = v;
		
	//	set local coordinates
		vrt->set_local_coordinates(localCoords.x, localCoords.y);
		
	//	add the constraining object id and index to the list
		constrainingObjsOut.push_back(std::make_pair(conObjType, conObjIndex));
	}

	return true;
}

}//	end of namespace

#endif
