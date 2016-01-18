#ifndef _NODETABLE_H_
#define _NODETABLE_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfNode;
	class ssfScene;
	class ssfScene;
	class ssfChunkHeader;

	class ssfNodeTable : public ssfChunkObject
		{
		public:
			/// The list of nodes in the node table
			vector<ssfCountedPointer<ssfNode>> NodeList;

			/// ssfNodeTable object constructor
			ssfNodeTable();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfScene &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfNodeTable();
			friend class ssfCountedPointer<ssfNodeTable>;
		};

	typedef ssfCountedPointer<ssfNodeTable> pssfNodeTable;

	};

#endif//_NODETABLE_H_
