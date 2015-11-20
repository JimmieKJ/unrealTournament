#ifndef _ssfInt8_h_
#define _ssfInt8_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfInt8
		{
		public:
			static const uint64 DataTypeId = 0x02; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			int8 Value;
			
			ssfInt8();
			ssfInt8(int8 value);
			ssfInt8( const ssfInt8 &other );
			~ssfInt8();
			const ssfInt8 & operator = ( const ssfInt8 &other );
			operator int8 () const;
			
			static auto_ptr<ssfInt8> Create( int8 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfInt8> & value );
			
		};
	};
	
#endif//_ssfInt8_h_ 