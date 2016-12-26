#include "inmost.h"
#include "incident_matrix.hpp"
#if defined(USE_MESH)

//TODO:
// incident_matrix class should measure for minimal volume,
// possibly check and update from projects/OctreeCutcell/octgrid.cpp

namespace INMOST
{

	
	void Face::SwapCells() const
	{
		Mesh * m = GetMeshLink();
		MarkerType hm = m->HideMarker();
		adj_type & hc = m->HighConn(GetHandle());
		if( m->Count(hc.data(),static_cast<enumerator>(hc.size()),hm) == 2 )
		{
			enumerator k1 = ENUMUNDEF, k2;
			k1 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
			k2 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
			HandleType temp = hc[k1];
			hc[k1] = hc[k2];
			hc[k2] = temp;
			//FixNormalOrientation(); //maybe should change orientation?
		}
	}

	void Edge::SwapEnds()
	{
		Mesh * m = GetMeshLink();
		MarkerType hm = m->HideMarker();
		adj_type & lc = m->LowConn(GetHandle());
		if( m->Count(lc.data(),static_cast<enumerator>(lc.size()),hm) == 2 )
		{
			enumerator k1 = ENUMUNDEF, k2;
			k1 = m->getNext(lc.data(),static_cast<enumerator>(lc.size()),k1,hm);
			k2 = m->getNext(lc.data(),static_cast<enumerator>(lc.size()),k1,hm);
			//std::cout << "k " << k1 << " " << k2 << std::endl;
			//std::cout << "was " << lc[k1] << " " << lc[k2] << std::endl;
			HandleType temp = lc[k1];
			lc[k1] = lc[k2];
			lc[k2] = temp;
			//std::cout << "now " << lc[k1] << " " << lc[k2] << std::endl;
		}
	}
	
	void Element::Disconnect(bool del_upper) const
	{
		
		dynarray<adj_type::size_type,64> del;
		Mesh * m = GetMeshLink();
		//BEGIN NEW CODE - CHECK
		//Reorder face edges, so that they always apear in right direction
		if( GetElementType() == CELL ) //This is a cell
		{
			getAsCell()->SwapBackCell();
			/*
			MarkerType hm = m->HideMarker();
			adj_type & lc = m->LowConn(GetHandle());
			for( adj_type::size_type it = 0; it < lc.size(); it++) //if( !m->GetMarker(lc[it],hm) ) //iterator over unhidden faces
			{
				adj_type & hc = m->HighConn(lc[it]);
				if( !hc.empty() && m->Count(hc.data(),static_cast<enumerator>(hc.size()),hm) == 2 ) //there are two cells connected to face
				{
					enumerator k1 = ENUMUNDEF, k2;
					k1 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
					if( hc[k1] == GetHandle() ) //the first cell is current
					{
						k2 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
						hc[k1] = hc[k2];
						//hc[k2] = GetHandle(); //cannot use the cell because virtualization table is already destroyed and FixNormalOrientation will do bad things
						hc.resize(1); //just remove element, we will do this anyway later
						Face(m,lc[it])->FixNormalOrientation(); //restore orientation
					}
				}
			}
			*/
		}
		//END NEW CODE - CHECK
		adj_type & hc = m->HighConn(GetHandle());
		adj_type::size_type i = 0;
		for( adj_type::size_type it = 0; it < hc.size(); it++)
		{
			int flag = 0;
			adj_type & ilc = m->LowConn(hc[it]);
			adj_type::iterator jt = ilc.begin();
			while (jt != ilc.end())
			{
				if(*jt == GetHandle())
				{
					flag = 1;
					jt = ilc.erase(jt);
				}
				else ++jt;
			}
			if( flag ) del.push_back(i);
			i++;
		}
		if( del_upper )
		{
			if( GetElementType() < CELL ) 
			{
				if( Hidden() ) 
				{
					for(dynarray<adj_type::size_type,64>::iterator it = del.begin(); it != del.end(); it++)
						if( m->GetMarker(hc[*it],m->HideMarker()) ) m->Delete(hc[*it]);
				}
				else 
				{
					for(dynarray<adj_type::size_type,64>::iterator it = del.begin(); it != del.end(); it++)
						m->Delete(hc[*it]);
				}
			}
		}
		hc.clear();
		adj_type & lc = m->LowConn(GetHandle());
		for(adj_type::size_type it = 0; it < lc.size(); it++)
		{
			adj_type & ihc = m->HighConn(lc[it]);
			adj_type::iterator jt = ihc.begin();
			while (jt != ihc.end())
			{
				if(*jt == GetHandle()) jt = ihc.erase(jt);
				else ++jt;
			}
		}
		lc.clear();
	}
	
