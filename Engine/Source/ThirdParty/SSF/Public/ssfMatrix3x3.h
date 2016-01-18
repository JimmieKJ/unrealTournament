#ifndef _ssfMatrix3x3_h_
#define _ssfMatrix3x3_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfMatrix3x3 
		{
		public:
			static const uint64 DataTypeId = 0x080000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_rows = 3;
			static const int num_columns = 3;

			double M[num_rows][num_columns]; // matrix in row-major order
		
			ssfMatrix3x3();
			ssfMatrix3x3( const ssfMatrix3x3 &other );
			~ssfMatrix3x3();
			const ssfMatrix3x3 & operator = ( const ssfMatrix3x3 &other );
		
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfMatrix3x3> & value );	
		};
	}

#endif//_ssfMatrix3x3_h_ 