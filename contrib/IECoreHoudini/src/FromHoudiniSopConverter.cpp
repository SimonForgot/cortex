//////////////////////////////////////////////////////////////////////////
//
//  Copyright 2010 Dr D Studios Pty Limited (ACN 127 184 954) (Dr. D Studios),
//  its affiliates and/or its licensors.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "FromHoudiniSopConverter.h"

// Houdini
#include <OP/OP_Director.h>

// Boost
#include <boost/python.hpp>
using namespace boost::python;

// Cortex
#include <IECore/MeshPrimitive.h>
#include <IECore/PointsPrimitive.h>
#include <IECorePython/ScopedGILLock.h>
using namespace IECore;

// CoreHoudini
#include "CoreHoudini.h"
#include "SOP_ParameterisedHolder.h"
using namespace IECoreHoudini;

// ctor
FromHoudiniSopConverter::FromHoudiniSopConverter( HOM_SopNode *hou_sop ) :
	ToCoreConverter( "Converts Houdini SOP geometry to IECore::MeshPrimitive "
			"or IECore::PointsPrimitive objects." ),
	m_sop(hou_sop)
{
}

// dtor
FromHoudiniSopConverter::~FromHoudiniSopConverter()
{
}

// convert a hou sop to a regular sop
SOP_Node *FromHoudiniSopConverter::getSop() const
{
	SOP_Node *sop = 0;
	if ( !m_sop )
		return sop;

	// get the hom path and use opdirector to get a regular OP_Node* to our node
	try
	{
		std::string node_path = m_sop->path();
		OP_Node *op = OPgetDirector()->findNode( node_path.c_str() );
		if ( op )
			sop = op->castToSOPNode();
	}
	catch( HOM_ObjectWasDeleted )
	{
	}
	return sop;
}

// Convert our geometry to cortex
ObjectPtr FromHoudiniSopConverter::doConversion(
		ConstCompoundObjectPtr operands ) const
{
	// find global time
	float time = CoreHoudini::currTime();

	// create our work context
	OP_Context context;
	context.setTime( time );

	// get our sop
	SOP_Node *sop = getSop();
	if( !sop )
		return 0;

	// get our geometry
	const GU_Detail *m_geo = sop->getCookedGeo( context );
	if ( !m_geo )
		return 0;

	// do some conversion!
	ObjectPtr return_obj = 0;
	const GEO_PointList &points = m_geo->points();
	const GEO_PrimList &prims = m_geo->primitives();
	int npoints = points.entries();
	int nprims = 0; // TODO: support meshes!
	int nverts = 0; // TODO: support meshes!

	// get the bbox
	UT_BoundingBox bbox;
	m_geo->getBBox( &bbox );
	Imath::Box3f box( Imath::V3f(bbox.xmin(), bbox.ymin(), bbox.zmin()),
			Imath::V3f(bbox.xmax(), bbox.ymax(), bbox.zmax()) );

	if ( nprims>0 )
	{
		// TODO: support meshes!
		// looks like a mesh
		return_obj = new MeshPrimitive();
		// getAttribInfo( m_geo, primitive_attribs, PRIMITIVE, attr_names, attr_data, attr_interp, nprims );
		// getAttribInfo( m_geo, primitive_attribs, VERTEX, attr_names, attr_data, attr_interp, nvertes );
		// extractPrimVertexAttribs( m_geo, prims, names, data, attr_entries, attr_types, attr_offset );
	}
	else
	{
		// just points
		return_obj = new PointsPrimitive(npoints);
		PointsPrimitivePtr result =	runTimeCast<PointsPrimitive>( return_obj );

		// add position
		std::vector<Imath::V3f> p_data(npoints);
		int index = 0;
		for ( const GEO_Point *curr = points.head(); curr!=0;
				curr=points.next(curr) )
		{
			const UT_Vector4 &pos = curr->getPos();
			p_data[index++] = Imath::V3f( pos[0], pos[1], pos[2] );
		}
		result->variables["P"] = PrimitiveVariable(
				PrimitiveVariable::Vertex,
				new V3fVectorData( p_data ) );

		// get other point attribute names
		const GEO_PointAttribDict &point_attribs = m_geo->pointAttribs();
		const GEO_PrimAttribDict &primitive_attribs = m_geo->primitiveAttribs();
		const GEO_VertexAttribDict &vertex_attribs = m_geo->vertexAttribs();
		const GB_AttributeTable &detail_attribs = m_geo->attribs();

		// our names, data and interpolation scheme
		std::vector<AttributeInfo> info;

		// point attributes
		getAttribInfo( m_geo, &point_attribs, PrimitiveVariable::Vertex, info,
				npoints );

		// detail attributes
		getAttribInfo( m_geo, &detail_attribs, PrimitiveVariable::Constant,
				info, 1 );

		// extract data from SOP attributes
		extractPointAttribs( m_geo, points, info );
		extractDetailAttribs( m_geo, info );

		// add the attributes to our PointsPrimitive
		for ( unsigned int attr_index=0; attr_index<info.size();
				++attr_index )
		{
			result->variables[info[attr_index].name] = PrimitiveVariable(
					info[attr_index].interp, info[attr_index].data );
		}
	}
	return return_obj;
}

