#ifndef _TEXTURE_H_
#define _TEXTURE_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfTextureTable;
	class ssfScene;
	class ssfChunkHeader;

	class ssfTexture : public ssfChunkObject
		{
		public:
			/// The texture name
			ssfAttribute< ssfString > Name;

			/// The texture uuid
			ssfAttribute< ssfString > Id;

			/// The texture file path
			ssfAttribute< ssfString > Path;

			/// ssfTexture object constructor
			ssfTexture();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfTextureTable &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfTexture();
			friend class ssfCountedPointer<ssfTexture>;
		};

	typedef ssfCountedPointer<ssfTexture> pssfTexture;

	};

#endif//_TEXTURE_H_
