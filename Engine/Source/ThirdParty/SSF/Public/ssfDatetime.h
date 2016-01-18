#ifndef _ssfDatetime_h_
#define _ssfDatetime_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfDatetime 
		{
		public:
			static const uint64 DataTypeId = 0x010000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size

			uint64 Time; // Number of 100-nanosecond ticks since 12:00:00 midnight, January 1, 0001 UTC.
		
			ssfDatetime();
			ssfDatetime( const ssfDatetime &other );
			~ssfDatetime();
			const ssfDatetime & operator = ( const ssfDatetime &other );
		
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfDatetime> & value );
		};
	}


#endif//_ssfDatetime_h_ 