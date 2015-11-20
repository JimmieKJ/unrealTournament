#ifndef _ssfHeader_h_
#define _ssfHeader_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfFileHeader 
		{
		public:
			uint8 MajorVersion;
			uint8 MinorVersion;
			string ToolName;
			uint8 ToolMajorVersion;
			uint8 ToolMinorVersion;
			uint16 ToolBuildVersion;
			uint64 FormatSignature;
			uint64 AttributesOffset;
			uint64 ChunksOffset;

			ssfFileHeader();
			ssfFileHeader( const ssfFileHeader &other );
			const ssfFileHeader & operator = ( const ssfFileHeader &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};
	}


#endif//_ssfHeader_h_ 