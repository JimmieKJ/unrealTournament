#ifndef _ssfCRC32_h_
#define _ssfCRC32_h_

#include "ssfBaseDataType.h"

namespace ssf
	{
	class ssfCRC32
		{
		private:
			uint32 crc;

		public:
			ssfCRC32();

			// add the bytes to the CRC32 checksum
			void AccumulateCRC32( const uint8 *p, size_t size );

			// return the checksum
			uint32 GetAccumulatedCRC32();
		};
	}

#endif//_ssfCRC32_h_