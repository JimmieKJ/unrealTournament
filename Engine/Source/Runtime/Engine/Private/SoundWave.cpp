// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundWave.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioDecompress.h"
#include "TargetPlatform.h"
#include "AudioDerivedData.h"
#include "SubtitleManager.h"
#include "DerivedDataCacheInterface.h"
/*-----------------------------------------------------------------------------
	FStreamedAudioChunk
-----------------------------------------------------------------------------*/

void FStreamedAudioChunk::Serialize(FArchive& Ar, UObject* Owner, int32 ChunkIndex)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStreamedAudioChunk::Serialize"), STAT_StreamedAudioChunk_Serialize, STATGROUP_LoadTime );

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	BulkData.Serialize(Ar, Owner, ChunkIndex);
	Ar << DataSize;

#if WITH_EDITORONLY_DATA
	if (!bCooked)
	{
		Ar << DerivedDataKey;
	}
#endif // #if WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
void FStreamedAudioChunk::StoreInDerivedDataCache(const FString& InDerivedDataKey)
{
	int32 BulkDataSizeInBytes = BulkData.GetBulkDataSize();
	check(BulkDataSizeInBytes > 0);

	TArray<uint8> DerivedData;
	FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
	Ar << BulkDataSizeInBytes;
	{
		void* BulkChunkData = BulkData.Lock(LOCK_READ_ONLY);
		Ar.Serialize(BulkChunkData, BulkDataSizeInBytes);
		BulkData.Unlock();
	}

	GetDerivedDataCacheRef().Put(*InDerivedDataKey, DerivedData);
	DerivedDataKey = InDerivedDataKey;
	BulkData.RemoveBulkData();
}
#endif // #if WITH_EDITORONLY_DATA

USoundWave::USoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Volume = 1.0;
	Pitch = 1.0;
	CompressionQuality = 40;
}

SIZE_T USoundWave::GetResourceSize(EResourceSizeMode::Type Mode)
{
	int32 CalculatedResourceSize = 0;

	if( DecompressionType == DTYPE_Native )
	{
		// If we've been decompressed, need to account for decompressed and also compressed
		CalculatedResourceSize += RawPCMDataSize;
	}

	if (GEngine && GEngine->GetMainAudioDevice())
	{
		// Don't add compressed data to size of streaming sounds
		if (!FPlatformProperties::SupportsAudioStreaming() || !IsStreaming())
		{
			CalculatedResourceSize += GetCompressedDataSize(GEngine->GetMainAudioDevice()->GetRuntimeFormat(this));
		}
	}

	return CalculatedResourceSize;
}

int32 USoundWave::GetResourceSizeForFormat(FName Format)
{
	return GetCompressedDataSize(Format);
}


FName USoundWave::GetExporterName()
{
#if WITH_EDITORONLY_DATA
	if( ChannelOffsets.Num() > 0 && ChannelSizes.Num() > 0 )
	{
		return( FName( TEXT( "SoundSurroundExporterWAV" ) ) );
	}
#endif // WITH_EDITORONLY_DATA

	return( FName( TEXT( "SoundExporterWAV" ) ) );
}


FString USoundWave::GetDesc()
{
	FString Channels;

	if( NumChannels == 0 )
	{
		Channels = TEXT( "Unconverted" );
	}
#if WITH_EDITORONLY_DATA
	else if( ChannelSizes.Num() == 0 )
	{
		Channels = ( NumChannels == 1 ) ? TEXT( "Mono" ) : TEXT( "Stereo" );
	}
#endif // WITH_EDITORONLY_DATA
	else
	{
		Channels = FString::Printf( TEXT( "%d Channels" ), NumChannels );
	}

	return FString::Printf( TEXT( "%3.2fs %s" ), Duration, *Channels );
}

void USoundWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	
#if WITH_EDITORONLY_DATA
	OutTags.Add( FAssetRegistryTag(SourceFileTagName(), SourceFilePath, FAssetRegistryTag::TT_Hidden) );
#endif
	// GetCompressedDataSize could technically modify this->CompressedFormatData therefore it is not const, however this information
	// is very useful in the asset registry so we will allow GetCompressedDataSize to be modified if the formats do not exist
	USoundWave* MutableThis = const_cast<USoundWave*>(this);
	const FString OggSize = FString::Printf(TEXT("%.2f"), MutableThis->GetCompressedDataSize("OGG") / 1024.0f );
	OutTags.Add( FAssetRegistryTag("OggSize", OggSize, UObject::FAssetRegistryTag::TT_Numerical) );
}

