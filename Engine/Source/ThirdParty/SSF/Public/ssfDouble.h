#ifndef _ssfDouble_h_
#define _ssfDouble_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfDouble
		{
		public:
			static const uint64 DataTypeId = 0x0B; // the type id of the data type
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size (TotalSizeInBytes will be a static class-method instead of per object)
		
			double Value;
			
			ssfDouble();
			ssfDouble(double value);
			ssfDouble( const ssfDouble &other );
			~ssfDouble();
			const ssfDouble & operator = ( const ssfDouble &other );
			operator double () const;
			
			static auto_ptr<ssfDouble> Create( double _value );
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s ) const;
			static uint64 TotalSizeInBytes();
			
			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfDouble> & value );
			
		};
	};
	
#endif//_ssfDouble_h_ 