	Cell Cell::UniteCells(ElementArray<Cell> & unite, MarkerType del_protect)
	{
		Mesh * m = unite.GetMeshLink();
		if( unite.empty() ) return Cell(m,InvalidHandle());
		tiny_map<HandleType, int, 64> face_visit; // we check all edges of faces, inner edges are visited several times, outer - once
		ElementArray<Face> faces(m); 
		dynarray<HandleType,64> inner_faces;
		bool doexit = false;
		MarkerType hm = m->HideMarker();
		for(ElementArray<Cell>::size_type j = 0; j < unite.size(); j++)
		{
			if( unite[j]->GetMarker(del_protect) ) doexit = true; 
			adj_type const & lc = m->LowConn(unite.at(j));
			for(adj_type::size_type it = 0; it < lc.size(); it++) if( !m->GetMarker(lc[it],hm) )
				face_visit[lc[it]]++;
		}
		if( doexit ) return Cell(m,InvalidHandle());
		MarkerType visited = m->CreateMarker(), rem = m->CreateMarker();
		dynarray<HandleType,64> edges;
		dynarray<HandleType,64> nodes;
		for(tiny_map<HandleType,int,64>::iterator it = face_visit.begin(); it != face_visit.end(); it++)
		{
			if( it->second == 1 )
				faces.push_back(it->first);
			else
			{
				if( m->GetMarker(it->first,del_protect) ) doexit = true;
				m->SetMarker(it->first,rem);
				adj_type const & lc = m->LowConn(it->first);
				for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
					if( !m->GetMarker(lc[jt],visited) )
					{
						m->SetMarker(lc[jt],visited);
						edges.push_back(lc[jt]);
					}
				inner_faces.push_back(it->first);
			}
		}
		for(dynarray<HandleType,64>::size_type i = 0; i < edges.size(); i++) 
		{
			m->RemMarker(edges[i],visited);
			adj_type const & lc = m->LowConn(edges[i]);
			for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
			{
				if( !m->GetMarker(lc[jt],visited) )
				{
					m->SetMarker(lc[jt],visited);
					nodes.push_back(lc[jt]);
				}
			}
			int nonzero = 0;
			adj_type const & hc = m->HighConn(edges[i]);
			for(adj_type::size_type jt = 0; jt < hc.size(); jt++) if( !m->GetMarker(hc[jt],hm) )//iterate over faces
				if( !m->GetMarker(hc[jt],rem) ) nonzero++; // check if it is not deleted
			if( nonzero == 0 ) //all faces should be deleted, edge to remove
			{
				m->SetMarker(edges[i],rem);
				if( m->GetMarker(edges[i],del_protect) ) doexit = true;
			}
		}
		for(dynarray<HandleType,64>::size_type i = 0; i < nodes.size(); i++) 
		{
			m->RemMarker(nodes[i],visited);
			
			if( m->GetMarker(nodes[i],del_protect) )
			{
				int nonzero = 0;
				adj_type const & hc = m->HighConn(nodes[i]);
				for(adj_type::size_type jt = 0; jt < hc.size(); jt++) if( !m->GetMarker(hc[jt],hm) )
					if( !m->GetMarker(hc[jt],rem) ) nonzero++;
				if( nonzero == 0 )
					doexit = true;
				
			}
		}
		m->ReleaseMarker(visited);
		for(dynarray<HandleType,64>::size_type it = 0; it < inner_faces.size(); it++)
			m->RemMarker(inner_faces[it],rem);
		for(dynarray<HandleType,64>::size_type i = 0; i < edges.size(); i++)  
			m->RemMarker(edges[i],rem);
		m->ReleaseMarker(rem);
		if( doexit ) return Cell(m,InvalidHandle());
		//delete cells
		for(ElementArray<Cell>::size_type j = 0; j < unite.size(); j++)
		{
			if( m->GetMarker(unite.at(j),del_protect) )
				std::cout << __FUNCTION__ << " deleted protected cells, united " << unite.size() << " cells " << std::endl;
			m->Delete(unite.at(j));
		}
		//delete inner faces
		for(dynarray<HandleType,64>::size_type j = 0; j < inner_faces.size(); j++)
		{
			if( m->GetMarker(inner_faces[j],del_protect) )
				std::cout << __FUNCTION__ << " deleted protected faces, united " << unite.size() << " cells " << std::endl;
			m->Delete(inner_faces[j]);
		}
		for(dynarray<HandleType,64>::size_type j = 0; j < edges.size(); j++) //delete unused edges
		{
			adj_type const & hc = m->HighConn(edges[j]);
			if( hc.empty() || m->Count(hc.data(),static_cast<integer>(hc.size()),hm) == 0 ) //there are no faces that use this edge
			{
				if( m->GetMarker(edges[j],del_protect) )
					std::cout << __FUNCTION__ << " deleted protected edge, united " << unite.size() << " cells " << std::endl;
				m->Delete(edges[j]);
			}
		}
		for(dynarray<HandleType,64>::size_type j = 0; j < nodes.size(); j++) //delete unused nodes
		{

			adj_type const & hc = m->HighConn(nodes[j]);
			if( hc.empty() || m->Count(hc.data(),static_cast<integer>(hc.size()),hm) == 0 ) //there are no edges that use this edge
			{
				// there must be no cells that use this node
				assert( m->LowConn(nodes[j]).empty() || m->Count(m->LowConn(nodes[j]).data(),static_cast<integer>(m->LowConn(nodes[j]).size()),hm) == 0 );
				if( m->GetMarker(nodes[j],del_protect) )
					std::cout << __FUNCTION__ << " deleted protected node, united " << unite.size() << " cells " << std::endl;
				m->Delete(nodes[j]);
				//disconnect node from cell
				//we don't need this algorithm, because all dependent cells should be deleted
				//if( !nodes[j]->Hide() )
				//{					
					//adj_iterator it = nodes[j]->LowConn().begin();
					//while(it != nodes[j]->LowConn().end()) if( !(*it)->GetMarker(hm) ) //iterate through cells of the node
					//{
					//	adj_iterator jt =(*it)->HighConn().begin();
					//	while(jt != (*it)->HighConn().end() ) if( !(*jt)->GetMarker(hm) ) // iterate through nodes of the cell
					//	{
					//		if( *jt == nodes[j] )
					//			jt = (*it)->HighConn().erase(jt); //erase link to node
					//		else ++jt;
					//	}
					//  ++it;
					//}
					//nodes[j]->Disconnect();
					//nodes[j]->Delete();
				//}
			}
		}
		//reconstruct cell by outer faces
		return m->CreateCell(faces).first;
	}
	
	
	bool Cell::TestUniteCells(const ElementArray<Cell> & unite, MarkerType del_protect)
	{
		if( unite.empty() ) return false;
		Mesh * m = unite.GetMeshLink();
		tiny_map<HandleType,int,64> face_visit; // we check all edges of faces, inner edges are visited several times, outer - once
		bool doexit = false;
		MarkerType hm = m->HideMarker();			
		for(ElementArray<Cell>::size_type j = 0; j < unite.size(); j++)
		{
			if( m->GetMarker(unite.at(j),del_protect) ) doexit = true; 
			adj_type const & lc = m->LowConn(unite.at(j));
			for(adj_type::size_type it = 0; it < lc.size(); it++) if( !m->GetMarker(lc[it],hm) )
				face_visit[lc[it]]++;
		}
		if( doexit ) return false;
		MarkerType rem = m->CreateMarker();
		dynarray<HandleType,64> edges;
		dynarray<HandleType,64> nodes;		
		for(tiny_map<HandleType,int,64>::iterator it = face_visit.begin(); it != face_visit.end(); it++)
		{
			if( it->second != 1 )
			{
				if( m->GetMarker(it->first,del_protect) ) doexit = true;
				m->SetMarker(it->first,rem);
				adj_type const & lc = m->LowConn(it->first);
				for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
					if( !m->GetMarker(lc[jt],rem) )
					{
						m->SetMarker(lc[jt],rem);
						edges.push_back(lc[jt]);
					}
			}
		}
		for(dynarray<HandleType,64>::size_type i = 0; i < edges.size(); i++) 
		{
			m->RemMarker(edges[i],rem);
			adj_type const & lc = m->LowConn(edges[i]);
			for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
			{
				if( !m->GetMarker(lc[jt],rem) )
				{
					m->SetMarker(lc[jt],rem);
					nodes.push_back(lc[jt]);
				}
			}
			int nonzero = 0;
			adj_type const & hc = m->HighConn(edges[i]);
			for(adj_type::size_type jt = 0; jt < hc.size(); jt++) if( !m->GetMarker(hc[jt],hm) ) //iterate over faces
				if( !m->GetMarker(hc[jt],rem) ) nonzero++; // check if it is not deleted
			if( nonzero == 0 ) //all faces should be deleted, edge to remove
			{
				m->SetMarker(edges[i],rem);
				if( m->GetMarker(edges[i],del_protect) ) doexit = true;
			}
		}
		for(dynarray<HandleType,64>::size_type i = 0; i < nodes.size(); i++) 
		{
			m->RemMarker(nodes[i],rem);
			if( m->GetMarker(nodes[i],del_protect) )
			{
				int nonzero = 0;
				adj_type const & hc = m->HighConn(nodes[i]);
				for(adj_type::size_type jt = 0; jt < hc.size(); jt++) if( !m->GetMarker(hc[jt],hm) )
					if( !m->GetMarker(hc[jt],rem) ) nonzero++;
				if( nonzero == 0 ) doexit = true;
			}
		}
		for(tiny_map<HandleType,int,64>::iterator it = face_visit.begin(); it != face_visit.end(); it++)
				if( it->second != 1 ) m->RemMarker(it->first,rem);
		for(dynarray<HandleType,64>::size_type i = 0; i < edges.size(); i++)  m->RemMarker(edges[i],rem);
		m->ReleaseMarker(rem);
		if( doexit ) return false;
		return true;
	}
	
	
	Face Face::UniteFaces(ElementArray<Face> & unite, MarkerType del_protect)
	{
		Mesh * m = unite.GetMeshLink();
		if( unite.empty() ) return Face(m,InvalidHandle());
		MarkerType hm = m->HideMarker();
		bool doexit = false, dothrow = false;
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
			if( m->GetMarker(unite.at(j),del_protect) ) doexit = true;
		if( doexit ) return Face(m,InvalidHandle());
		dynarray<HandleType,64> high_conn_set;
		MarkerType edge_set = m->CreateMarker();
		MarkerType rem = m->CreateMarker();
		
		
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
		{
			adj_type const & hc = m->HighConn(unite.at(j));
			for(adj_type::size_type it = 0; it < hc.size(); it++) if( !m->GetMarker(hc[it],hm) )
			{
				if( !m->GetMarker(hc[it],edge_set) )
				{
					high_conn_set.push_back(hc[it]);
					m->SetMarker(hc[it],edge_set);
				}
			}
		}		
		
		
		for(dynarray<HandleType,64>::size_type j = 0; j < high_conn_set.size(); j++)
			m->RemMarker(high_conn_set[j],edge_set);
		
		
		if( m->GetTopologyCheck(TRIPLE_SHARED_FACE) && high_conn_set.size() > 2 )
		{
			m->SetTopologyError(TRIPLE_SHARED_FACE);
			if( m->GetTopologyCheck(PRINT_NOTIFY) ) std::cerr << TopologyCheckNotifyString(TRIPLE_SHARED_FACE) << std::endl;
			if( m->GetTopologyCheck(THROW_EXCEPTION) ) throw TopologyCheckError;
		}
		
		dynarray<HandleType,64> nodes;
		tiny_map<HandleType, int,64> edge_visit;
		ElementArray<Edge> edges(m);
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
		{
			adj_type const & lc = m->LowConn(unite.at(j));
			for(adj_type::size_type it = 0; it < lc.size(); it++) if( !m->GetMarker(lc[it],hm) )
				edge_visit[lc[it]]++;
		}
		HandleType first = InvalidHandle(); //first edge
		HandleType prev = InvalidHandle(); //prev node
		for(tiny_map<HandleType,int,64>::iterator it = edge_visit.begin(); it != edge_visit.end(); it++)
		{
			if( it->second == 1 )
			{
				m->SetMarker(it->first,edge_set);
				if( first == InvalidHandle() ) first = it->first;
			}
			else if( it->second == 2 )
			{
				if( m->GetMarker(it->first,del_protect) ) doexit = true; // edge is protected
				m->SetMarker(it->first,rem);
				adj_type const & lc = m->LowConn(it->first);
				for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
				{
					if( !m->GetMarker(lc[jt],edge_set) )
					{
						m->SetMarker(lc[jt],edge_set);
						nodes.push_back(lc[jt]);
					}
				}
			}
			else 
			{
				doexit = true; //the faces we unite would not appear as one surface
				dothrow = true;
			}
		}
		
		for(dynarray<HandleType,64>::size_type j = 0; j < nodes.size(); j++) 
		{
			m->RemMarker(nodes[j],edge_set);
			if( m->GetMarker(nodes[j],del_protect) )
			{
				int nonzero = 0;
				adj_type const & hc = m->HighConn(nodes[j]);
				for(adj_type::size_type it = 0; it < hc.size(); it++) if( !m->GetMarker(hc[it],hm) ) //iterate through edges of the node
					if( !m->GetMarker(hc[it],rem) ) nonzero++; // check if edge should not be deleted
				if( nonzero == 0 ) //all edges are deleted but the node is protected
					doexit = true;
			}
		}
			
		if( doexit )
		{
			for(tiny_map<HandleType,int,64>::iterator it = edge_visit.begin(); it != edge_visit.end(); it++)
			{
				if( it->second != 1 ) m->RemMarker(it->first,rem);
				else m->RemMarker(it->first,edge_set);
			}	
			m->ReleaseMarker(edge_set);
			m->ReleaseMarker(rem);
			assert( !dothrow ); //report the situation, because user need to debug the input
			return Face(m,InvalidHandle());
		}
		edges.push_back(first);
		edges.back()->RemMarker(edge_set);
		bool done = false;
		int q = 0;
		while( !done )
		{
			enumerator k1 = ENUMUNDEF,k2;
			adj_type const & lc = m->LowConn(edges.atback());
			k1 = m->getNext(lc.data(),static_cast<enumerator>(lc.size()),k1,hm);
			k2 = m->getNext(lc.data(),static_cast<enumerator>(lc.size()),k1,hm);
			if( lc[k1] != prev ) 
				prev = lc[k1]; 
			else 
				prev = lc[k2];
			bool found = false;
			adj_type const & hc = m->HighConn(prev);
			for(adj_type::size_type it = 0; it < hc.size(); ++it) if( !m->GetMarker(hc[it],hm) )
			{
				if( hc[it] == first && q != 0)
				{
					found = done = true;
					break;
				}
				if( m->GetMarker(hc[it],edge_set) )
				{
					q++;
					found = true;
					edges.push_back(hc[it]);
					m->RemMarker(hc[it],edge_set);
					break;
				}
			}
			assert(found);
			//if( !found ) throw Failure;
		}
		m->ReleaseMarker(edge_set);
		
		

		
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
			m->SetMarker(unite.at(j),rem);
		
		if( !m->HideMarker() ) //we can't hide elements
		{
			for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_set.size(); it++) //untie every face from cell
			{
				adj_type & lc = m->LowConn(high_conn_set[it]);
				adj_type::iterator jt = lc.begin();
				while( jt != lc.end()) //don't need to check is it hidden
					if( m->GetMarker(*jt,rem) )
						jt = lc.erase(jt);
					else jt++;
			}
		}
		
