// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/** 
 * Playable sound object for wave files that are streamed, particularly VOIP
 */

#pragma once
#include "Sound/SoundWave.h"
#include "SoundWaveProcedural.generated.h"

DECLARE_DELEGATE_TwoParams( FOnSoundWaveProceduralUnderflow, class USoundWaveProcedural*, int32 );

DEPRECATED(4.9, "FOnSoundWaveStreamingUnderflow has been renamed FOnSoundWaveProceduralUnderflow")
typedef FOnSoundWaveProceduralUnderflow FOnSoundWaveStreamingUnderflow;

DEPRECATED(4.9, "USoundWaveStreaming has been renamed USoundWaveProcedural.")
typedef class USoundWaveProcedural USoundWaveStreaming;

UCLASS()
class ENGINE_API USoundWaveProcedural : public USoundWave
{
	GENERATED_UCLASS_BODY()

private:
	TArray<uint8> QueuedAudio;

public:

	//~ Begin UObject Interface. 
	virtual void Serialize( FArchive& Ar ) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	//~ End UObject Interface. 

	//~ Begin USoundWave Interface.
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	virtual FByteBulkData* GetCompressedData(FName Format) override;
	virtual void InitAudioResource( FByteBulkData& CompressedData ) override;
	virtual bool InitAudioResource(FName Format) override;
	virtual int32 GetResourceSizeForFormat(FName Format) override;
	//~ End USoundWave Interface.


	/** Add data to the FIFO that feeds the audio device. */
	void QueueAudio(const uint8* AudioData, const int32 BufferSize);

	/** Remove all queued data from the FIFO. This is only necessary if you want to start over, or GeneratePCMData() isn't going to be called, since that will eventually drain it. */
	void ResetAudio();

	/** Query bytes queued for playback */
	int32 GetAvailableAudioByteCount();

	/** Called when GeneratePCMData is called but not enough data is available. Allows more data to be added, and will try again */
	FOnSoundWaveProceduralUnderflow OnSoundWaveProceduralUnderflow;
};