#ifndef _ssfMESH_H_
#define _ssfMESH_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfMeshData;
	class ssfMeshTable;
	class ssfScene;
	class ssfChunkHeader;

	class ssfMesh : public ssfChunkObject
		{
		public:
			/// The number of materials in the Mesh
			ssfUInt32 GetMaterialsCount() const;

			/// The number of bones used by the Mesh
			ssfUInt32 GetBonesCount() const;

			/// The mesh name
			ssfAttribute< ssfString > Name;

			/// The mesh uuid
			ssfAttribute< ssfString > Id;

			/// Material uuid list for materials used by the mesh
			ssfAttribute< ssfList<ssfString> > MaterialIds;

			/// Bone node uuid list for bones used by the mesh
			ssfAttribute< ssfList<ssfString> > BoneIds;

			/// The list of MeshData objects in the mesh
			vector<ssfCountedPointer<ssfMeshData>> MeshDataList;

			/// ssfMesh object constructor
			ssfMesh();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfMeshTable &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfMesh();
			friend class ssfCountedPointer<ssfMesh>;
		};

	typedef ssfCountedPointer<ssfMesh> pssfMesh;

	};

#endif//_ssfMESH_H_
