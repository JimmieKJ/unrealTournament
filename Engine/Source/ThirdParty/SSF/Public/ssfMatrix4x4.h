#ifndef _ssfMatrix4x4_h_
#define _ssfMatrix4x4_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfMatrix4x4 
		{
		public:
			static const uint64 DataTypeId = 0x090000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_rows = 4;
			static const int num_columns = 4;

			double M[num_rows][num_columns]; // matrix in row-major order

			ssfMatrix4x4();
			ssfMatrix4x4( const ssfMatrix4x4 &other );
			~ssfMatrix4x4();
			const ssfMatrix4x4 & operator = ( const ssfMatrix4x4 &other );
		
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfMatrix4x4> & value );		
		};
	}

#endif//_ssfMatrix4x4_h_ 