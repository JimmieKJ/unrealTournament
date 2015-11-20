#ifndef _ssfVector4_h_
#define _ssfVector4_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfVector4 
		{
		public:
			static const uint64 DataTypeId = 0x040000;
			static const bool DataTypeIsConstantSize = true; // true if all items of this type is always the same size
			static const int num_dims = 4; // the number of dimensions of the vector, in this case 4D

			double V[num_dims]; // the vector data
		
			ssfVector4();
			ssfVector4( const ssfVector4 &other );
			~ssfVector4();
			const ssfVector4 & operator = ( const ssfVector4 &other );
		
			// value accessors
			double & X() { return V[0]; }
			double & Y() { return V[1]; }
			double & Z() { return V[2]; }
			double & W() { return V[3]; }

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			static uint64 TotalSizeInBytes();

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfVector4> & value );	
		};
	}

#endif//_ssfVector4_h_ 