// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioDecompress.h: Unreal audio vorbis decompression interface object.
=============================================================================*/

#pragma once

#include "Sound/SoundWave.h"

// 186ms of 44.1KHz data
// 372ms of 22KHz data
#define MONO_PCM_BUFFER_SAMPLES		8192
#define MONO_PCM_BUFFER_SIZE		( MONO_PCM_BUFFER_SAMPLES * sizeof( int16 ) )

/**
 * Interface class to decompress various types of audio data
 */
class ICompressedAudioInfo
{
public:
	/**
	* Virtual destructor.
	*/
	virtual ~ICompressedAudioInfo() { }

	/**
	* Reads the header information of a compressed format
	*
	* @param	InSrcBufferData		Source compressed data
	* @param	InSrcBufferDataSize	Size of compressed data
	* @param	QualityInfo			Quality Info (to be filled out)
	*/
	ENGINE_API virtual bool ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo) = 0;

	/**
	* Decompresses data to raw PCM data.
	*
	* @param	Destination	where to place the decompressed sound
	* @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	* @param	BufferSize	number of bytes of PCM data to create
	*
	* @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
	*/
	ENGINE_API virtual bool ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) = 0;

	/**
	 * Seeks to time (Some formats might not be seekable)
	 */
	virtual void SeekToTime(const float SeekTime) = 0;

	/**
	* Decompress an entire data file to a TArray
	*/
	ENGINE_API virtual void ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo) = 0;

	/**
	* Sets decode to half-rate
	*
	* @param	HalfRate	Whether Half rate is enabled
	*/
	ENGINE_API virtual void EnableHalfRate(bool HalfRate) = 0;

	/**
	 * Gets the size of the source buffer originally passed to the info class (bytes)
	 */
	virtual uint32 GetSourceBufferSize() const = 0;

	/**
	 * Whether the decompressed audio will be arranged using Vorbis' channel ordering
	 * See http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9 for details
	 */
	virtual bool UsesVorbisChannelOrdering() const = 0;

	/**
	* Gets the preferred size for a streaming buffer for this decompression scheme
	*/
	virtual int GetStreamBufferSize() const = 0;

	////////////////////////////////////////////////////////////////
	// Following functions are optional if streaming is supported //
	////////////////////////////////////////////////////////////////

	/**
	 * Whether this decompression class supports streaming decompression
	 */
	virtual bool SupportsStreaming() const {return false;}

	/**
	* Streams the header information of a compressed format
	*
	* @param	Wave			Wave that will be read from to retrieve necessary chunk
	* @param	QualityInfo		Quality Info (to be filled out)
	*/
	virtual bool StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo) {return false;}

	/**
	* Decompresses streamed data to raw PCM data.
	*
	* @param	Destination	where to place the decompressed sound
	* @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	* @param	BufferSize	number of bytes of PCM data to create
	*
	* @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
	*/
	virtual bool StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) {return false;}

	/**
	 * Gets the chunk index that was last read from (for Streaming Manager requests)
	 */
	virtual int32 GetCurrentChunkIndex() const {return -1;}

	/**
	 * Gets the offset into the chunk that was last read to (for Streaming Manager priority)
	 */
	virtual int32 GetCurrentChunkOffset() const {return -1;}
};


/**
 * Asynchronous audio decompression
 */
class FAsyncAudioDecompressWorker : public FNonAbandonableTask
{
protected:
	class USoundWave*		Wave;

	ICompressedAudioInfo*	AudioInfo;

public:
	/**
	 * Async decompression of audio data
	 *
	 * @param	InWave		Wave data to decompress
	 */
	ENGINE_API FAsyncAudioDecompressWorker(USoundWave* InWave);

	/**
	 * Performs the async audio decompression
	 */
	ENGINE_API void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncAudioDecompressWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

typedef FAsyncTask<FAsyncAudioDecompressWorker> FAsyncAudioDecompress;

enum class ERealtimeAudioTaskType : uint8
{
	/** Parses the wave compressed asset header file info */
	CompressedInfo,

	/** Decompresses a chunk */
	Decompress,

