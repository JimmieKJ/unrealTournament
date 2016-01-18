#ifndef _MATERIAL_H_
#define _MATERIAL_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfMaterialChannel;
	class ssfMaterialTable;
	class ssfScene;
	class ssfChunkHeader;

	class ssfMaterial : public ssfChunkObject
		{
		public:
			/// The material name
			ssfAttribute< ssfString > Name;

			/// The material uuid
			ssfAttribute< ssfString > Id;

			/// The TangentSpaceNormals flag. If set, the material uses tangent-space based normals, rather than object space normals.
			ssfAttribute< ssfBool > TangentSpaceNormals;

			/// The list of materials channels in the material
			vector<ssfCountedPointer<ssfMaterialChannel>> MaterialChannelList;

			/// ssfMaterial object constructor
			ssfMaterial();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfMaterialTable &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfMaterial();
			friend class ssfCountedPointer<ssfMaterial>;
		};

	typedef ssfCountedPointer<ssfMaterial> pssfMaterial;

	};

#endif//_MATERIAL_H_