void USoundWave::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USoundWave::Serialize"), STAT_SoundWave_Serialize, STATGROUP_LoadTime );

	Super::Serialize( Ar );

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogAudio, Fatal, TEXT("This platform requires cooked packages, and audio data was not cooked into %s."), *GetFullName());
	}

	if (Ar.IsCooking())
	{
		CompressionName = Ar.CookingTarget()->GetWaveFormat(this);
	}
	
	if (Ar.UE4Ver() >= VER_UE4_SOUND_COMPRESSION_TYPE_ADDED)
	{
		Ar << CompressionName;
	}

	if (bCooked)
	{
		// Only want to cook/load full data if we don't support streaming
		if (!IsStreaming() ||
			(Ar.IsLoading() && !FPlatformProperties::SupportsAudioStreaming()) ||
			(Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::AudioStreaming)))
		{
			if (Ar.IsCooking())
			{
#if WITH_ENGINE
				TArray<FName> ActualFormatsToSave;
				if (!Ar.CookingTarget()->IsServerOnly())
				{
					// for now we only support one format per wav
					FName Format = Ar.CookingTarget()->GetWaveFormat(this);
					GetCompressedData(Format); // Get the data from the DDC or build it

					ActualFormatsToSave.Add(Format);
				}
				CompressedFormatData.Serialize(Ar, this, &ActualFormatsToSave);
#endif
			}
			else
			{
				CompressedFormatData.Serialize(Ar, this);
			}
		}
	}
	else
	{
		// only save the raw data for non-cooked packages
		RawData.Serialize( Ar, this );
	}

	Ar << CompressedDataGuid;

	if (IsStreaming())
	{
		if (bCooked)
		{
			// only cook/load streaming data if it's supported
			if ((Ar.IsLoading() && FPlatformProperties::SupportsAudioStreaming()) ||
				(Ar.IsCooking() && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::AudioStreaming)))
			{
				SerializeCookedPlatformData(Ar);
			}
		}

#if WITH_EDITORONLY_DATA	
		if (Ar.IsLoading() && !Ar.IsTransacting() && !bCooked && !(GetOutermost()->PackageFlags & PKG_ReloadingForCooker))
		{
			BeginCachePlatformData();
		}
#endif // #if WITH_EDITORONLY_DATA
	}
}

/**
 * Prints the subtitle associated with the SoundWave to the console
 */
void USoundWave::LogSubtitle( FOutputDevice& Ar )
{
	FString Subtitle = "";
	for( int32 i = 0; i < Subtitles.Num(); i++ )
	{
		Subtitle += Subtitles[ i ].Text.ToString();
	}

	if( Subtitle.Len() == 0 )
	{
		Subtitle = SpokenText;
	}

	if( Subtitle.Len() == 0 )
	{
		Subtitle = "<NO SUBTITLE>";
	}

	Ar.Logf( TEXT( "Subtitle:  %s" ), *Subtitle );
#if WITH_EDITORONLY_DATA
	Ar.Logf( TEXT( "Comment:   %s" ), *Comment );
#endif // WITH_EDITORONLY_DATA
	Ar.Logf( bMature ? TEXT( "Mature:    Yes" ) : TEXT( "Mature:    No" ) );
}

void USoundWave::PostInitProperties()
{
	Super::PostInitProperties();

	if(!IsTemplate())
	{
		InvalidateCompressedData();
	}
}

FByteBulkData* USoundWave::GetCompressedData(FName Format)
{
	if (IsTemplate() || IsRunningDedicatedServer())
	{
		return NULL;
	}
	bool bContainedData = CompressedFormatData.Contains(Format);
	FByteBulkData* Result = &CompressedFormatData.GetFormat(Format);
	if (!bContainedData)
	{
		if (!FPlatformProperties::RequiresCookedData() && GetDerivedDataCache())
		{
			TArray<uint8> OutData;
			FDerivedAudioDataCompressor* DeriveAudioData = new FDerivedAudioDataCompressor(this, Format);
			GetDerivedDataCacheRef().GetSynchronous(DeriveAudioData, OutData);
			if (OutData.Num())
			{
				Result->Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Result->Realloc(OutData.Num()), OutData.GetData(), OutData.Num());
				Result->Unlock();
			}
		}
		else
		{
			UE_LOG(LogAudio, Error, TEXT("Attempt to access the DDC when there is none available on sound '%s', format = %s. Should have been cooked."), *GetFullName(), *Format.ToString());
		}
	}
	check(Result);
	return Result->GetBulkDataSize() > 0 ? Result : NULL; // we don't return empty bulk data...but we save it to avoid thrashing the DDC
}

