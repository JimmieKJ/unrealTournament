#ifndef _ssfInt64_h_
#define _ssfInt64_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfInt64
		{
		public:
			static const uint64 DataTypeId = 0x08; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			int64 Value;
			
			ssfInt64();
			ssfInt64(int64 value);
			ssfInt64( const ssfInt64 &other );
			~ssfInt64();
			const ssfInt64 & operator = ( const ssfInt64 &other );
			operator int64 () const;
			
			static auto_ptr<ssfInt64> Create( int64 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfInt64> & value );
			
		};
	};
	
#endif//_ssfInt64_h_ 