// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundWaveProcedural.h"
#include "IMediaAudioSink.h"
#include "MediaSoundWave.generated.h"


class UMediaPlayer;


/**
 * Implements a playable sound asset for audio streams from UMediaPlayer assets.
 */
UCLASS(hidecategories=(Compression, Info, Sound, SoundWave, Subtitles))
class MEDIAASSETS_API UMediaSoundWave
	: public USoundWave
	, public IMediaAudioSink
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_EVENT_OneParam(UMediaSoundWave, FOnBeginDestroy, UMediaSoundWave& /*DestroyedSoundWave*/)
	FOnBeginDestroy& OnBeginDestroy()
	{
		return BeginDestroyEvent;
	}

public:

	//~ USoundWave interface

	virtual int32 GeneratePCMData(uint8* Data, const int32 SamplesRequested) override;
	virtual FByteBulkData* GetCompressedData(FName Format) override;
	virtual int32 GetResourceSizeForFormat(FName Format) override;
	virtual void InitAudioResource(FByteBulkData& CompressedData) override;
	virtual bool InitAudioResource(FName Format) override;

public:

	//~ UObject interface

	virtual void BeginDestroy() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual void Serialize(FArchive& Ar) override;

public:

	//~ IMediaAudioSink interface

	virtual void FlushAudioSink() override;
	virtual int32 GetAudioSinkChannels() const override;
	virtual int32 GetAudioSinkSampleRate() const override;
	virtual bool InitializeAudioSink(uint32 Channels, uint32 SampleRate) override;
	virtual void PauseAudioSink() override;
	virtual void PlayAudioSink(const uint8* SampleBuffer, uint32 BufferSize, FTimespan Time) override;
	virtual void ResumeAudioSink() override;
	virtual void ShutdownAudioSink() override;

protected:

	//~ Deprecated members

	DEPRECATED_FORGAME(4.13, "The AudioTrackIndex property is no longer used. Please upgrade your content to Media Framework 2.0")
	UPROPERTY(BlueprintReadWrite, Category=MediaPlayer)
	int32 AudioTrackIndex;

	DEPRECATED_FORGAME(4.13, "The MediaPlayer property is no longer used. Please upgrade your content to Media Framework 2.0")
	UPROPERTY(BlueprintReadWrite, Category=MediaPlayer)
	class UMediaPlayer* MediaPlayer;

private:

	/** An event delegate that is invoked when this media texture is being destroyed. */
	FOnBeginDestroy BeginDestroyEvent;

	/** Critical section for synchronizing access to QueuedAudio. */
	mutable FCriticalSection CriticalSection;

	/** Whether the audio is currently paused. */
	bool Paused;

	/** Holds queued audio samples. */
	TArray<uint8> QueuedAudio;

	/** The sink's number of audio channels. */
	int32 SinkNumChannels;

	/** The sink's sample rate. */
	int32 SinkSampleRate;
};
