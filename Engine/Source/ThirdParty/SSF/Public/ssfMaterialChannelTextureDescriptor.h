#ifndef _MATERIALCHANNELTEXTUREDESCRIPTOR_H_
#define _MATERIALCHANNELTEXTUREDESCRIPTOR_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfMaterialChannel;
	class ssfScene;
	class ssfChunkHeader;

	class ssfMaterialChannelTextureDescriptor : public ssfChunkObject
		{
		public:
			/// The texture ID used by the channel
			ssfAttribute< ssfString > TextureID;

			/// The texture coordinate used by the texture in the channel
			ssfAttribute< ssfString > TexCoordSet;

			/// ssfMaterialChannelTextureDescriptor object constructor
			ssfMaterialChannelTextureDescriptor();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfMaterialChannel &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfMaterialChannelTextureDescriptor();
			friend class ssfCountedPointer<ssfMaterialChannelTextureDescriptor>;
		};

	typedef ssfCountedPointer<ssfMaterialChannelTextureDescriptor> pssfMaterialChannelTextureDescriptor;

	};

#endif//_MATERIALCHANNELTEXTUREDESCRIPTOR_H_
