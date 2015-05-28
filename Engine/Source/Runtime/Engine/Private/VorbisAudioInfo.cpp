// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "VorbisAudioInfo.h"
#include "IAudioFormat.h"

#if WITH_OGGVORBIS

// hack to get ogg types right for HTML5. 
#if PLATFORM_HTML5 && !PLATFORM_HTML5_WIN32
#define _WIN32 
#define __MINGW32__
#endif 
#pragma pack(push, 8)
#include "ogg/ogg.h"
#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"
#pragma pack(pop)
#endif

#if PLATFORM_HTML5 && !PLATFORM_HTML5_WIN32
#undef  _WIN32 
#undef __MINGW32__
#endif 

#if PLATFORM_LITTLE_ENDIAN
#define VORBIS_BYTE_ORDER 0
#else
#define VORBIS_BYTE_ORDER 1
#endif

/**
 * Channel order expected for a multi-channel ogg vorbis file.
 * Ordering taken from http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
 */
namespace VorbisChannelInfo
{
	const int32 Order[8][8] = {
		{ 0 },
		{ 0, 1 },
		{ 0, 2, 1 },
		{ 0, 1, 2, 3 },
		{ 0, 2, 1, 3, 4 },
		{ 0, 2, 1, 4, 5, 3 },
		{ 0, 2, 1, 4, 5, 6, 3 },
		{ 0, 2, 1, 4, 5, 6, 7, 3 }
	};
};

/*------------------------------------------------------------------------------------
	FVorbisFileWrapper. Hides Vorbis structs from public headers.
------------------------------------------------------------------------------------*/
struct FVorbisFileWrapper
{
	FVorbisFileWrapper()
	{
#if WITH_OGGVORBIS
		FMemory::Memzero( &vf, sizeof( OggVorbis_File ) );
#endif
	}

	~FVorbisFileWrapper()
	{
#if WITH_OGGVORBIS
		ov_clear( &vf ); 
#endif
	}

#if WITH_OGGVORBIS
	/** Ogg vorbis decompression state */
	OggVorbis_File	vf;
#endif
};

#if WITH_OGGVORBIS
/*------------------------------------------------------------------------------------
	FVorbisAudioInfo.
------------------------------------------------------------------------------------*/
FVorbisAudioInfo::FVorbisAudioInfo( void ) 
	: VFWrapper(new FVorbisFileWrapper())
	,	SrcBufferData(NULL)
	,	SrcBufferDataSize(0)
	,	BufferOffset(0)
{ 
}

FVorbisAudioInfo::~FVorbisAudioInfo( void ) 
{ 
	check( VFWrapper != NULL );
	delete VFWrapper;
}

/** Emulate read from memory functionality */
size_t FVorbisAudioInfo::Read( void *Ptr, uint32 Size )
{
	size_t BytesToRead = FMath::Min( Size, SrcBufferDataSize - BufferOffset );
	FMemory::Memcpy( Ptr, SrcBufferData + BufferOffset, BytesToRead );
	BufferOffset += BytesToRead;
	return( BytesToRead );
}

static size_t OggRead( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Read( ptr, size * nmemb ) );
}

int FVorbisAudioInfo::Seek( uint32 offset, int whence )
{
	switch( whence )
	{
	case SEEK_SET:
		BufferOffset = offset;
		break;

	case SEEK_CUR:
		BufferOffset += offset;
		break;

	case SEEK_END:
		BufferOffset = SrcBufferDataSize - offset;
		break;
	}

	return( BufferOffset );
}

static int OggSeek( void *datasource, ogg_int64_t offset, int whence )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Seek( offset, whence ) );
}

int FVorbisAudioInfo::Close( void )
{
	return( 0 );
}

static int OggClose( void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Close() );
}

long FVorbisAudioInfo::Tell( void )
{
	return( BufferOffset );
}

static long OggTell( void *datasource )
{
	FVorbisAudioInfo *OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Tell() );
}

/** 
 * Reads the header information of an ogg vorbis file
 * 
 * @param	Resource		Info about vorbis data
 */
bool FVorbisAudioInfo::ReadCompressedInfo( const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo )
{
	SCOPE_CYCLE_COUNTER( STAT_VorbisPrepareDecompressionTime );

	check( VFWrapper != NULL );
	ov_callbacks		Callbacks;

	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	BufferOffset = 0;

	Callbacks.read_func = OggRead;
	Callbacks.seek_func = OggSeek;
	Callbacks.close_func = OggClose;
	Callbacks.tell_func = OggTell;

	// Set up the read from memory variables
	if (ov_open_callbacks(this, &VFWrapper->vf, NULL, 0, Callbacks) < 0)
	{
		return( false );
	}

	if( QualityInfo )
	{
		// The compression could have resampled the source to make loopable
		vorbis_info* vi = ov_info( &VFWrapper->vf, -1 );
		QualityInfo->SampleRate = vi->rate;
		QualityInfo->NumChannels = vi->channels;
		ogg_int64_t PCMTotal = ov_pcm_total( &VFWrapper->vf, -1 );
		if (PCMTotal >= 0)
		{
			QualityInfo->SampleDataSize = PCMTotal * QualityInfo->NumChannels * sizeof( int16 );
			QualityInfo->Duration = ( float )ov_time_total( &VFWrapper->vf, -1 );
		}
		else if (PCMTotal == OV_EINVAL)
		{
			// indicates an error or that the bitstream is non-seekable
			QualityInfo->SampleDataSize = 0;
			QualityInfo->Duration = 0.0f;
		}
	}

	return( true );
}