void USoundWave::InvalidateCompressedData()
{
	CompressedDataGuid = FGuid::NewGuid();
	CompressedFormatData.FlushData();
}

void USoundWave::PostLoad()
{
	Super::PostLoad();

	if (GetOutermost()->PackageFlags & PKG_ReloadingForCooker)
	{
		return;
	}

	// Compress to whatever formats the active target platforms want
	// static here as an optimization
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			GetCompressedData(Platforms[Index]->GetWaveFormat(this));
		}
	}

	// We don't precache default objects and we don't precache in the Editor as the latter will
	// most likely cause us to run out of memory.
	if( !GIsEditor && !IsTemplate( RF_ClassDefaultObject ) && GEngine )
	{
		FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
		if( AudioDevice && AudioDevice->bStartupSoundsPreCached)
		{
			// Upload the data to the hardware, but only if we've precached startup sounds already
			AudioDevice->Precache( this );
		}
		// remove bulk data if no AudioDevice is used and no sounds were initialized
		else if( IsRunningGame() )
		{
			RawData.RemoveBulkData();
		}
	}

	// Only add this streaming sound if we're not a dedicated server or if there is an audio device manager
	if (IsStreaming() && !IsRunningDedicatedServer() && GEngine && GEngine->GetAudioDeviceManager())
	{
#if WITH_EDITORONLY_DATA
		FinishCachePlatformData();
#endif // #if WITH_EDITORONLY_DATA
		IStreamingManager::Get().GetAudioStreamingManager().AddStreamingSoundWave(this);
	}

	INC_FLOAT_STAT_BY( STAT_AudioBufferTime, Duration );
	INC_FLOAT_STAT_BY( STAT_AudioBufferTimeChannels, NumChannels * Duration );
}


void USoundWave::InitAudioResource( FByteBulkData& CompressedData )
{
	if( !ResourceSize )
	{
		// Grab the compressed vorbis data from the bulk data
		ResourceSize = CompressedData.GetBulkDataSize();
		if( ResourceSize > 0 )
		{
			check(!ResourceData);
			CompressedData.GetCopy( ( void** )&ResourceData, true );
		}
	}
}

bool USoundWave::InitAudioResource(FName Format)
{
	if( !ResourceSize && (!FPlatformProperties::SupportsAudioStreaming() || !IsStreaming()) )
	{
		FByteBulkData* Bulk = GetCompressedData(Format);
		if (Bulk)
		{
			ResourceSize = Bulk->GetBulkDataSize();
			check(ResourceSize > 0);
			check(!ResourceData);
			Bulk->GetCopy((void**)&ResourceData, true);
		}
	}

	return ResourceSize > 0;
}

void USoundWave::RemoveAudioResource()
{
	if(ResourceData)
	{
		FMemory::Free(ResourceData);
		ResourceSize = 0;
		ResourceData = NULL;
	}
}


bool USoundWave::IsLocalizedResource()
{
	FString FullPathName;
	bool bIsLocalised = false;

	//@todo-packageloc Handle this based on the appropriate localized package name.

	return( Super::IsLocalizedResource() || Subtitles.Num() > 0 || bIsLocalised );
}

#if WITH_EDITOR

void USoundWave::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static FName CompressionQualityFName = FName( TEXT( "CompressionQuality" ) );
	static FName StreamingFName = GET_MEMBER_NAME_CHECKED(USoundWave, bStreaming);

	// Prevent constant re-compression of SoundWave while properties are being changed interactively
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
		// Regenerate on save any compressed sound formats
		if ( PropertyThatChanged && PropertyThatChanged->GetFName() == CompressionQualityFName )
		{
			InvalidateCompressedData();
			FreeResources();
			UpdatePlatformData();
			MarkPackageDirty();
		}
		else if (PropertyThatChanged && PropertyThatChanged->GetFName() == StreamingFName)
		{
			FreeResources();
			UpdatePlatformData();
			MarkPackageDirty();
		}
	}
}
#endif // WITH_EDITOR

