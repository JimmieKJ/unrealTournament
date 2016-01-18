#ifndef _ssfChunks_h_
#define _ssfChunks_h_

#include "ssfChunkHeader.h"

namespace ssf
	{
	class ssfAttributeHeader;

	class ssfChunkObject : public ssfObject
		{
		public:
			ssfChunkObject();

			// Desc: Reads an SSF file from an input stream. 
			// The whole file is read, with subchunks, and the checksum is checked from the file.
			void ReadFile( ssfBinaryInputStream *is ,
				ssfString &ToolName ,
				uint8 &ToolMajorVersion ,
				uint8 &ToolMinorVersion ,
				uint16 &ToolBuildVersion 				
				);

			// Desc: Writes an SSF file to an output stream.
			// The whole file is written, with subchunks, and the checksum is created for the file
			void WriteFile( ssfBinaryOutputStream *is ,
				ssfString ToolName ,
				uint8 ToolMajorVersion ,
				uint8 ToolMinorVersion ,
				uint16 ToolBuildVersion 				
				);

			///////////////////////////////////////////////////////////
			
			// Desc: Reads an SSF chunk from an input stream. This should only be used by the
			// SSF framework. Use ReadFile to read an SSF file from an input stream.
			void ReadChunk( ssfBinaryInputStream *s );

			// Desc: Sets up the chunk for writing, and returns sizes of the attributes
			// and sub chunks data. This should only be used by the SSF framework. Use 
			// WriteFile to write an SSF file to an output stream.
			void SetupWriteChunk( uint64 *AttributesSize , uint64 *ChildChunksSize );

			// Desc: Writes an SSF chunk to an output stream. The chunk and subchunks are written.
			// This should only be used by the SSF framework. Use WriteFile to write an SSF file to an output stream.
			void WriteChunk( const ssfChunkHeader *chunk , ssfBinaryOutputStream *s );

		protected:

			// Desc: Reads attributes from the input stream. This will call
			// Impl_ReadAttribute in the implementing class for all attributes found.
			void ReadAttributes( ssfBinaryInputStream *is );

			// Desc: Reads an attribute from the input stream, implemented by the 
			// implementing class. If an attribute is not read by the implementing class, it must 
			// return false, which will make the reader skip over the attribute. 
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is ) = 0;
			
			// Desc: Reads all sub-chunks for the chunk. This will call  
			// Impl_ReadChunk in the implementing class for all chunks found.
			void ReadChildChunks( ssfBinaryInputStream *is );

			// Desc: Reads a sub-chunk from the input stream. this is implemented by
			// the implementing class. If a chunk is not read by an implementing class, it
			// must return false from the method, so that the reader can skip over the chunk.
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is ) = 0;

			// Desc: Implemented by the implementation. Sets up the vector of output attributes
			virtual void Impl_SetupWriteAttributes() = 0;

			// Desc: Implemented by the implementation. Sets up the vector of output subchunks
			virtual void Impl_SetupWriteChildChunks() = 0;
			
			// Desc: Write attributes to the output stream. This will call
			// Impl_WriteAttributes in the implementing class for all attributes found.
			void WriteAttributes( ssfBinaryOutputStream *is );

			// Desc: Implemented by the implementation. Writes the attributes to the output stream
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s ) = 0;
	
			// Desc: Write subchunks to the output stream. This will call
			// Impl_WriteChildChunks in the implementing class for all attributes found.
			void WriteChildChunks( ssfBinaryOutputStream *is );

			// Desc: Implemented by the implementation. Writes the subchunks to the output stream
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s ) = 0;
			
			class ChunkWriteData
				{
				public:
					uint64 m_AttributesSize;
					uint64 m_ChildChunksSize;
					vector<ssfAttributeHeader> m_WriteAttributes;
					vector<ssfChunkHeader> m_WriteChildChunks;
					vector<uint64> m_WriteChildChunkSizes;
				};

			// Data that is used to set up the write attributes and chunks before streaming to file
			ChunkWriteData *WriteData;

			ssfChunkObject( const ssfChunkObject &other );
			~ssfChunkObject();
			const ssfChunkObject & operator = ( const ssfChunkObject &other );
			friend class ssfCountedPointer<ssfChunkObject>;
		};

	}

#endif//_ssfChunks_h_ 