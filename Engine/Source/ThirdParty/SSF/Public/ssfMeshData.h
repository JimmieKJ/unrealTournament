#ifndef _ssfMESHDATA_H_
#define _ssfMESHDATA_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfMesh;
	class ssfScene;
	class ssfChunkHeader;

	class ssfMeshData : public ssfChunkObject
		{
		public:
			/// The number of vertices in the MeshData
			ssfUInt32 GetVerticesCount() const;

			/// The number of triangles in the MeshData
			ssfUInt32 GetTrianglesCount() const;

			/// The number of triangle corners in the MeshData
			ssfUInt32 GetCornersCount() const;

			/// Per-vertex Coordinate data
			ssfAttribute< ssfList<ssfVector3> > Coordinates;

			/// Per-vertex skinning weights
			ssfAttribute< ssfTupleList<ssfDouble> > BoneWeights;

			/// Per-vertex skinning indices
			ssfAttribute< ssfTupleList<ssfInt32> > BoneIndices;

			/// Per-vertex vertex lock set
			ssfAttribute< ssfList<ssfBool> > VertexLocks;

			/// Per-vertex vertex weights
			ssfAttribute< ssfList<ssfDouble> > VertexWeights;

			/// Per-vertex vertex crease
			ssfAttribute< ssfList<ssfDouble> > VertexCrease;

			/// Original vertex indices
			ssfAttribute< ssfList<ssfInt32> > OriginalVertexIndices;

			/// Triangle vertex indices, one item per triangle
			ssfAttribute< ssfList<ssfIndex3> > TriangleIndices;

			/// Original triangle indices
			ssfAttribute< ssfList<ssfInt32> > OriginalTriangleIndices;

			/// Per-corner Normals data
			ssfAttribute< ssfList<ssfVector3> > Normals;

			/// Per-corner Tangent data
			ssfAttribute< ssfList<ssfVector3> > Tangents;

			/// Per-corner Bitangent data
			ssfAttribute< ssfList<ssfVector3> > Bitangents;

			/// Per-corner edge crease
			ssfAttribute< ssfList<ssfDouble> > EdgeCrease;

			/// Per-corner edge crease
			ssfAttribute< ssfList<ssfDouble> > EdgeWeights;

			/// Per-triangle smoothing group
			ssfAttribute< ssfList<ssfInt32> > SmoothingGroup;

			/// List of per-corner named texture coords sets
			list< ssfNamedList<ssfVector2> > TextureCoordinatesList;

			/// List of per-corner named color sets
			list< ssfNamedList<ssfVector4> > ColorsList;

			/// List of per-corner named blend shape sets
			list< ssfNamedList<ssfVector3> > BlendShapesList;

			/// Triangle material indices, one integer per triangle
			ssfAttribute< ssfList<ssfUInt32> > MaterialIndices;

			/// Named vertex selection sets
			list< ssfNamedList<ssfUInt32> > VertexSetList;

			/// Named edge selection sets
			list< ssfNamedList<ssfUInt32> > EdgeSetList;

			/// Named triangle selection sets
			list< ssfNamedList<ssfUInt32> > TriangleSetList;	

			/// Max deviation
			double MaxDeviation;

			/// Pixel size
			int PixelSize;

			/// ssfMeshData object constructor
			ssfMeshData();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfMesh &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfMeshData();
			friend class ssfCountedPointer<ssfMeshData>;
		};

	typedef ssfCountedPointer<ssfMeshData> pssfMeshData;

	};

#endif//_ssfMESHDATA_H_
