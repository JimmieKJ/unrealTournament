// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VorbisAudioInfo.h: Unreal audio vorbis decompression interface object.
=============================================================================*/

#pragma once

#include "AudioDecompress.h"

/**
 * Whether to use OggVorbis audio format.
 **/
#ifndef WITH_OGGVORBIS
	#if PLATFORM_DESKTOP
		#define WITH_OGGVORBIS 1
	#else
		#define WITH_OGGVORBIS 0
	#endif
#endif

namespace VorbisChannelInfo
{
	extern ENGINE_API const int32 Order[8][8];
}

/**
 * Loads vorbis dlls
*/
ENGINE_API void LoadVorbisLibraries();

#if WITH_OGGVORBIS
/** 
 * Helper class to parse ogg vorbis data
 */
class FVorbisAudioInfo : public ICompressedAudioInfo
{
public:
	ENGINE_API FVorbisAudioInfo( void );
	ENGINE_API virtual ~FVorbisAudioInfo( void );

	/** Emulate read from memory functionality */
	size_t			Read( void *ptr, uint32 size );
	int				Seek( uint32 offset, int whence );
	int				Close( void );
	long			Tell( void );

	/** 
	 * Reads the header information of an ogg vorbis file
	 * 
	 * @param	Resource		Info about vorbis data
	 */
	ENGINE_API virtual bool ReadCompressedInfo( const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo ) override;

	/** 
	 * Decompresses ogg data to raw PCM data. 
	 * 
	 * @param	Destination	where to place the decompressed sound
	 * @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	 * @param	BufferSize	number of bytes of PCM data to create
	 *
	 * @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
	 */
	ENGINE_API virtual bool ReadCompressedData( uint8* Destination, bool bLooping, uint32 BufferSize ) override;

	ENGINE_API virtual void SeekToTime( const float SeekTime ) override;

	/** 
	 * Decompress an entire ogg data file to a TArray
	 */
	ENGINE_API virtual void ExpandFile( uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo ) override;

	/** 
	 * Sets ogg to decode to half-rate
	 * 
	 * @param	Resource		Info about vorbis data
	 */
	ENGINE_API virtual void EnableHalfRate( bool HalfRate ) override;

	virtual uint32 GetSourceBufferSize() const override { return SrcBufferDataSize; }

	virtual bool UsesVorbisChannelOrdering() const override { return true; }
	virtual int GetStreamBufferSize() const override { return MONO_PCM_BUFFER_SIZE; }

	struct FVorbisFileWrapper* VFWrapper;
	const uint8*		SrcBufferData;
	uint32			SrcBufferDataSize;
	uint32			BufferOffset;

	FThreadSafeBool bPerformingOperation;

	/** Critical section used to prevent multiple threads accessing same ogg-vorbis file handles at the same time */
	FCriticalSection VorbisCriticalSection;
};
#endif
