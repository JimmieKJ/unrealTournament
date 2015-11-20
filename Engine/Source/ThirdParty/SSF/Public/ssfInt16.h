#ifndef _ssfInt16_h_
#define _ssfInt16_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfInt16
		{
		public:
			static const uint64 DataTypeId = 0x04; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			int16 Value;
			
			ssfInt16();
			ssfInt16(int16 value);
			ssfInt16( const ssfInt16 &other );
			~ssfInt16();
			const ssfInt16 & operator = ( const ssfInt16 &other );
			operator int16 () const;
			
			static auto_ptr<ssfInt16> Create( int16 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfInt16> & value );
			
		};
	};
	
#endif//_ssfInt16_h_ 