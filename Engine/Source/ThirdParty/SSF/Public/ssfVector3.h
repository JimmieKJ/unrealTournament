#ifndef _ssfVector3_h_
#define _ssfVector3_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfVector3 
		{
		public:
			static const uint64 DataTypeId = 0x030000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 3; // the number of dimensions of the vector, in this case 3D

			double V[num_dims]; // the vector data

			ssfVector3();
			ssfVector3( const ssfVector3 &other );
			~ssfVector3();
			const ssfVector3 & operator = ( const ssfVector3 &other );

			// value accessors
			double & X() { return V[0]; }
			double & Y() { return V[1]; }
			double & Z() { return V[2]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfVector3> & value );		
		};
	}

#endif//_ssfVector3_h_ 