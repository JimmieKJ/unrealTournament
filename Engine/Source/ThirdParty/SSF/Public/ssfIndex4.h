#ifndef _ssfIndex4_h_
#define _ssfIndex4_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfIndex4 
		{
		public:
			static const uint64 DataTypeId = 0x0D0000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 4; // the number of dimensions of the vector, in this case 4D

			int32 V[num_dims]; // the vector data

			ssfIndex4();
			ssfIndex4( const ssfIndex4 &other );
			~ssfIndex4();
			const ssfIndex4 & operator = ( const ssfIndex4 &other );

			// value accessors
			int & A() { return V[0]; }
			int & B() { return V[1]; }
			int & C() { return V[2]; }
			int & D() { return V[3]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfIndex4> & value );	
		};
	}

#endif//_ssfIndex4_h_ 