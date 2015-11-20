#ifndef _ssfBool_h_
#define _ssfBool_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfBool
		{
		public:
			static const uint64 DataTypeId = 0x01; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			bool Value;
			
			ssfBool();
			ssfBool(bool value);
			ssfBool( const ssfBool &other );
			~ssfBool();
			const ssfBool & operator = ( const ssfBool &other );
			operator bool () const;
			
			static auto_ptr<ssfBool> Create( bool _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfBool> & value );
			
		};
	};
	
#endif//_ssfBool_h_ 