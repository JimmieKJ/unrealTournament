#ifndef _ssfBinaryInputStream_h_
#define _ssfBinaryInputStream_h_

#include "ssfBaseDataType.h"

namespace ssf
	{
	class ssfString;

	class ssfBinaryInputStream 
		{
		public:
			ssfBinaryInputStream();
			~ssfBinaryInputStream();

			/// Reads the whole file into the binary input stream.
			void OpenFile( const ssfString &path ); 

			/// close the stream
			void CloseFile( bool check_crc_checksum = true );

			/** read a number of bytes from the input stream
				@param returns true on success
				*/
			void Read( void *dest , uint64 num_bytes );
		
			/** read words of data from the input stream
				the words are converted to the native endianness of the target platform
				@param dest the destination data area
				*/
			void Read1ByteWord( void *dest );
			void Read2ByteWord( void *dest );
			void Read4ByteWord( void *dest );
			void Read8ByteWord( void *dest );
			template<int word_size> void ReadNByteWord( void *dest );

			/** read a native type from the input stream.
				the items are converted to the native endianness of the target platform
				@returns the value read from the stream
				*/
			int8 ReadInt8();
			uint8 ReadUInt8();
			int16 ReadInt16();
			uint16 ReadUInt16();
			int32 ReadInt32();
			uint32 ReadUInt32();
			int64 ReadInt64();
			uint64 ReadUInt64();
			float ReadFloat();
			double ReadDouble();
			string ReadString();

			/** skips over a number of bytes in the stream. 
				this code still uses the bytes for checksums etc.
				*/
			bool SkipOverBytesInStream( uint64 num_bytes );

			/** get the current position in the binary stream
				*/
			uint64 GetCurrentPosition() const;
		
		private:
			template<int word_size> void ReadNBytes( void *dest );

			void *FileHandle; // the handle of the open file

			uint64 TotalSize; // the total size of the stream
			uint64 CurrPos; // the current position

			uint64 CacheStartPos; // the cache position, where the data cache starts
			uint64 CacheEndPos; // the cache position, where the data cache ends
			int8 *Data; // the cache data area

			void FillCacheFromFile();

			ssfCRC32 crc;
		};

	template<int num_bytes> void ssfBinaryInputStream::ReadNBytes( void *dest )
		{
		uint64 end_pos = this->CurrPos + num_bytes;
		if( end_pos >= CacheEndPos )
			{
			// the position is "complex", in that it runs across the cache line or
			// is too large for one cache line. let the regular Read handle it instead
			this->Read( dest , num_bytes );
			return;
			}
		else
			{
			// simple read from the source
			unsigned int data_pos = (unsigned int)( this->CurrPos - this->CacheStartPos );
			memcpy( dest , &this->Data[data_pos] , num_bytes );
			this->CurrPos = end_pos;
			}
		}

	template<int word_size> inline void ssfBinaryInputStream::ReadNByteWord( void *dest )
		{
		// copy to output variable, convert endianness if needed
		if( convert_endianness )
			{
			int8 *d = (int8*)dest;

			// read into temp variable
			int8 buf[word_size];
			this->ReadNBytes<word_size>(buf);

			// swap bytes, write to dest
			for( unsigned int i=0; i<word_size; ++i )
				{
				d[i] = buf[(word_size-1)-i];
				}
			}
		else
			{
			// no conversion, read directly to dest
			this->ReadNBytes<word_size>(dest);
			}
		}

	inline void ssfBinaryInputStream::Read1ByteWord(void *dest )
		{
		this->ReadNBytes<1>(dest);
		}

	inline void ssfBinaryInputStream::Read2ByteWord(void *dest )
		{
		this->ReadNByteWord<2>(dest);
		}

	inline void ssfBinaryInputStream::Read4ByteWord(void *dest )
		{
		this->ReadNByteWord<4>(dest);
		}

	inline void ssfBinaryInputStream::Read8ByteWord(void *dest )
		{
		this->ReadNByteWord<8>(dest);
		}

	inline int8 ssfBinaryInputStream::ReadInt8()
		{
		int8 value;
		this->Read1ByteWord( (void*) &value );
		return value;
		}

	inline uint8 ssfBinaryInputStream::ReadUInt8()
		{
		uint8 value;
		this->Read1ByteWord( (void*) &value );
		return value;
		}

	inline int16 ssfBinaryInputStream::ReadInt16()
		{
		int16 value;
		this->Read2ByteWord( (void*) &value );
		return value;
		}

	inline uint16 ssfBinaryInputStream::ReadUInt16()
		{
		uint16 value;
		this->Read2ByteWord( (void*) &value );
		return value;
		}

	inline int32 ssfBinaryInputStream::ReadInt32()
		{
		int32 value;
		this->Read4ByteWord( (void*) &value );
		return value;
		}

	inline uint32 ssfBinaryInputStream::ReadUInt32()
		{
		uint32 value;
		this->Read4ByteWord( (void*) &value );
		return value;
		}

	inline int64 ssfBinaryInputStream::ReadInt64()
		{
		int64 value;
		this->Read8ByteWord( (void*) &value );
		return value;
		}

	inline uint64 ssfBinaryInputStream::ReadUInt64()
		{
		uint64 value;
		this->Read8ByteWord( (void*) &value );
		return value;
		}

	inline float ssfBinaryInputStream::ReadFloat()
		{
		float value;
		this->Read4ByteWord( (void*) &value );
		return value;
		}

	inline double ssfBinaryInputStream::ReadDouble()
		{
		double value;
		this->Read8ByteWord( (void*) &value );
		return value;
		}

	inline string ssfBinaryInputStream::ReadString()
		{
		uint32 cnt = this->ReadUInt32();
	
		int8 *str = new int8[cnt+1];
		this->Read(str,cnt);
		str[cnt] = '\0';
		string ret(str);
		delete [] str;

		return ret;
		}

	inline uint64 ssfBinaryInputStream::GetCurrentPosition() const
		{
		return CurrPos;
		}

	};

#endif//_ssfBinaryInputStream_h_ 