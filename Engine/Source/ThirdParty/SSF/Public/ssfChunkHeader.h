#ifndef _ssfChunk_h_
#define _ssfChunk_h_

#include "ssfBaseDataType.h"

namespace ssf
	{
	class ssfBinaryInputStream;
	class ssfBinaryOutputStream;

	class ssfChunkHeader 
		{
		public:
			string Type;
			uint8 MajorVersion;
			uint8 MinorVersion;
			uint64 Offset; // offset from end of header to chunks (size of attributes)
			uint64 Length; // size of chunks 

			ssfChunkHeader();
			ssfChunkHeader( const ssfChunkHeader &other );
			const ssfChunkHeader & operator = ( const ssfChunkHeader &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			virtual uint64 HeaderSizeInBytes() const;

		};
	}

#endif//_ssfChunk_h_ 