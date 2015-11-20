#ifndef _ssfIndex2_h_
#define _ssfIndex2_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfIndex2 
		{
		public:
			static const uint64 DataTypeId = 0x0B0000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 2; // the number of dimensions of the vector, in this case 2D

			int32 V[num_dims]; // the vector data

			ssfIndex2();
			ssfIndex2( const ssfIndex2 &other );
			~ssfIndex2();
			const ssfIndex2 & operator = ( const ssfIndex2 &other );

			// value accessors
			int & A() { return V[0]; }
			int & B() { return V[1]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfIndex2> & value );	
		};
	}

#endif//_ssfIndex2_h_ 