void USoundWave::FreeResources()
{
	// Housekeeping of stats
	DEC_FLOAT_STAT_BY( STAT_AudioBufferTime, Duration );
	DEC_FLOAT_STAT_BY( STAT_AudioBufferTimeChannels, NumChannels * Duration );

	// GEngine is NULL during script compilation and GEngine->Client and its audio device might be
	// destroyed first during the exit purge.
	if( GEngine && !GExitPurge )
	{
		// Notify the audio device to free the bulk data associated with this wave.
		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		if (AudioDeviceManager != NULL)
		{
			AudioDeviceManager->StopSoundsUsingWave(this);
			AudioDeviceManager->FreeResource(this);
		}
	}

	// Just in case the data was created but never uploaded
	if (RawPCMData != NULL)
	{
		FMemory::Free(RawPCMData);
		RawPCMData = NULL;
	}

	// Remove the compressed copy of the data
	RemoveAudioResource();

	// Stat housekeeping
	DEC_DWORD_STAT_BY(STAT_AudioMemorySize, TrackedMemoryUsage);
	DEC_DWORD_STAT_BY(STAT_AudioMemory, TrackedMemoryUsage);
	TrackedMemoryUsage = 0;

	SampleRate = 0;
	Duration = 0.0f;
	ResourceID = 0;
	bDynamicResource = false;
	DecompressionType = DTYPE_Setup;
	bDecompressedFromOgg = 0;
	RawPCMDataSize = 0;
}

FWaveInstance* USoundWave::HandleStart( FActiveSound& ActiveSound, const UPTRINT WaveInstanceHash )
{
	// Create a new wave instance and associate with the ActiveSound
	FWaveInstance* WaveInstance = new FWaveInstance( &ActiveSound );
	WaveInstance->WaveInstanceHash = WaveInstanceHash;
	ActiveSound.WaveInstances.Add( WaveInstanceHash, WaveInstance );

	// Add in the subtitle if they exist
	if (ActiveSound.bHandleSubtitles && Subtitles.Num() > 0)
	{
		if (UAudioComponent* AudioComponent = ActiveSound.AudioComponent.Get())
		{
			// TODO - Audio Threading. This would need to be a call back to the main thread.
			if (AudioComponent->OnQueueSubtitles.IsBound())
			{
				// intercept the subtitles if the delegate is set
				ActiveSound.AudioComponent->OnQueueSubtitles.ExecuteIfBound( Subtitles, Duration );
			}
			else
			{
				// otherwise, pass them on to the subtitle manager for display
				// Subtitles are hashed based on the associated sound (wave instance).
				FSubtitleManager::GetSubtitleManager()->QueueSubtitles( ( PTRINT )WaveInstance, ActiveSound.SubtitlePriority, bManualWordWrap, bSingleLine, Duration, Subtitles, ActiveSound.World->GetAudioTimeSeconds() );
			}
		}
	}

	return WaveInstance;
}

bool USoundWave::IsReadyForFinishDestroy()
{
	bool bIsStreamingInProgress = IStreamingManager::Get().GetAudioStreamingManager().IsStreamingInProgress(this);

	// Wait till streaming and decompression finishes before deleting resource.
	return( !bIsStreamingInProgress && (( AudioDecompressor == NULL ) || AudioDecompressor->IsDone()) );
}


void USoundWave::FinishDestroy()
{
	FreeResources();

	Super::FinishDestroy();

	CleanupCachedRunningPlatformData();
#if WITH_EDITOR
	ClearAllCachedCookedPlatformData();
#endif

	IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundWave(this);
}

