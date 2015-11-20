#ifndef _ssfFloat_h_
#define _ssfFloat_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfFloat
		{
		public:
			static const uint64 DataTypeId = 0x0A; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			float Value;
			
			ssfFloat();
			ssfFloat(float value);
			ssfFloat( const ssfFloat &other );
			~ssfFloat();
			const ssfFloat & operator = ( const ssfFloat &other );
			operator float () const;
			
			static auto_ptr<ssfFloat> Create( float _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfFloat> & value );
			
		};
	};
	
#endif//_ssfFloat_h_ 