#ifndef _ssfBinaryOutputStream_h_
#define _ssfBinaryOutputStream_h_

#include "ssfBaseDataType.h"

namespace ssf
	{
	class ssfString;

	class ssfBinaryOutputStream 
		{
		public:
			ssfBinaryOutputStream();
			~ssfBinaryOutputStream();

			/** open a file for streaming
				*/
			bool OpenFile( const ssfString &path );

			/** close the opened file
				*/
			void CloseFile( bool write_crc_checksum = true );

			/** write a specific number of bytes to the output stream
				@param src source data buffer
				@param num_bytes number of bytes to write to the stream
				@param throw_on_fail if the write fails, throw an exception
				@param returns true on success
				*/
			bool Write( const void *src , uint64 num_bytes , bool throw_on_fail = false );
		
			/** write words of data to the output stream
				the words are converted to little endian, regardless on the platform
				@param src source data buffer
				@param throw_on_fail if the write fails, throw an exception
				@param returns true on success
				*/
			bool Write1ByteWord( const void *src , bool throw_on_fail = false );
			bool Write2ByteWord( const void *src , bool throw_on_fail = false );
			bool Write4ByteWord( const void *src , bool throw_on_fail = false );
			bool Write8ByteWord( const void *src , bool throw_on_fail = false );

			/** write a native type to the output stream
				the words are converted to little endian, regardless on the platform
				@param value the value 
				*/
			void WriteInt8( int8 value );
			void WriteUInt8( uint8 value );
			void WriteInt16( int16 value );
			void WriteUInt16( uint16 value );
			void WriteInt32( int32 value );
			void WriteUInt32( uint32 value );
			void WriteInt64( int64 value );
			void WriteUInt64( uint64 value );
			void WriteFloat( float value );
			void WriteDouble( double value );
			void WriteString( string value );

			/** get the number of bytes written to the stream, the current position in the stream
				*/
			uint64 GetCurrentPosition() const;

		private:
			template<int word_size> bool WriteNByteWord( const void *src , bool throw_on_fail );
			void FlushCacheToFile();

			uint64 CurrPos; // the current position

			uint64 CacheStartPos; // the cache position, where the data cache starts
			uint64 CacheEndPos; // the cache position, where the data cache ends
			int8 *Data; // the cache data area
			
			void *FileHandle;
			
			ssfCRC32 crc;

		};
	};


#endif//_ssfBinaryOutputStream_h_ 