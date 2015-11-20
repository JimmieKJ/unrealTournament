#ifndef _ssfInt32_h_
#define _ssfInt32_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfInt32
		{
		public:
			static const uint64 DataTypeId = 0x06; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			int32 Value;
			
			ssfInt32();
			ssfInt32(int32 value);
			ssfInt32( const ssfInt32 &other );
			~ssfInt32();
			const ssfInt32 & operator = ( const ssfInt32 &other );
			operator int32 () const;
			
			static auto_ptr<ssfInt32> Create( int32 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfInt32> & value );
			
		};
	};
	
#endif//_ssfInt32_h_ 