void USoundWave::Parse( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FWaveInstance* WaveInstance = ActiveSound.FindWaveInstance(NodeWaveInstanceHash);

	// Create a new WaveInstance if this SoundWave doesn't already have one associated with it.
	if( WaveInstance == NULL )
	{
		if( !ActiveSound.bRadioFilterSelected )
		{
			ActiveSound.ApplyRadioFilter( AudioDevice, ParseParams );
		}

		WaveInstance = HandleStart( ActiveSound, NodeWaveInstanceHash);
	}

	// Looping sounds are never actually finished
	if (bLooping || ParseParams.bLooping)
	{
		WaveInstance->bIsFinished = false;
#if !NO_LOGGING
		if (!ActiveSound.bWarnedAboutOrphanedLooping && !ActiveSound.AudioComponent.IsValid())
		{
			UE_LOG(LogAudio, Warning, TEXT("Detected orphaned looping sound '%s'."), *ActiveSound.Sound->GetName());
			ActiveSound.bWarnedAboutOrphanedLooping = true;
		}
#endif
	}

	// Check for finished paths.
	if( !WaveInstance->bIsFinished )
	{
		// Propagate properties and add WaveInstance to outgoing array of FWaveInstances.
		WaveInstance->Volume = ParseParams.Volume * Volume;
		WaveInstance->VolumeMultiplier = ParseParams.VolumeMultiplier;
		WaveInstance->Pitch = ParseParams.Pitch * Pitch;
		WaveInstance->HighFrequencyGain = ParseParams.HighFrequencyGain;
		WaveInstance->bApplyRadioFilter = ActiveSound.bApplyRadioFilter;
		WaveInstance->StartTime = ParseParams.StartTime;
		WaveInstance->UserIndex = ActiveSound.UserIndex;
		WaveInstance->OmniRadius = ParseParams.OmniRadius;

		bool bAlwaysPlay = false;

		// Properties from the sound class
		WaveInstance->SoundClass = ParseParams.SoundClass;
		if (ParseParams.SoundClass)
		{
			FSoundClassProperties* SoundClassProperties = AudioDevice->GetSoundClassCurrentProperties(ParseParams.SoundClass);
			// Use values from "parsed/ propagated" sound class properties
			WaveInstance->VolumeMultiplier *= SoundClassProperties->Volume;
			WaveInstance->Pitch *= SoundClassProperties->Pitch;
			//TODO: Add in HighFrequencyGainMultiplier property to sound classes


			WaveInstance->VoiceCenterChannelVolume = SoundClassProperties->VoiceCenterChannelVolume;
			WaveInstance->RadioFilterVolume = SoundClassProperties->RadioFilterVolume * ParseParams.VolumeMultiplier;
			WaveInstance->RadioFilterVolumeThreshold = SoundClassProperties->RadioFilterVolumeThreshold * ParseParams.VolumeMultiplier;
			WaveInstance->StereoBleed = SoundClassProperties->StereoBleed;
			WaveInstance->LFEBleed = SoundClassProperties->LFEBleed;
			
			WaveInstance->bIsUISound = ActiveSound.bIsUISound || SoundClassProperties->bIsUISound;
			WaveInstance->bIsMusic = ActiveSound.bIsMusic || SoundClassProperties->bIsMusic;
			WaveInstance->bCenterChannelOnly = ActiveSound.bCenterChannelOnly || SoundClassProperties->bCenterChannelOnly;
			WaveInstance->bEQFilterApplied = ActiveSound.bEQFilterApplied || SoundClassProperties->bApplyEffects;
			WaveInstance->bReverb = ActiveSound.bReverb || SoundClassProperties->bReverb;
			WaveInstance->OutputTarget = SoundClassProperties->OutputTarget;

			bAlwaysPlay = ActiveSound.bAlwaysPlay || SoundClassProperties->bAlwaysPlay;
		}
		else
		{
			WaveInstance->VoiceCenterChannelVolume = 0.f;
			WaveInstance->RadioFilterVolume = 0.f;
			WaveInstance->RadioFilterVolumeThreshold = 0.f;
			WaveInstance->StereoBleed = 0.f;
			WaveInstance->LFEBleed = 0.f;
			WaveInstance->bEQFilterApplied = ActiveSound.bEQFilterApplied;
			WaveInstance->bIsUISound = ActiveSound.bIsUISound;
			WaveInstance->bIsMusic = ActiveSound.bIsMusic;
			WaveInstance->bReverb = ActiveSound.bReverb;
			WaveInstance->bCenterChannelOnly = ActiveSound.bCenterChannelOnly;

			bAlwaysPlay = ActiveSound.bAlwaysPlay;
		}

		WaveInstance->PlayPriority = WaveInstance->Volume + ( bAlwaysPlay ? 1.0f : 0.0f ) + WaveInstance->RadioFilterVolume;
		WaveInstance->Location = ParseParams.Transform.GetTranslation();
		WaveInstance->bIsStarted = true;
		WaveInstance->bAlreadyNotifiedHook = false;
		WaveInstance->bUseSpatialization = ParseParams.bUseSpatialization;
		WaveInstance->SpatializationAlgorithm = ParseParams.SpatializationAlgorithm;
		WaveInstance->WaveData = this;
		WaveInstance->NotifyBufferFinishedHooks = ParseParams.NotifyBufferFinishedHooks;
		WaveInstance->LoopingMode = ((bLooping || ParseParams.bLooping) ? LOOP_Forever : LOOP_Never);

		if (AudioDevice->IsHRTFEnabledForAll() && ParseParams.SpatializationAlgorithm == SPATIALIZATION_Default)
		{
			WaveInstance->SpatializationAlgorithm = SPATIALIZATION_HRTF;
		}
		else
		{
			WaveInstance->SpatializationAlgorithm = ParseParams.SpatializationAlgorithm;
		}

		// Don't add wave instances that are not going to be played at this point.
		if( WaveInstance->PlayPriority > KINDA_SMALL_NUMBER )
		{
			WaveInstances.Add( WaveInstance );
		}

		// We're still alive.
		ActiveSound.bFinished = false;

		// Sanity check
		if( NumChannels >= 2 && WaveInstance->bUseSpatialization && !WaveInstance->bReportedSpatializationWarning)
		{
			static TSet<USoundWave*> ReportedSounds;
			if (!ReportedSounds.Contains(this))
			{
				FString SoundWarningInfo = FString::Printf(TEXT("Spatialisation on stereo and multichannel sounds is not supported. SoundWave: %s"), *GetName());
				if (ActiveSound.Sound != this)
				{
					SoundWarningInfo += FString::Printf(TEXT(" SoundCue: %s"), *ActiveSound.Sound->GetName());
				}

				if (ActiveSound.AudioComponent.IsValid())
				{
					// TODO - Audio Threading. This log would have to be a task back to game thread
					AActor* SoundOwner = ActiveSound.AudioComponent->GetOwner();
					UE_LOG(LogAudio, Warning, TEXT( "%s Actor: %s AudioComponent: %s" ), *SoundWarningInfo, (SoundOwner ? *SoundOwner->GetName() : TEXT("None")), *ActiveSound.AudioComponent->GetName() );
				}
				else
				{
					UE_LOG(LogAudio, Warning, TEXT("%s"), *SoundWarningInfo );
				}

				ReportedSounds.Add(this);
			}
			WaveInstance->bReportedSpatializationWarning = true;
		}
	}
}