		if( m->HideMarker() )
			for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++) m->Hide(unite.at(j));
		else
		{
			for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++) // delete all faces
			{
				m->HighConn(unite.at(j)).clear(); //untie cells from face
				if( m->GetMarker(unite.at(j),del_protect) )
					std::cout << __FUNCTION__ << " deleted protected face, united " << unite.size() << " faces " << std::endl;
				m->Delete(unite.at(j)); 
			}
		}

		for(tiny_map<HandleType,int,64>::iterator it = edge_visit.begin(); it != edge_visit.end(); it++)
			if( it->second != 1 )
			{
				m->RemMarker(it->first,rem);
				assert( m->HighConn(it->first).empty() || m->Count(m->HighConn(it->first).data(),static_cast<integer>(m->HighConn(it->first).size()),hm) == 0 ); //it's connected to face somewhere
				if( m->GetMarker(it->first,del_protect) )
					std::cout << __FUNCTION__ << " deleted protected edge, united " << unite.size() << " faces " << std::endl;
				m->Delete(it->first);
			}
			
		m->ReleaseMarker(rem);

		for(dynarray<HandleType,64>::size_type it = 0; it < nodes.size(); it++) //delete nodes inside the face
		{
			adj_type const & hc = m->HighConn(nodes[it]);
			if( hc.empty() || m->Count(hc.data(),static_cast<integer>(hc.size()),hm) == 0 )
			{
				if( !m->Hide(nodes[it]) )
				{
					adj_type & lc = m->LowConn(nodes[it]);
					adj_type::iterator jt = lc.begin();
					while(jt != lc.end() ) // iterate through cells of the node
					{
						adj_type & ihc = m->HighConn(*jt);
						adj_type::iterator qt = ihc.begin(); //iterate through nodes of the cell
						while( qt != ihc.end() )
						{
							if( *qt == nodes[it] ) 
								qt = ihc.erase(qt); //remove links from the cell to the node
							else ++qt;
						}
						++jt;
					}
					lc.clear(); // remove links to cells
					if( m->GetMarker(nodes[it],del_protect) )
						std::cout << __FUNCTION__ << " deleted protected node, united " << unite.size() << " faces " << std::endl;
					m->Destroy(nodes[it]);
				}
			}
		}
				
		Face ret = m->CreateFace(edges).first;
		
		assert( ret->GetGeometricType() != MultiLine );
		
		adj_type & hc = m->HighConn(ret->GetHandle());
		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_set.size(); it++)  //tie new face to old cells
		{
			hc.push_back(high_conn_set[it]); // connect new face to cells
			m->LowConn(high_conn_set[it]).push_back(ret.GetHandle()); // connect cells to new face
			
		}
		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_set.size(); it++)  //tie new face to old cells
		{
			m->ComputeGeometricType(high_conn_set[it]);
			m->RecomputeGeometricData(high_conn_set[it]);
		}
		
		return ret;
	}
	
	
	bool Face::TestUniteFaces(const ElementArray<Face> & unite, MarkerType del_protect)
	{
		Mesh * m = unite.GetMeshLink();
		if( unite.size() == 0 ) return false;
		bool doexit = false, dothrow = false;
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
			if( m->GetMarker(unite.at(j),del_protect) ) doexit = true;
		MarkerType hm = m->HideMarker();
		if( doexit ) return false;
		dynarray<HandleType,64> high_conn_set;
		MarkerType rem = m->CreateMarker();
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
		{
			adj_type const & hc = m->HighConn(unite.at(j));
			for(adj_type::size_type it = 0; it < hc.size(); it++) if( !m->GetMarker(hc[it],hm) )
				if( !m->GetMarker(hc[it],rem) )
				{
					high_conn_set.push_back(hc[it]);
					m->SetMarker(hc[it],rem);
				}
		}
		for(dynarray<HandleType,64>::size_type j = 0; j < high_conn_set.size(); j++) 
			m->RemMarker(high_conn_set[j],rem);
		dynarray<HandleType,64> nodes;
		tiny_map<HandleType, int,64> edge_visit;
		for(ElementArray<Face>::size_type j = 0; j < unite.size(); j++)
		{
			adj_type const & lc = m->LowConn(unite.at(j));
			for(adj_type::size_type it = 0; it < lc.size(); it++) if( !m->GetMarker(lc[it],hm) )
				edge_visit[lc[it]]++;
		}
		for(tiny_map<HandleType,int,64>::iterator it = edge_visit.begin(); it != edge_visit.end(); it++)
		{
			if( it->second == 2 )
			{
				if( m->GetMarker(it->first,del_protect) ) doexit = true; // edge is protected
				m->SetMarker(it->first,rem);
				adj_type const & lc = m->LowConn(it->first);
				for(adj_type::size_type jt = 0; jt < lc.size(); jt++) if( !m->GetMarker(lc[jt],hm) )
				{
					if( !m->GetMarker(lc[jt],rem) )
					{
						m->SetMarker(lc[jt],rem);
						nodes.push_back(lc[jt]);
					}
				}
			}
			else if( it->second != 1 )
			{
				doexit = true;
				dothrow = true;
			}
		}
		for(dynarray<HandleType,64>::size_type j = 0; j < nodes.size(); j++) 
		{
			m->RemMarker(nodes[j],rem);
			if( m->GetMarker(nodes[j],del_protect) )
			{
				int nonzero = 0;
				adj_type const & hc = m->HighConn(nodes[j]);
				for(adj_type::size_type it = 0; it < hc.size(); it++) if( !m->GetMarker(hc[it],hm) )//iterate through edges of the node
					if( !m->GetMarker(hc[it],rem) ) nonzero++; // check if edge should not be deleted
				if( nonzero == 0 ) //all edges are deleted but the node is protected
					doexit = true;
			}
		}
		for(tiny_map<HandleType,int,64>::iterator it = edge_visit.begin(); it != edge_visit.end(); it++)
			if( it->second != 1 ) m->RemMarker(it->first,rem);
		m->ReleaseMarker(rem);
		if( doexit )
		{
			assert(!dothrow);
			//if( dothrow ) throw Impossible; //something went the way it shouldn't, possibly bad input
			return false;
		}
		return true;
	}

	Edge Edge::UniteEdges(ElementArray<Edge> & edges, MarkerType del_protect)
	{
		Mesh * m = edges.GetMeshLink();
		if( edges.size() == 0 ) return Edge(m,InvalidHandle());
		
		bool doexit = false, dothrow = false;
		MarkerType hm = m->HideMarker();
		MarkerType rem = m->CreateMarker();
		dynarray<HandleType,64> cells;
		dynarray<HandleType,64> high_conn_faces;
		tiny_map<HandleType,int,64> nodes;
		ElementArray<Node> build_nodes(m);
		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it)
		{
			if( m->GetMarker(edges.at(it),del_protect) )
				doexit = true;
			adj_type const & hc = m->HighConn(edges.at(it));
			for(adj_type::size_type jt = 0; jt != hc.size(); ++jt) if( !m->GetMarker(hc[jt],hm) )
			{
				if( !m->GetMarker(hc[jt],rem) )
				{
					high_conn_faces.push_back(hc[jt]);
					m->SetMarker(hc[jt],rem);
				}
			}
			adj_type const & lc = m->LowConn(edges.at(it));
			for(adj_type::size_type jt = 0; jt < lc.size(); ++jt) if( !m->GetMarker(lc[jt],hm) )
				nodes[lc[jt]]++;
		}
		
		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_faces.size(); ++it) 
		{
			m->RemMarker(high_conn_faces[it],rem);
			adj_type const & hc = m->HighConn(high_conn_faces[it]);
			for(adj_type::size_type jt = 0; jt < hc.size(); ++jt)
			{
				if( !m->GetMarker(hc[jt],rem) )
				{
					m->SetMarker(hc[jt],rem);
					cells.push_back(hc[jt]);
				}
			}
		}

		if( doexit )
		{
			m->ReleaseMarker(rem);
			return Edge(m,InvalidHandle());
		}
		
		for(tiny_map<HandleType,int,64>::iterator it = nodes.begin(); it != nodes.end(); it++)
		{
			if( it->second == 1 )
				build_nodes.push_back(it->first);
			else if( it->second == 2 )
			{
				if( m->GetMarker(it->first,del_protect) )
					doexit = true;
			}
			else 
			{
				doexit = true; //edges have some loop
				dothrow = true;
			}
		}

		if( doexit )
		{
			m->ReleaseMarker(rem);
			assert(!dothrow);
			//if( dothrow ) throw Impossible; // bad input
			return Edge(m,InvalidHandle());
		}


		dynarray<adj_type::size_type,64> insert_pos; //position where we insert new edge

		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it) m->SetMarker(edges.at(it),rem);

		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_faces.size(); ++it)
		{
			adj_type const & lc = m->LowConn(high_conn_faces[it]); //edges of face
			for(adj_type::size_type k = 0; k < lc.size(); k++) //insert new edge to the first position where we delete old edge
			{
				if( m->GetMarker(lc[k],rem) )
				{
					//all united edges should appear in consecutive order in deleted face
					adj_type::size_type sum = 0, j = k;
					while( !m->GetMarker(lc[j],hm) && m->GetMarker(lc[j],rem) )
					{
						sum++; j++;
					}
					if( sum != static_cast<adj_type::size_type>(edges.size()) ) //not all edges belong to current face - face will be broken!
					{
						doexit = true;
						dothrow = true;
					}
					insert_pos.push_back(k);
					break;
				}
			}
		}

		if( doexit )
		{
			for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it) m->RemMarker(edges.at(it),rem);
			m->ReleaseMarker(rem);
			assert(!dothrow);
			//if( dothrow ) throw Impossible; // bad input
			return Edge(m,InvalidHandle());
		}

		if( !m->HideMarker() ) //disconnect if cannot hide
		{
			for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_faces.size(); ++it)
			{
				adj_type & lc = m->LowConn(high_conn_faces[it]);
				adj_iterator jt = lc.begin(); //iterate over edges of faces
				while( jt != lc.end())
				{
					if( m->GetMarker(*jt,rem) )
						jt = lc.erase(jt);
					else ++jt;
				}
			}
		}


		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it)//delete edges
		{
			m->RemMarker(edges.at(it),rem);
			if( !m->Hide(edges.at(it)) ) //cannot hide
			{
				m->HighConn(edges.at(it)).clear(); //remove connection from edge to faces
				m->Destroy(edges.at(it));
			}
		}

		m->ReleaseMarker(rem);

		for(tiny_map<HandleType,int,64>::iterator it = nodes.begin(); it != nodes.end(); it++)
		{
			if( it->second != 1 )
			{
				if( !m->Hide(it->first) ) //cannot hide, we have to untie cells from nodes
				{
					adj_type & lc = m->LowConn(it->first);
					for(adj_type::size_type qt = 0; qt < lc.size(); ++qt) //iterate through cells of the node
					{
						adj_type & hc = m->HighConn(lc[qt]);
						adj_iterator jt = hc.begin();
						while(jt != hc.end() ) // iterate through nodes of the cell
						{
							if( *jt == it->first )
								jt = hc.erase(jt); //erase link to node
							else ++jt;
						}
					}
					m->Destroy(it->first);
				}
			}
		}
			
		Edge e = m->CreateEdge(build_nodes).first;
		adj_type & ehc = m->HighConn(e->GetHandle());

		for(dynarray<adj_type::size_type,64>::size_type k = 0; k < insert_pos.size(); k++)
		{
			adj_type & lc = m->LowConn(high_conn_faces[k]);
			lc.insert(lc.begin()+insert_pos[k],e->GetHandle());
			ehc.push_back(high_conn_faces[k]);
			m->ComputeGeometricType(high_conn_faces[k]);
			m->RecomputeGeometricData(high_conn_faces[k]);
		}

		for(dynarray<HandleType,64>::size_type it = 0; it < cells.size(); ++it)
		{
			m->ComputeGeometricType(cells[it]);
			m->RecomputeGeometricData(cells[it]);
		}


		return e;
	}
	bool Edge::TestUniteEdges(const ElementArray<Edge> & edges, MarkerType del_protect)
	{
		if( edges.empty() ) return false;
		Mesh * m = edges.GetMeshLink();
		bool doexit = false, dothrow = false;
		MarkerType hm = m->HideMarker();
		MarkerType rem = m->CreateMarker();
		dynarray<HandleType,64> high_conn_faces;
		tiny_map<HandleType,int,64> nodes;

		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it)
		{
			if( m->GetMarker(edges.at(it),del_protect) )
				doexit = true;
			adj_type const & hc = m->HighConn(edges.at(it));
			for(adj_type::size_type jt = 0; jt < hc.size(); ++jt) if( !m->GetMarker(hc[jt],hm) )
			{
				if( !m->GetMarker(hc[jt],rem) )
				{
					high_conn_faces.push_back(hc[jt]);
					m->SetMarker(hc[jt],rem);
				}
			}
			adj_type const & lc = m->LowConn(edges.at(it));
			for(adj_type::size_type jt = 0; jt < lc.size(); ++jt) if( !m->GetMarker(lc[jt],hm) )
				nodes[lc[jt]]++;
		}
		
		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_faces.size(); ++it) 
			m->RemMarker(high_conn_faces[it],rem);

		if( doexit )
		{
			m->ReleaseMarker(rem);
			return false;
		}
		
		for(tiny_map<HandleType,int,64>::iterator it = nodes.begin(); it != nodes.end(); it++)
		{
			if( it->second == 1 )
			{
//				build_nodes.push_back(it->first);
			}
			else if( it->second == 2 )
			{
				if( m->GetMarker(it->first,del_protect) )
					doexit = true;
			}
			else 
			{
				doexit = true;
				dothrow = true;
			}
		}

		if( doexit )
		{
			m->ReleaseMarker(rem);
			assert(!dothrow); //inner loop in deleted edges
			//if( dothrow ) throw Impossible; // bad input
			return false;
		}



		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it) m->SetMarker(edges.at(it),rem);

		for(dynarray<HandleType,64>::size_type it = 0; it < high_conn_faces.size(); ++it)
		{
			adj_type const & lc = m->LowConn(high_conn_faces[it]);
			for(adj_type::size_type k = 0; k < lc.size(); k++) //insert new edge to the first position where we delete old edge
			{
				if( m->GetMarker(lc[k],rem) )
				{
					//all united edges should appear in consecutive order in deleted face
					adj_type::size_type sum = 0, j = k;
					while( !m->GetMarker(lc[j],hm) && m->GetMarker(lc[j],rem) )
					{
						sum++; j++;
					}
					if( sum != static_cast<adj_type::size_type>(edges.size()) )
					{
						doexit = true;
						dothrow = true;
					}
					break;
				}
			}
		}
		for(ElementArray<Edge>::size_type it = 0; it < edges.size(); ++it) m->RemMarker(edges.at(it),rem);

		if( doexit )
		{
			m->ReleaseMarker(rem);
			assert(!dothrow);
			//if( dothrow ) throw Impossible; // bad input
			return false;
		}

		return true;
	}


	ElementArray<Edge> Edge::SplitEdge(Edge e, const ElementArray<Node> & nodes, MarkerType del_protect)
	{
		Mesh * m = e->GetMeshLink();
		ElementArray<Edge> ret(m);
		dynarray<HandleType,64> faces;
		dynarray<HandleType,128> cells;
		HandleType n[2];
		if( e->GetMarker(del_protect) || nodes.empty() ) return ret;
		ret.reserve(nodes.size()+1);
		MarkerType hm = m->HideMarker();
		MarkerType dup = m->CreateMarker();
		adj_type & hc = m->HighConn(e->GetHandle());
		
		for(adj_type::size_type it = 0; it < hc.size(); ++it) if( !m->GetMarker(hc[it],hm) )
		{
			faces.push_back(hc[it]);
			adj_type const & ihc = m->HighConn(hc[it]);
			for(adj_type::size_type jt = 0; jt < ihc.size(); ++jt) if( !m->GetMarker(ihc[jt],hm) )
			{
				if( !m->GetMarker(ihc[jt],dup) )
				{
					cells.push_back(ihc[jt]);
					m->SetMarker(ihc[jt],dup);
				}
			}
		}

		for(dynarray<HandleType,128>::size_type it = 0; it < cells.size(); ++it)
			m->RemMarker(cells[it],dup);

		m->ReleaseMarker(dup);

		int k = 0;

		adj_type const & lc = m->LowConn(e->GetHandle());

		for(adj_type::size_type it = 0; it < lc.size(); ++it)
			if( !m->GetMarker(lc[it],hm) )
				n[k++] = lc[it];

		assert( k == 2 );

		dynarray<adj_type::size_type,64> insert_pos; //position where we insert new edges

		for(dynarray<HandleType,64>::size_type it = 0; it < faces.size(); ++it)
		{
			adj_type const ilc = m->LowConn(faces[it]);
			for(adj_type::size_type jt = 0; jt < ilc.size(); ++jt) if( !m->GetMarker(ilc[jt],hm) )
			{
				if( ilc[jt] == e->GetHandle() )
				{
					insert_pos.push_back(jt);
					break;
				}
			}
		}
		

		if( !e->Hide() ) //we cannot hide the edge, should disconnect faces
		{
			for(adj_type::size_type it = 0; it < hc.size(); ++it)
			{
				adj_type & ilc = m->LowConn(hc[it]);
				adj_iterator jt = ilc.begin();
				while(jt != ilc.end())
					if( *jt == e->GetHandle() ) jt = ilc.erase(jt);
					else ++jt;
			}
			hc.clear();
			m->Destroy(e->GetHandle());
		}
		
		{
			ElementArray<Node> build_nodes(m,2);
			build_nodes.at(0) = n[0];
			build_nodes.at(1) = nodes.at(0);
			ret.push_back(m->CreateEdge(build_nodes).first->GetHandle());
			
			for(ElementArray<Node>::size_type k = 0; k < nodes.size()-1; k++)
			{
				build_nodes.at(0) = nodes.at(k);
				build_nodes.at(1) = nodes.at(k+1);
				ret.push_back(m->CreateEdge(build_nodes).first->GetHandle());
			}

			build_nodes.at(0) = nodes.at(nodes.size()-1);
			build_nodes.at(1) = n[1];
			ret.push_back(m->CreateEdge(build_nodes).first->GetHandle());
		}

		//connect new edges to faces
		for(dynarray<HandleType,64>::size_type it = 0; it < faces.size(); ++it)
		{
			adj_type & lc = m->LowConn(faces[it]);
			lc.insert(lc.begin()+insert_pos[it],ret.begin(),ret.end());
			m->ComputeGeometricType(faces[it]);
			m->RecomputeGeometricData(faces[it]);
		}

		for(dynarray<HandleType,128>::size_type it = 0; it < cells.size(); ++it)
		{
			m->ComputeGeometricType(cells[it]);
			m->RecomputeGeometricData(cells[it]);
		}
		return ret;
	}
	
	bool Edge::TestSplitEdge(Edge e, const ElementArray<Node> & nodes, MarkerType del_protect)
	{
		return !nodes.empty() && !e->GetMarker(del_protect);
	}

	
	ElementArray<Face> Face::SplitFace(Face face, const ElementArray<Edge> & edges, MarkerType del_protect)
	{
		Mesh * m = face->GetMeshLink();
		ElementArray<Edge> loop(m);
		ElementArray<Face> ret(m);
		dynarray<HandleType,128> temp;
		if( edges.empty() || face->GetMarker(del_protect) ) return ret;
		MarkerType hm = m->HideMarker();
		ElementArray<Edge>::size_type input_edges = edges.size();
		dynarray<HandleType,2> cells;

		adj_type & hc = m->HighConn(face->GetHandle());
		for(adj_type::size_type it = 0; it < hc.size(); ++it) if( !m->GetMarker(hc[it],hm) )
			cells.push_back(hc[it]);

		//assert(cells.size() == 2);

		temp.insert(temp.end(),edges.begin(),edges.end());

		adj_type & lc = m->LowConn(face->GetHandle());
		for(adj_type::size_type it = 0; it < lc.size(); ++it) if( !m->GetMarker(lc[it],hm) )
			temp.push_back(lc[it]);

		incident_matrix<Edge> mat(m,temp.begin(),temp.end(),input_edges);
		
		if( !face->Hide() )
		{
			for(dynarray<HandleType,2>::size_type k = 0; k < cells.size(); k++)
			{
				adj_type & ilc = m->LowConn(cells[k]);
				for(adj_type::size_type it = 0; it < ilc.size(); ++it)
				{
					if( ilc[it] == face->GetHandle() )
					{
						ilc.erase(ilc.begin()+it);
						break;
					}
				}
			}
			hc.clear();
			m->Destroy(face->GetHandle());
		}
		
		do
		{
			mat.find_shortest_loop(loop);
			if (!loop.empty())
			{
				ret.push_back(m->CreateFace(loop).first);
				adj_type & hc = m->HighConn(ret.back()->GetHandle());
				hc.replace(hc.begin(),hc.end(),cells.begin(),cells.end());
			}
			else break;
		} while(true);
		

		


		for(dynarray<HandleType,2>::size_type it = 0; it < cells.size(); ++it)
		{
			adj_type & hc = m->HighConn(cells[it]); //cell nodes
			hc.clear(); //have to recompute cell nodes
			adj_type & lc = m->LowConn(cells[it]); //cell faces
			lc.insert(lc.end(),ret.begin(),ret.end());
			m->ComputeGeometricType(cells[it]);
			ElementArray<Node> nn(m);
			m->RestoreCellNodes(cells[it],nn);
			hc.insert(hc.begin(),nn.begin(),nn.end());
			m->RecomputeGeometricData(cells[it]);
			//have to add cell to nodes that do not yet have connection to the cell
			for(ElementArray<Node>::iterator jt = nn.begin(); jt != nn.end(); ++jt)
			{
				adj_type & nlc = m->LowConn(*jt); //node cells
				bool have_cell = false;
				for(adj_type::iterator kt = nlc.begin(); kt != nlc.end() && !have_cell; ++kt)
					if( *kt == cells[it] ) have_cell = true;
				if( !have_cell )
					nlc.push_back(cells[it]);
			}
		}

		return ret;
	}
	bool Face::TestSplitFace(Face face, const ElementArray<Edge> & edges, MarkerType del_protect)
	{
		return !edges.empty() && !face->GetMarker(del_protect);
	}


	ElementArray<Cell> Cell::SplitCell(Cell cell, const ElementArray<Face> & faces, MarkerType del_protect)
	{
		Mesh * m = cell->GetMeshLink();
		ElementArray<Cell> ret(m);
		ElementArray<Face> loop(m);
		dynarray<HandleType,128> temp;
		if( faces.empty() || cell->GetMarker(del_protect) ) return ret;
		MarkerType hm = m->HideMarker();
		ElementArray<Face>::size_type input_faces = faces.size();
		temp.insert(temp.end(),faces.begin(),faces.end());
		adj_type & lc = m->LowConn(cell->GetHandle());
		for(adj_type::size_type it = 0; it < lc.size(); ++it) if( !m->GetMarker(lc[it],hm) )
			temp.push_back(lc[it]);
			
		incident_matrix<Face> mat(m,temp.begin(),temp.end(),input_faces);
		//mat.print_matrix();
		cell->Delete();
			
		do
		{
			mat.find_shortest_loop(loop);
			if (!loop.empty()) ret.push_back(m->CreateCell(loop).first);
			else break;
		} while( true );

		return ret;
	}
	bool Cell::TestSplitCell(Cell cell, const ElementArray<Face> & faces, MarkerType del_protect)
	{
		return !faces.empty() && !cell->GetMarker(del_protect);
	}
	
	void Mesh::BeginModification()
	{
		hide_element = CreateMarker();
		new_element = CreateMarker();
	}
	
	void Mesh::SwapModification()
	{
		MarkerType temp = hide_element;
		hide_element = new_element;
		new_element = temp;

		integer tmp[6];
		memcpy(tmp,hidden_count,sizeof(integer)*6);
		memcpy(hidden_count,hidden_count_zero,sizeof(integer)*6);
		memcpy(hidden_count_zero,tmp,sizeof(integer)*6);

		for(ElementType etype = EDGE; etype <= CELL; etype = etype << 1)
		{
			for(integer it = 0; it < LastLocalID(etype); ++it) if( isValidElement(etype,it) )
			{
				HandleType h = ComposeHandle(etype,it);
				if( GetMarker(h,new_element) )
				{
					ComputeGeometricType(h);
					RecomputeGeometricData(h);
				}
			}
		}
	}
	
	void Mesh::ApplyModification()
	{
		for(Mesh::iteratorTag it = BeginTag(); it != EndTag(); ++it)
		{
			if( it->GetDataType() == DATA_REFERENCE )
			{
				if( *it == HighConnTag() || *it  == LowConnTag() ) continue;
				for(ElementType etype = NODE; etype <= CELL; etype = etype << 1)
					if( it->isDefined(etype) )
					{
						if( it->isSparse(etype) )
						{
							for(integer jt = 0; jt < LastLocalID(etype); ++jt) if( isValidElement(etype,jt) )
							{
								HandleType h = ComposeHandle(etype,jt);
								if( HaveData(h,*it) )
								{
									reference_array arr = ReferenceArray(h,*it);
									for(reference_array::size_type qt = 0; qt < arr.size(); ++qt)
										if( arr.at(qt) != InvalidHandle() && Hidden(arr.at(qt)) ) arr.at(qt) = InvalidHandle();
								}
							}
						}
						else
						{
							for(integer jt = 0; jt < LastLocalID(etype); ++jt) if( isValidElement(etype,jt) )
							{
								HandleType h = ComposeHandle(etype,jt);
								reference_array arr = ReferenceArray(h,*it);
								for(reference_array::size_type qt = 0; qt < arr.size(); ++qt)
									if( arr.at(qt) != InvalidHandle() && Hidden(arr.at(qt)) ) arr.at(qt) = InvalidHandle();
							}
						}
					}
			}
		}
		//need to gather the set of deleted elements
		/*//old approach
		ElementSet erase = CreateSet("TEMPORARY_ERASE_SET").first;
		for(ElementType etype = NODE; etype <= ESET; etype = etype << 1)
		{
			for(integer it = 0; it < LastLocalID(etype); ++it)
				if( isValidElement(etype,it) && Hidden(ComposeHandle(etype,it)) )
					erase->PutElement(ComposeHandle(etype,it)); 
		}
		//the formed above set will be automatically sorted by handles, it does not harm to explicitly indicate that,
		//in case any algorithm further will use this fact (Subtract currently would not use this fact)
		//but may use by retriving lower_bound/higher_bound O(log(n)) operations to narrow performed operations
		erase->BulkDF(SetComparatorTag()) = ElementSet::HANDLE_COMPARATOR;
		*/

		//for(integer jt = 0; jt < LastLocalID(ESET); ++jt) if( isValidElementSet(jt) )
		for(Mesh::iteratorSet it = BeginSet(); it != EndSet(); it++)
		{
			//ElementSet it = EsetByLocalID(jt);
			//std::cout << "set name: " << it->GetName() << " size " << it->Size() << " id " << it->LocalID() << std::endl;
			if( it->HaveParent() && it->GetParent()->Hidden() )
				it->GetParent()->RemChild(it->self());
			while( it->HaveChild() && it->GetChild()->Hidden() )
				it->RemChild(it->GetChild());
			while( it->HaveSibling() && it->GetSibling()->Hidden() )
				it->RemSibling(it->GetSibling());
			temp_hide_element = hide_element;
			hide_element = 0;
			ElementSet::iterator jt = it->Begin();
			//int q = 0;
			while(jt != it->End() )
			{
				//++q;
				//std::cout << "check element " << ElementTypeName(jt->GetElementType()) << " num " << jt->LocalID() << " handle " << jt->GetHandle() << std::endl;
				if( jt->GetMarker(temp_hide_element) ) 
				{
					//std::cout << "erase element " << ElementTypeName(jt->GetElementType()) << " num " << jt->LocalID() << " handle " << jt->GetHandle() << std::endl;
					jt = it->Erase(jt);
				}
				else ++jt;
			}
			//std::cout << "size " << it->Size() << " traversed " << q << std::endl;
			hide_element = temp_hide_element;
			//it->Subtract(erase); //old approach
		}
		//Destroy(erase);//old approach
	}

	void Mesh::ResolveModification()
	{
		throw NotImplemented;
	}
	
	void Mesh::EndModification()
	{
		//ApplyModification();
		//temp_hide_element = hide_element;
		//hide_element = 0;
		for(ElementType etype = CELL; etype >= NODE; etype = PrevElementType(etype))
		{
			for(integer it = 0; it < LastLocalID(etype); ++it) if( isValidElement(etype,it) )
			{
				HandleType h = ComposeHandle(etype,it);
				RemMarker(h,new_element);
				if( GetMarker(h,hide_element) )
					Destroy(h);
			}
		}
		memset(hidden_count,0,sizeof(integer)*6);
		memset(hidden_count_zero,0,sizeof(integer)*6);
		ReleaseMarker(hide_element);
		ReleaseMarker(new_element);
		hide_element = 0;
		new_element = 0;
		if( have_global_id ) AssignGlobalID(have_global_id);
	}

