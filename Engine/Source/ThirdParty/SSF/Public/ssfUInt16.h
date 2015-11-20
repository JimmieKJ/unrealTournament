#ifndef _ssfUInt16_h_
#define _ssfUInt16_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfUInt16
		{
		public:
			static const uint64 DataTypeId = 0x05; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			uint16 Value;
			
			ssfUInt16();
			ssfUInt16(uint16 value);
			ssfUInt16( const ssfUInt16 &other );
			~ssfUInt16();
			const ssfUInt16 & operator = ( const ssfUInt16 &other );
			operator uint16 () const;
			
			static auto_ptr<ssfUInt16> Create( uint16 _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfUInt16> & value );
			
		};
	};
	
#endif//_ssfUInt16_h_ 