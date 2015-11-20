#ifndef _ssfVector2_h_
#define _ssfVector2_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfVector2 
		{
		public:
			static const uint64 DataTypeId = 0x020000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 2; // the number of dimensions of the vector, in this case 2D

			double V[num_dims]; // the vector data

			ssfVector2();
			ssfVector2( const ssfVector2 &other );
			~ssfVector2();
			const ssfVector2 & operator = ( const ssfVector2 &other );

			// value accessors
			double & X() { return V[0]; }
			double & Y() { return V[1]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfVector2> & value );		
		};
	}

#endif//_ssfVector2_h_ 