/** 
 * Decompress an entire ogg vorbis data file to a TArray
 */
void FVorbisAudioInfo::ExpandFile( uint8* DstBuffer, FSoundQualityInfo* QualityInfo )
{
	uint32		TotalBytesRead, BytesToRead;

	check( VFWrapper != NULL );
	check( DstBuffer );
	check( QualityInfo );

	// A zero buffer size means decompress the entire ogg vorbis stream to PCM.
	TotalBytesRead = 0;
	BytesToRead = QualityInfo->SampleDataSize;

	char* Destination = ( char* )DstBuffer;
	while( TotalBytesRead < BytesToRead )
	{
		long BytesRead = ov_read( &VFWrapper->vf, Destination, BytesToRead - TotalBytesRead, 0, 2, 1, NULL );

		if (BytesRead < 0)
		{
			// indicates an error - fill remainder of buffer with zero
			FMemory::Memzero(Destination, BytesToRead - TotalBytesRead);
			return;
		}

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}
}



/** 
 * Decompresses ogg vorbis data to raw PCM data. 
 * 
 * @param	PCMData		where to place the decompressed sound
 * @param	bLooping	whether to loop the wav by seeking to the start, or pad the buffer with zeroes
 * @param	BufferSize	number of bytes of PCM data to create. A value of 0 means decompress the entire sound.
 *
 * @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
 */
bool FVorbisAudioInfo::ReadCompressedData( uint8* InDestination, bool bLooping, uint32 BufferSize )
{
	bool		bLooped;
	uint32		TotalBytesRead;

	SCOPE_CYCLE_COUNTER( STAT_VorbisDecompressTime );

	bLooped = false;

	// Work out number of samples to read
	TotalBytesRead = 0;
	char* Destination = ( char* )InDestination;

	check( VFWrapper != NULL );

	while( TotalBytesRead < BufferSize )
	{
		long BytesRead = ov_read( &VFWrapper->vf, Destination, BufferSize - TotalBytesRead, 0, 2, 1, NULL );
		if( !BytesRead )
		{
			// We've reached the end
			bLooped = true;
			if( bLooping )
			{
				int Result = ov_pcm_seek_page( &VFWrapper->vf, 0 );
				if (Result < 0)
				{
					// indicates an error - fill remainder of buffer with zero
					FMemory::Memzero(Destination, BufferSize - TotalBytesRead);
					return true;
				}
			}
			else
			{
				int32 Count = ( BufferSize - TotalBytesRead );
				FMemory::Memzero( Destination, Count );

				BytesRead += BufferSize - TotalBytesRead;
			}
		}
		else if (BytesRead < 0)
		{
			// indicates an error - fill remainder of buffer with zero
			FMemory::Memzero(Destination, BufferSize - TotalBytesRead);
			return false;
		}

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}

	return( bLooped );
}

void FVorbisAudioInfo::SeekToTime( const float SeekTime )
{
	const float TargetTime = FMath::Min(SeekTime, ( float )ov_time_total( &VFWrapper->vf, -1 ));
	ov_time_seek( &VFWrapper->vf, TargetTime );
}

void FVorbisAudioInfo::EnableHalfRate( bool HalfRate )
{
	ov_halfrate( &VFWrapper->vf, int32(HalfRate));
}

void LoadVorbisLibraries()
{
	static bool bIsIntialized = false;
	if (!bIsIntialized)
	{
		bIsIntialized = true;
#if PLATFORM_WINDOWS  && WITH_OGGVORBIS
		//@todo if ogg is every ported to another platform, then use the platform abstraction to load these DLLs
		// Load the Ogg dlls
		FString VSVersion = TEXT("VS2013/");
		FString PlatformString = TEXT("Win32");
		FString DLLNameStub = TEXT(".dll");
#if PLATFORM_64BITS
		PlatformString = TEXT("Win64");
		DLLNameStub = TEXT("_64.dll");
#endif

		FString RootOggPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Ogg/") / PlatformString / VSVersion;
		FString RootVorbisPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Vorbis/") / PlatformString / VSVersion;

		FString DLLToLoad = RootOggPath + TEXT("libogg") + DLLNameStub;
		verifyf(LoadLibraryW(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
		// Load the Vorbis dlls
		DLLToLoad = RootVorbisPath + TEXT("libvorbis") + DLLNameStub;
		verifyf(LoadLibraryW(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
		DLLToLoad = RootVorbisPath + TEXT("libvorbisfile") + DLLNameStub;
		verifyf(LoadLibraryW(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
#endif	//PLATFORM_WINDOWS
	}
}

#endif		// WITH_OGGVORBIS