bool USoundWave::IsPlayable() const
{
	return true;
}

float USoundWave::GetMaxAudibleDistance()
{
	return (AttenuationSettings ? AttenuationSettings->Attenuation.GetMaxDimension() : WORLD_MAX);
}

float USoundWave::GetDuration()
{
	return ( bLooping ? INDEFINITELY_LOOPING_DURATION : Duration);
}

bool USoundWave::IsStreaming() const
{
	// TODO: add in check on whether it's part of a streaming SoundGroup
	return bStreaming;
}

void USoundWave::UpdatePlatformData()
{
	if (IsStreaming())
	{
		// Make sure there are no pending requests in flight.
		while (IStreamingManager::Get().GetAudioStreamingManager().IsStreamingInProgress(this))
		{
			// Give up timeslice.
			FPlatformProcess::Sleep(0);
		}

#if WITH_EDITORONLY_DATA
		// Temporarily remove from streaming manager to release currently used data chunks
		IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundWave(this);
		// Recache platform data if the source has changed.
		CachePlatformData();
		// Add back to the streaming manager to reload first chunk
		IStreamingManager::Get().GetAudioStreamingManager().AddStreamingSoundWave(this);
#endif
	}
	else
	{
		IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundWave(this);
	}
}

void USoundWave::GetChunkData(int32 ChunkIndex, uint8** OutChunkData)
{
	if (RunningPlatformData->TryLoadChunk(ChunkIndex, OutChunkData) == false)
	{
		// Unable to load chunks from the cache. Rebuild the sound and try again.
		UE_LOG(LogAudio, Warning, TEXT("GetChunkData failed for %s"), *GetPathName());
#if WITH_EDITORONLY_DATA
		ForceRebuildPlatformData();
		if (RunningPlatformData->TryLoadChunk(ChunkIndex, OutChunkData) == false)
		{
			UE_LOG(LogAudio, Error, TEXT("Failed to build sound %s."), *GetPathName());
		}
#endif // #if WITH_EDITORONLY_DATA
	}
}