	/** Processes a procedural buffer to generate more audio */
	Procedural
};

template<class T>
class FAsyncRealtimeAudioTaskWorker : public FNonAbandonableTask
{
protected:
	T* AudioBuffer;
	USoundWave* WaveData;
	uint8* AudioData;
	int32 MaxSamples;
	int32 BytesWritten;
	ERealtimeAudioTaskType TaskType;
	uint32 bSkipFirstBuffer:1;
	uint32 bLoopingMode:1;
	uint32 bLooped:1;

public:
	FAsyncRealtimeAudioTaskWorker(T* InAudioBuffer, USoundWave* InWaveData)
		: AudioBuffer(InAudioBuffer)
		, WaveData(InWaveData)
		, AudioData(nullptr)
		, MaxSamples(0)
		, BytesWritten(0)
		, TaskType(ERealtimeAudioTaskType::CompressedInfo)
		, bSkipFirstBuffer(false)
		, bLoopingMode(false)
		, bLooped(false)
	{
		check(AudioBuffer && AudioBuffer->DecompressionState);
		check(WaveData);
	}

	FAsyncRealtimeAudioTaskWorker(T* InAudioBuffer, uint8* InAudioData, bool bInLoopingMode, bool bInSkipFirstBuffer)
		: AudioBuffer(InAudioBuffer)
		, AudioData(InAudioData)
		, TaskType(ERealtimeAudioTaskType::Decompress)
		, bSkipFirstBuffer(bInSkipFirstBuffer)
		, bLoopingMode(bInLoopingMode)
		, bLooped(false)
	{
		check(AudioBuffer && AudioBuffer->DecompressionState);
		check(AudioData);
	}

	FAsyncRealtimeAudioTaskWorker(USoundWave* InWaveData, uint8* InAudioData, int32 InMaxSamples)
		: WaveData(InWaveData)
		, AudioData(InAudioData)
		, MaxSamples(InMaxSamples)
		, BytesWritten(0)
		, TaskType(ERealtimeAudioTaskType::Procedural)
	{
		check(WaveData);
		check(AudioData);
	}

	void DoWork()
	{
		switch(TaskType)
		{
		case ERealtimeAudioTaskType::CompressedInfo:
			AudioBuffer->ReadCompressedInfo(WaveData);
			break;

		case ERealtimeAudioTaskType::Decompress:
			if (bSkipFirstBuffer)
			{
#if PLATFORM_ANDROID
				// Only skip one buffer on Android
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
#else
				// If we're using cached data we need to skip the first two reads from the data
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
#endif
			}
			bLooped = AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
			break;

		case ERealtimeAudioTaskType::Procedural:
			BytesWritten = WaveData->GeneratePCMData( (uint8*)AudioData, MaxSamples );
			break;

		default:
			check(false);
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		if (TaskType == ERealtimeAudioTaskType::Procedural)
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncRealtimeAudioProceduralWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
		else
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncRealtimeAudioDecompressWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	}

	ERealtimeAudioTaskType GetTaskType() const
	{
		return TaskType;
	}

	bool GetBufferLooped() const
	{ 
		check(TaskType == ERealtimeAudioTaskType::Decompress);
		return bLooped;
	}

	int32 GetBytesWritten() const
	{ 
		check(TaskType == ERealtimeAudioTaskType::Procedural);
		return BytesWritten;
	}
};

template<class T>
class FAsyncRealtimeAudioTaskProxy
{
public:
	FAsyncRealtimeAudioTaskProxy(T* InAudioBuffer, USoundWave* InWaveData)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InAudioBuffer, InWaveData);
	}

	FAsyncRealtimeAudioTaskProxy(T* InAudioBuffer, uint8* InAudioData, bool bInLoopingMode, bool bInSkipFirstBuffer)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InAudioBuffer, InAudioData, bInLoopingMode, bInSkipFirstBuffer);
	}

	FAsyncRealtimeAudioTaskProxy(USoundWave* InWaveData, uint8* InAudioData, int32 InMaxSamples)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InWaveData, InAudioData, InMaxSamples);
	}

	~FAsyncRealtimeAudioTaskProxy()
	{
		check(IsDone());
		delete Task;
	}

	bool IsDone()
	{
		FScopeLock Lock(&CritSect);
		return Task->IsDone();
	}

	void EnsureCompletion(bool bDoWorkOnThisThreadIfNotStarted = true)
	{
		FScopeLock Lock(&CritSect);
		Task->EnsureCompletion(bDoWorkOnThisThreadIfNotStarted);
	}

	void StartBackgroundTask()
	{
		FScopeLock Lock(&CritSect);
		Task->StartBackgroundTask();
	}

	FAsyncRealtimeAudioTaskWorker<T>& GetTask()
	{
		FScopeLock Lock(&CritSect);
		return Task->GetTask();
	}

private:
	FCriticalSection CritSect;
	FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>* Task;
};