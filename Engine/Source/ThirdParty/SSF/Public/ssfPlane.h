#ifndef _ssfPlane_h_
#define _ssfPlane_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfPlane 
		{
		public:
			static const uint64 DataTypeId = 0x0A0000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size

			double A; 
			double B; 
			double C; 
			double D; 
		
			ssfPlane();
			ssfPlane( const ssfPlane &other );
			~ssfPlane();
			const ssfPlane & operator = ( const ssfPlane &other );
		
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfPlane> & value );			
		};
	}

#endif//_ssfPlane_h_ 