#if defined(USE_PARALLEL_WRITE_TIME)
#define REPORT_STR(x) {WriteTab(out_time) << "<TEXT><![CDATA[" << x << "]]></TEXT>" << std::endl;}
#define REPORT_VAL(str,x) {WriteTab(out_time) << "<VALUE name=\"" << str << "\"> <CONTENT><![CDATA[" << x << "]]></CONTENT> <CODE><![CDATA[" << #x << "]]></CODE></VALUE>" << std::endl;}
#endif

	void Mesh::Destroy(HandleType h)
	{
		//std::cout << "destroy " << ElementTypeName(GetHandleElementType(h)) << " id " << GetHandleID(h) << " handle " << h << (GetMarker(h,temp_hide_element) ? " hidden " : " not hidden ") << std::endl;
		assert(isValidHandleRange(h));
		assert(isValidHandle(h));
		ElementType htype = GetHandleElementType(h);
		//if( Hidden(h) ) Show(h);
		if( htype & (NODE|EDGE|FACE|CELL) )
			ElementByHandle(h).Disconnect(true);
		else if( htype & ESET )
		{
			ElementSet eset(this,h);
			if( eset->HaveParent() )
				eset->GetParent()->RemChild(eset);
			while( eset->HaveChild() )
				eset->RemChild(eset->GetChild());
		}
		for(iteratorTag t = BeginTag(); t != EndTag(); ++t) 
			if( t->isDefined(htype) ) DelData(h,*t);
		UntieElement(GetHandleElementNum(h),GetHandleID(h));
	}

	bool Mesh::Hide(HandleType h)
	{
		if(HideMarker()) 
		{
			if( !Hidden(h) )
			{
				//TODO if we hide a back cell for a face we have to swap it's normal!
				if( GetHandleElementType(h) == CELL ) Cell(this,h)->SwapBackCell();
				hidden_count[GetHandleElementNum(h)]++;
				SetMarker(h,HideMarker()); 
			}
			return true;
		} 
		return false;
	}
	
	bool Mesh::Show(HandleType h)
	{
		if(HideMarker()) 
		{
			if( Hidden(h) )
			{
				hidden_count[GetHandleElementNum(h)]--;
				RemMarker(h,HideMarker()); 
			}
			return true;
		} 
		return false;
	}
	
	bool Mesh::Delete(HandleType h) 
	{
		if(!New(h) && Hide(h))
		{
			if( GetHandleElementType(h) != CELL ) //mark all elements that rely on this that they should be deleted
			{
				Element::adj_type & hc = HighConn(h);
				for(Element::adj_type::size_type it = 0; it < hc.size(); ++it) if( !Hidden(hc[it]) ) Hide(hc[it]);
			}
			return false;
		}
		else 
		{
			Destroy(h);
			return true;
		}
	}
	
	bool Mesh::Hidden(HandleType h) const
	{
		assert(h != InvalidHandle());
		return GetMarker(h,HideMarker());
	}
	
	bool Mesh::New(HandleType h) const
	{
		assert(h != InvalidHandle());
		return GetMarker(h,NewMarker());
	}
	
	bool Element::Hide() const
	{
		return GetMeshLink()->Hide(GetHandle());
	}
	
	bool Element::Show() const
	{
		return GetMeshLink()->Show(GetHandle());
	}
	
	bool Element::Delete() 
	{
		bool ret = GetMeshLink()->Delete(GetHandle());
		if( ret ) handle = InvalidHandle();
		return ret;
	}
	
	bool Element::Hidden() const
	{
		return GetMeshLink()->Hidden(GetHandle());
	}
	
	bool Element::New() const
	{
		return GetMeshLink()->New(GetHandle());
	}
	
	
	Storage::enumerator Mesh::getNext(const HandleType * arr, enumerator size, enumerator k, MarkerType marker) const
	{
		k++;
		while(k < size && GetMarker(arr[k],marker)) k++;
		return k;
	}
	
	Storage::enumerator Mesh::Count(const HandleType * arr, enumerator size, MarkerType marker) const
	{
		enumerator ret = 0, k = 0;
		while(k < size) 
		{
			if( !GetMarker(arr[k],marker) ) ret++;
			k++;
		}
		return ret;
	}

	void Element::UpdateGeometricData() const
	{
		GetMeshLink()->RecomputeGeometricData(GetHandle());
	}
	
	void Cell::SwapBackCell() const
	{
		Mesh * m = GetMeshLink();
		
		if( m->HaveGeometricData(ORIENTATION,FACE) )
		{
			//retrive faces
			MarkerType hm = m->HideMarker();
			adj_type & lc = m->LowConn(GetHandle());
			for( adj_type::size_type it = 0; it < lc.size(); it++) if( !m->GetMarker(lc[it],hm) ) //iterator over unhidden faces
			{
				adj_type & hc = m->HighConn(lc[it]);
				if( !hc.empty() && m->Count(hc.data(),static_cast<enumerator>(hc.size()),hm) == 2 ) //there are two cells connected to face
				{
					enumerator k1 = ENUMUNDEF, k2;
					k1 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
					if( hc[k1] == GetHandle() ) //the first cell is current
					{
						k2 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
						std::swap(hc[k1],hc[k2]);
						//hc[k2] = GetHandle(); //cannot use the cell because virtualization table is already destroyed and FixNormalOrientation will do bad things
						//hc.resize(1); //just remove element, we will do this anyway later
						Face(m,lc[it])->FixNormalOrientation(); //restore orientation
					}
				}
			}
		}
	}
	
	void Element::Disconnect(const HandleType * adjacent, INMOST_DATA_ENUM_TYPE num) const
	{
		Mesh * m = GetMeshLink();
		INMOST_DATA_ENUM_TYPE k = 0;
		dynarray<HandleType,64> arr[4];
		MarkerType mrk = m->CreateMarker(), mod = m->CreateMarker();
		for(k = 0; k < num; k++) m->SetMarker(adjacent[k], mrk);
		adj_iterator it;
		// disconnects nodes from current edge, edges from current face, faces from current cell
		if( GetElementType() > NODE ) //cannot disconnect cells from current node
		{
			adj_type & lc = m->LowConn(GetHandle());
			it = lc.begin();
			//look into nodes (edge), edges (face), faces (cell)
			while(it != lc.end() ) 
			{
				if( m->GetMarker(*it,mrk) ) //element should be disconnected
				{
					adj_type & hc = m->HighConn(*it);
					if( GetElementType() == CELL ) //update some geometric data
					{
						MarkerType hm = m->HideMarker();
						if( !hc.empty() && m->Count(hc.data(),static_cast<enumerator>(hc.size()),hm) == 2 )
						{
							enumerator k1 = ENUMUNDEF, k2;
							k1 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
							if( hc[k1] == GetHandle() ) //the first cell is current
							{
								k2 = m->getNext(hc.data(),static_cast<enumerator>(hc.size()),k1,hm);
								hc[k1] = hc[k2];
								hc[k2] = GetHandle();
								Face(m,*it)->FixNormalOrientation(); //restore orientation
							}
						}
					}
					//look into edges of node (edge) faces of edge (face) cells of face (cell)
					for(adj_iterator jt = hc.begin(); jt != hc.end(); ++jt)
					{
						if( *jt == GetHandle() ) //remove me
						{
							hc.erase(jt);
							//don't update me
							//only lower adjacencies affect geometric data of element
							//this may be subject to change
							break;
						}
					}
					//update my data
					if( !m->GetMarker(GetHandle(),mod) ) 
					{
						m->SetMarker(GetHandle(),mod);
						arr[GetElementNum()].push_back(GetHandle());
					}
					//disconnect element
					it = lc.erase(it);
				}
				else it++;
			}
		}
		// disconnect edges from current node, faces from current edge, cells from current face
		if( GetElementType() < CELL ) //Cell cannot be disconnected from nodes
		{
			adj_type & hc = m->HighConn(GetHandle());
			it = hc.begin();
			//go over edges (node), faces (edge), cells (face)
			while(it != hc.end() ) 
			{
				if( m->GetMarker(*it,mrk) ) //should be disconnected
				{
					adj_type & lc = m->LowConn(*it);
					//look into nodes of edge (node), edges of face (edge), faces of cell (face)
					for(adj_iterator jt = lc.begin(); jt != lc.end(); ++jt)
					{
						if( *jt == GetHandle() ) //remove me
						{
							lc.erase(jt);
							// lower adjacencies of edge (node), face (edge), cell (face) where modified
							// update it's geometric data
							if( !m->GetMarker(*it,mod) ) 
							{
								m->SetMarker(*it,mod);
								arr[GetHandleElementNum(*it)].push_back(*it);
							}
							break;
						}
					}
					/* //don't update me
					//only lower adjacencies affect geometric data of element
					//this may be subject to change
					if( !GetMarker(mod) )
					{
						SetMarker(mod);
						arr[GetElementNum()].push_back(this);
					}
					*/
					//disconnect element
					it = hc.erase(it);
				}
				else it++;
			}
		}
		for(k = 0; k < num; k++) m->RemMarker(adjacent[k],mrk);
		//if element placed below on ierarhy was modified, then all upper elements
		//should be also modified, start from lowest elements in ierarchy and go up
		for(ElementType etype = NODE; etype <= CELL; etype = etype << 1 )
		{
			int el_num = ElementNum(etype);
			for(dynarray<HandleType, 64>::size_type it = 0; it < arr[el_num].size(); it++) 
			{
				assert( GetHandleElementType(arr[el_num][it]) == etype );
				if( etype < CELL ) //check for upper adjacencies of current element
				{
					//check all upper adjacencies that may be affected by modification of me
					adj_type & hc = m->HighConn(arr[el_num][it]);
					for(adj_type::size_type jt = 0; jt < hc.size(); ++jt)
					{
						if( !m->GetMarker(hc[jt],mod))
						{
							m->SetMarker(hc[jt],mod);
							arr[GetHandleElementNum(hc[jt])].push_back(hc[jt]);
						}
					}
					m->ComputeGeometricType(arr[el_num][it]);
				}
				else //update nodes for current cell
				{
					//remove me from old nodes
					adj_type & hc = m->HighConn(arr[el_num][it]);
					for(adj_iterator jt = hc.begin(); jt != hc.end(); ++jt) //iterate over my nodes
					{
						adj_type & lc = m->LowConn(*jt);
						adj_iterator kt = lc.begin(); 
						while(kt != lc.end()) //iterate over nodes's cells
						{
							if( (*kt) == arr[el_num][it] ) // if nodes's cell is equal to modified cell, then remove connection
							{
								kt = lc.erase(kt);
								break;
							}
							else kt++;
						}
					}
					hc.clear(); //disconnect cell from all of it's nodes
					if( !m->LowConn(arr[el_num][it]).empty() )
					{
						ElementArray<Node> newnodes(m);
						m->ComputeGeometricType(arr[el_num][it]);
						m->RestoreCellNodes(arr[el_num][it],newnodes); //find new nodes of the cell
						hc.insert(hc.end(),newnodes.begin(),newnodes.end()); //connect to new nodes
						//add me to my new nodes
						for(adj_iterator jt = hc.begin(); jt != hc.end(); ++jt)
							m->LowConn(*jt).push_back(arr[el_num][it]);
					}
				}
				m->RecomputeGeometricData(arr[el_num][it]);
				m->RemMarker(arr[el_num][it],mod);
			}
		}
		m->ReleaseMarker(mod);
		m->ReleaseMarker(mrk);
	}

	void Element::Connect(const HandleType * adjacent, INMOST_DATA_ENUM_TYPE num) const
	{
		assert( !(GetElementType() == EDGE && GetMeshLink()->LowConn(GetHandle()).size() > 2) ); // cannot add another node to edge
		Mesh * m = GetMeshLink();
		for(INMOST_DATA_ENUM_TYPE k = 0; k < num; k++)
		{
			assert( GetHandleElementType(adjacent[k]) == (GetElementType() >> 1) ); //only lower dimension elements can be connected
			assert( !(GetHandleElementType(adjacent[k]) == FACE && m->HighConn(adjacent[k]).size() > 1) ); // face already connected to two cells

			m->HighConn(adjacent[k]).push_back(GetHandle());
			//this may be dead slow
			//LowConn().push_back(adjacent[k]);
		}
		adj_type & lc = m->LowConn(GetHandle());
		//TODO:
		//for face have to find positions where to attach edges
		lc.insert(lc.end(),adjacent,adjacent+num);
		if( Element::GetGeometricDimension(m->GetGeometricType(GetHandle())) == 2 )
		{
			if( GetElementType() == CELL ) 
			{
				getAsCell()->FixEdgeOrder();
			}
			else if( GetElementType() == FACE ) 
			{
				if( getAsFace()->FixEdgeOrder() )
					getAsFace()->FixNormalOrientation();
			}
		}
		if( GetElementType() == CELL ) //update cell nodes
		{
			//remove me from old nodes
			adj_type & hc = m->HighConn(GetHandle());
			for(adj_type::size_type jt = 0; jt < hc.size(); ++jt) //iterate over my nodes
			{
				adj_type & ilc = m->LowConn(hc[jt]);
				adj_iterator kt = ilc.begin(); 
				while(kt != ilc.end()) //iterate over nodes's cells
				{
					if( (*kt) == GetHandle() ) // if nodes's cell is equal to modified cell, then remove connection
					{
						kt = ilc.erase(kt);
						break;
					}
					else kt++;
				}
			}
			hc.clear(); //disconnect cell from all of it's nodes
			if( !lc.empty() )
			{
				ElementArray<Node> newnodes(m);
				ComputeGeometricType();
				m->RestoreCellNodes(GetHandle(),newnodes); //find new nodes of the cell
				hc.insert(hc.end(),newnodes.begin(),newnodes.end()); //connect to new nodes
				//add me to my new nodes
				for(adj_type::size_type jt = 0; jt < hc.size(); ++jt) m->LowConn(hc[jt]).push_back(GetHandle());
			}
		}
		else ComputeGeometricType();
		UpdateGeometricData();
	}
}




#endif
