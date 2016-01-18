#ifndef _ssfUInt8_h_
#define _ssfUInt8_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfUInt8
		{
		public:
			static const uint64 DataTypeId = 0x03; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			uint8 Value;
			
			ssfUInt8();
			ssfUInt8(uint8 value);
			ssfUInt8( const ssfUInt8 &other );
			~ssfUInt8();
			const ssfUInt8 & operator = ( const ssfUInt8 &other );
			operator uint8 () const;
			
			static auto_ptr<ssfUInt8> Create( uint8 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfUInt8> & value );
			
		};
	};
	
#endif//_ssfUInt8_h_ 