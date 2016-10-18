// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../../../ThirdParty/zlib/zlib-1.2.5/Inc/zlib.h"
#pragma comment( lib, "../../../ThirdParty/zlib/zlib-1.2.5/Lib/Win64/zlib_64.lib" )

// Taken from GenericPlatform.h

// Unsigned base types.
typedef unsigned char 		uint8;		// 8-bit  unsigned.
typedef unsigned short int	uint16;		// 16-bit unsigned.
typedef unsigned int		uint32;		// 32-bit unsigned.
typedef unsigned long long	uint64;		// 64-bit unsigned.

// Signed base types.
typedef	signed char			int8;		// 8-bit  signed.
typedef signed short int	int16;		// 16-bit signed.
typedef signed int	 		int32;		// 32-bit signed.
typedef signed long long	int64;		// 64-bit signed.

// Taken from Compression.cpp

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return  Less than zero values are error codes if the uncompress fails, greater than zero is the uncompressed size in bytes
 */
extern "C"
{

	__declspec(dllexport)
	int32 __cdecl UE4UncompressMemoryZLIB(void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize)
	{
		// Zlib wants to use unsigned long.
		unsigned long ZCompressedSize	= CompressedSize;
		unsigned long ZUncompressedSize	= UncompressedSize;
	
		// Uncompress data.
		int32 Result = uncompress((uint8*)UncompressedBuffer, &ZUncompressedSize, (const uint8*)CompressedBuffer, ZCompressedSize);

		// Sanity check to make sure we uncompressed as much data as we expected to.
		return (Result == Z_OK) ? (int32)ZUncompressedSize : Result;
	}

}