// gathers information and allocates memory for storage of attributes
void FromHoudiniSopConverter::getAttribInfo( const GU_Detail *geo,
		const UT_LinkList *attribs,
		PrimitiveVariable::Interpolation interp_type,
		std::vector<AttributeInfo> &info,
		int num_entries ) const
{
	// temp storage for attributes of this class
	bool valid = true;

	// extract all the attributes of the desired class
	for( UT_LinkNode *curr=attribs->head(); curr!=0;
			curr=attribs->next(curr) )
	{
		GB_Attribute *attr = dynamic_cast<GB_Attribute*>(curr);
		if ( !attr )
			continue;

		DataPtr d_ptr = 0;
		int len = 0;
		switch( attr->getType() )
		{
			case GB_ATTRIB_FLOAT:
			{
				len = attr->getSize() / sizeof( float );
				switch( len )
				{
					case 1: // float
					{
						FloatVectorDataPtr data = new FloatVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					case 2: // V2f
					{
						V2fVectorDataPtr data = new V2fVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					case 3: // V3f
					{
						V3fVectorDataPtr data = new V3fVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					default:
					{
						valid = false;
						break;
					}
				}
				break;
			}
			case GB_ATTRIB_INT:
			{
				len = attr->getSize() / sizeof( int );
				switch( len )
				{
					case 1: // int
					{
						IntVectorDataPtr data = new IntVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					case 2: // V2i
					{
						V2iVectorDataPtr data = new V2iVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					case 3: // V3i
					{
						V3iVectorDataPtr data = new V3iVectorData();
						data->writable().resize(num_entries);
						d_ptr = data;
						break;
					}
					default:
					{
						valid = false;
						break;
					}
				}
				break;
			}
			case GB_ATTRIB_VECTOR:
			{
				len = attr->getSize() / (sizeof(float ) * 3);
				if ( len>1 ) // only support single element vectors
				{
					valid = false;
					break;
				}
				V3fVectorDataPtr data = new V3fVectorData();
				data->writable().resize(len * num_entries);
				d_ptr = data;
				break;
			}
			default:
				valid = false;
				break;
		}

		// build an info object describing our attribute
		if ( valid )
		{
			AttributeInfo inf;
			inf.name = std::string( attr->getName() );
			inf.data = d_ptr;
			inf.interp = interp_type;
			inf.entries = len;
			inf.type = attr->getType();
			switch( interp_type )
			{
				case PrimitiveVariable::Vertex:
					inf.offset = geo->findPointAttrib( attr );
					break;
				case PrimitiveVariable::Constant:
					inf.offset = geo->findAttrib( attr );
					break;
				default:
					inf.offset = -1;
					break;
			}
			info.push_back( inf );
		}
	}
}

// extracts point attributes from the sop into the pre-allocated storage
void FromHoudiniSopConverter::extractPointAttribs( const GU_Detail *geo,
		const GEO_PointList &points,
		std::vector<AttributeInfo> &info
		) const
{
	// loop over points getting P and other data
	unsigned int index = 0;
	for ( const GEO_Point *curr = points.head(); curr!=0;
			curr=points.next(curr) )
	{
		for ( unsigned int attr_index=0; attr_index<info.size();
				++attr_index )
		{
			if ( info[attr_index].interp!=PrimitiveVariable::Vertex ||
					info[attr_index].offset==-1 )
				continue;

			int len = info[attr_index].entries;
			switch( info[attr_index].type )
			{
				case GB_ATTRIB_FLOAT:
				{
					const float *ptr = curr->castAttribData<float>(info[attr_index].offset);
					switch( len )
					{
						case 1:
							runTimeCast<FloatVectorData>(info[attr_index].data)->writable()[index] = ptr[0];
							break;
						case 2:
							runTimeCast<V2fVectorData>(info[attr_index].data)->writable()[index] = Imath::V2f( ptr[0], ptr[1] );
							break;
						case 3:
							runTimeCast<V3fVectorData>(info[attr_index].data)->writable()[index] = Imath::V3f( ptr[0], ptr[1], ptr[2] );
							break;
						default:
							break;
					}
					break;
				}
				case GB_ATTRIB_INT:
				{
					const int *ptr = curr->castAttribData<int>(info[attr_index].offset);
					switch( len )
					{
						case 1:
							runTimeCast<IntVectorData>(info[attr_index].data)->writable()[index] = ptr[0];
							break;
						case 2:
							runTimeCast<V2iVectorData>(info[attr_index].data)->writable()[index] = Imath::V2i( ptr[0], ptr[1] );
							break;
						case 3:
							runTimeCast<V3iVectorData>(info[attr_index].data)->writable()[index] = Imath::V3i( ptr[0], ptr[1], ptr[2] );
							break;
						default:
							break;
					}
					break;
				}
				case GB_ATTRIB_VECTOR:
				{
					const float *ptr = curr->castAttribData<float>(info[attr_index].offset);
					runTimeCast<V3fVectorData>(info[attr_index].data)->writable()[index] = Imath::V3f( ptr[0], ptr[1], ptr[2] );
					break;
				}
				default:
					break;
			}
		}
		++index;
	}
}

// extracts detail attributes from the sop into the pre-allocated storage
void FromHoudiniSopConverter::extractDetailAttribs( const GU_Detail *geo,
		std::vector<AttributeInfo> &info
		) const
{
	for ( unsigned int attr_index=0; attr_index<info.size();
			++attr_index )
	{
		if ( info[attr_index].interp!=PrimitiveVariable::Constant ||
				info[attr_index].offset==-1 )
			continue;
		int len = info[attr_index].entries;
		const GB_AttributeTable &attrs = geo->attribs();

		switch( info[attr_index].type )
		{
			case GB_ATTRIB_FLOAT:
			{
				const float *ptr = attrs.castAttribData<float>(info[attr_index].offset);
				switch( len )
				{
					case 1:
						runTimeCast<FloatVectorData>(info[attr_index].data)->writable()[0] = ptr[0];
						break;
					case 2:
						runTimeCast<V2fVectorData>(info[attr_index].data)->writable()[0] = Imath::V2f( ptr[0], ptr[1] );
						break;
					case 3:
						runTimeCast<V3fVectorData>(info[attr_index].data)->writable()[0] = Imath::V3f( ptr[0], ptr[1], ptr[2] );
						break;
					default:
						break;
				}
				break;
			}
			case GB_ATTRIB_INT:
			{
				const int *ptr = attrs.castAttribData<int>(info[attr_index].offset);
				switch( len )
				{
					case 1:
						runTimeCast<IntVectorData>(info[attr_index].data)->writable()[0] = ptr[0];
						break;
					case 2:
						runTimeCast<V2iVectorData>(info[attr_index].data)->writable()[0] = Imath::V2i( ptr[0], ptr[1] );
						break;
					case 3:
						runTimeCast<V3iVectorData>(info[attr_index].data)->writable()[0] = Imath::V3i( ptr[0], ptr[1], ptr[2] );
						break;
					default:
						break;
				}
				break;
			}
			case GB_ATTRIB_VECTOR:
			{
				const float *ptr = attrs.castAttribData<float>(info[attr_index].offset);
				runTimeCast<V3fVectorData>(info[attr_index].data)->writable()[0] = Imath::V3f( ptr[0], ptr[1], ptr[2] );
				break;
			}
			default:
				break;
		}
	}
}