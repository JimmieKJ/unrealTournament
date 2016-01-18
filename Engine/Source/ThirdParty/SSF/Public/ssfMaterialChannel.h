#ifndef _MATERIALCHANNEL_H_
#define _MATERIALCHANNEL_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfMaterialChannelTextureDescriptor;
	class ssfMaterial;
	class ssfScene;
	class ssfChunkHeader;

	class ssfMaterialChannel : public ssfChunkObject
		{
		public:
			/// The material channel name
			ssfAttribute< ssfString > ChannelName;

			/// The material channel base color
			ssfAttribute< ssfVector4 > Color;

			/// The material channel shading network
			ssfAttribute< ssfString > ShadingNetwork;

			/// The list of texture descriptors in the material channel
			vector<ssfCountedPointer<ssfMaterialChannelTextureDescriptor>> MaterialChannelTextureDescriptorList;

			/// ssfMaterialChannel object constructor
			ssfMaterialChannel();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfMaterial &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfMaterialChannel();
			friend class ssfCountedPointer<ssfMaterialChannel>;
		};

	typedef ssfCountedPointer<ssfMaterialChannel> pssfMaterialChannel;

	};

#endif//_MATERIALCHANNEL_H_
