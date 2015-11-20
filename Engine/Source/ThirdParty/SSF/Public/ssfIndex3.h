#ifndef _ssfIndex3_h_
#define _ssfIndex3_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfIndex3 
		{
		public:
			static const uint64 DataTypeId = 0x0C0000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 3; // the number of dimensions of the vector, in this case 3D

			int32 V[num_dims]; // the vector data

			ssfIndex3();
			ssfIndex3( const ssfIndex3 &other );
			~ssfIndex3();
			const ssfIndex3 & operator = ( const ssfIndex3 &other );

			// value accessors
			int & A() { return V[0]; }
			int & B() { return V[1]; }
			int & C() { return V[2]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfIndex3> & value );	
		};
	}

#endif//_ssfIndex3_h_ 