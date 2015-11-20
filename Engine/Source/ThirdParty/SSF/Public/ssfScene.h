#ifndef _SCENE_H_
#define _SCENE_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfNodeTable;
	class ssfMeshTable;
	class ssfMaterialTable;
	class ssfTextureTable;
	class ssfScene;
	class ssfChunkHeader;

	class ssfScene : public ssfChunkObject
		{
		public:
			/// The World Up Vector (1-x up, 2-y up, 3-z up)
			ssfAttribute< ssfInt32 > WorldOrientation;

			/// The Coordinate System handedness (1-left, 2-right)
			ssfAttribute< ssfInt32 > CoordinateSystem;

			/// Named proxy group sets
			list< ssfNamedList<ssfString> > ProxyGroupSetsList;

			/// Named occluder group sets
			list< ssfNamedList<ssfString> > OccluderGroupSetsList;

			/// Named ID selection group sets
			list< ssfNamedIdList<ssfString> > SelectionGroupSetsList;

			/// The table of all nodes
			ssfCountedPointer<ssfNodeTable> NodeTable;

			/// The table of all mesh nodes
			ssfCountedPointer<ssfMeshTable> MeshTable;

			/// The table of all material nodes
			ssfCountedPointer<ssfMaterialTable> MaterialTable;

			/// The table of all textures
			ssfCountedPointer<ssfTextureTable> TextureTable;

			/// ssfScene object constructor
			ssfScene();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate();

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfScene();
			friend class ssfCountedPointer<ssfScene>;
		};

	typedef ssfCountedPointer<ssfScene> pssfScene;

	};

#endif//_SCENE_H_
