#ifndef _ssfUInt32_h_
#define _ssfUInt32_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfUInt32
		{
		public:
			static const uint64 DataTypeId = 0x07; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			uint32 Value;
			
			ssfUInt32();
			ssfUInt32(uint32 value);
			ssfUInt32( const ssfUInt32 &other );
			~ssfUInt32();
			const ssfUInt32 & operator = ( const ssfUInt32 &other );
			operator uint32 () const;
			
			static auto_ptr<ssfUInt32> Create( uint32 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfUInt32> & value );
			
		};
	};
	
#endif//_ssfUInt32_h_ 