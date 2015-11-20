#ifndef _ssfUInt64_h_
#define _ssfUInt64_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfUInt64
		{
		public:
			static const uint64 DataTypeId = 0x09; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			uint64 Value;
			
			ssfUInt64();
			ssfUInt64(uint64 value);
			ssfUInt64( const ssfUInt64 &other );
			~ssfUInt64();
			const ssfUInt64 & operator = ( const ssfUInt64 &other );
			operator uint64 () const;
			
			static auto_ptr<ssfUInt64> Create( uint64 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfUInt64> & value );
			
		};
	};
	
#endif//_ssfUInt64_h_ 