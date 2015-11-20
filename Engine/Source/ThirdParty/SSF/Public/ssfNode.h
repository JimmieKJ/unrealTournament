#ifndef _NODE_H_
#define _NODE_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfNodeTable;
	class ssfScene;
	class ssfChunkHeader;

	class ssfNode : public ssfChunkObject
		{
		public:
			/// The node name
			ssfAttribute< ssfString > Name;

			/// The node uuid
			ssfAttribute< ssfString > Id;

			/// The mesh uuid
			ssfAttribute< ssfString > MeshId;

			/// The parent node uuid
			ssfAttribute< ssfString > ParentId;

			/// The node replacement uuid
			ssfAttribute< ssfString > ReplacementId;

			/// The local transform of the node
			ssfAttribute< ssfMatrix4x4 > LocalTransform;

			/// Flag, if set this node is used for HLOD replacement
			ssfAttribute< ssfBool > IsHLODReplacementNode;

			/// Size of on-screen size were this is used as an HLOD
			ssfAttribute< ssfUInt32 > HLODOnScreenSize;

			/// Flag, if set, this node is a current HLOD replacement
			ssfAttribute< ssfBool > IsCurrentReplacement;

			/// The IsFrozen flag. If set, if set, this node will be untouched
			ssfAttribute< ssfBool > IsFrozen;

			/// ssfNode object constructor
			ssfNode();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfNodeTable &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfNode();
			friend class ssfCountedPointer<ssfNode>;
		};

	typedef ssfCountedPointer<ssfNode> pssfNode;

	};

#endif//_NODE_H_
