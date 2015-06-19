// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioEffect.h"
#include "AudioDecompress.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "IAudioExtensionPlugin.h"

/*-----------------------------------------------------------------------------
	FAudioDevice implementation.
-----------------------------------------------------------------------------*/

FAudioDevice::FAudioDevice()
	: CommonAudioPool(nullptr)
	, CommonAudioPoolFreeBytes(0)
	, bGameWasTicking(true)
	, bDisableAudioCaching(false)
	, bStartupSoundsPreCached(false)
	, CurrentTick(0)
	, TestAudioComponent(nullptr)
	, DebugState(DEBUGSTATE_None)
	, TransientMasterVolume(1.0f)
	, LastUpdateTime(0.0f)
	, NextResourceID(1)
	, BaseSoundMix(nullptr)
	, DefaultBaseSoundMix(nullptr)
	, Effects(nullptr)
	, CurrentAudioVolume(nullptr)
	, HighestPriorityReverb(nullptr)
	, SpatializationPlugin(nullptr)
	, SpatializeProcessor(nullptr)
	, bSpatializationExtensionEnabled(false)
	, bHRTFEnabledForAll(false)
	, bIsDeviceMuted(false)
	, bIsInitialized(false)
{
}

FAudioEffectsManager* FAudioDevice::CreateEffectsManager()
{
	// use a nop effect manager by default
	return new FAudioEffectsManager(this);
}

bool FAudioDevice::Init()
{
	if (bIsInitialized)
	{
		return true;
	}

	bool bDeferStartupPrecache = false;
	// initialize config variables
	verify(GConfig->GetInt(TEXT("Audio"), TEXT("MaxChannels"), MaxChannels, GEngineIni));
	verify(GConfig->GetInt(TEXT("Audio"), TEXT("CommonAudioPoolSize"), CommonAudioPoolSize, GEngineIni));
	
	// If this is true, skip the initial startup precache so we can do it later in the flow
	GConfig->GetBool(TEXT("Audio"), TEXT("DeferStartupPrecache"), bDeferStartupPrecache, GEngineIni);

	const FStringAssetReference DefaultBaseSoundMixName = GetDefault<UAudioSettings>()->DefaultBaseSoundMix;
	if (DefaultBaseSoundMixName.IsValid())
	{
		DefaultBaseSoundMix = LoadObject<USoundMix>(NULL, *DefaultBaseSoundMixName.ToString());
	}

	GetDefault<USoundGroups>()->Initialize();

	// Parses sound classes.
	InitSoundClasses();

	// allow the platform to startup
	if (InitializeHardware() == false)
	{
		// Could not initialize hardware, teardown anything that was set up during initialization
		Teardown();

		return false;
	}

	// create a platform specific effects manager
	Effects = CreateEffectsManager();

	// Get the audio spatialization plugin
	// Note: there is only *one* spatialization plugin currently, but the GetModularFeatureImplementation only returns a TArray
	// Therefore, we are just grabbing the first one that is returned (if one is returned).
	TArray<IAudioSpatializationPlugin *> SpatializationPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IAudioSpatializationPlugin>(IAudioSpatializationPlugin::GetModularFeatureName());
	for (IAudioSpatializationPlugin* Plugin : SpatializationPlugins)
	{
		SpatializationPlugin = Plugin;

		Plugin->Initialize();
		SpatializeProcessor = Plugin->GetNewSpatializationAlgorithm(this);
		bSpatializationExtensionEnabled = true;

		// There should only ever be 0 or 1 spatialization plugin at the moment
		check(SpatializationPlugins.Num() == 1);
		break;
	}

	InitSoundSources();
	
	// Make sure the Listeners array has at least one entry, so we don't have to check for Listeners.Num() == 0 all the time
	Listeners.Add(FListener());

	if (!bDeferStartupPrecache)
	{
		PrecacheStartupSounds();
	}

	UE_LOG(LogInit, Log, TEXT("FAudioDevice initialized." ));

	bIsInitialized = true;

	return true;
}

float FAudioDevice::GetLowPassFilterResonance() const
{
	return GetDefault<UAudioSettings>()->LowPassFilterResonance;
}

void FAudioDevice::PrecacheStartupSounds()
{
	// Iterate over all already loaded sounds and precache them. This relies on Super::Init in derived classes to be called last.
	if (!GIsEditor)
	{
		for (TObjectIterator<USoundWave> It; It; ++It)
		{
			USoundWave* SoundWave = *It;
			Precache(SoundWave);
		}

		bStartupSoundsPreCached = true;
	}
}

void FAudioDevice::SetMaxChannels(int32 InMaxChannels)
{
	if (InMaxChannels >= Sources.Num())
	{
		UE_LOG(LogAudio, Warning, TEXT( "Can't increase channels past starting number!" ) );
		return;
	}

	MaxChannels = InMaxChannels;
}

void FAudioDevice::Teardown()
{
	// Flush stops all sources so sources can be safely deleted below.
	Flush(NULL);

	// Clear out the EQ/Reverb/LPF effects
	if (Effects)
	{
		delete Effects;
		Effects = NULL;
	}

	// let platform shutdown
	TeardownHardware();

	// Note: we don't free audio buffers at this stage since they are managed in the audio device manager

	// Must be after FreeBufferResource as that potentially stops sources
	for (int32 Index = 0; Index < Sources.Num(); Index++)
	{
		Sources[Index]->Stop();
		delete Sources[Index];
	}
	Sources.Empty();
	FreeSources.Empty();

	if (SpatializationPlugin != nullptr)
	{
		if (SpatializeProcessor != nullptr)
		{
			delete SpatializeProcessor;
			SpatializeProcessor = nullptr;
		}

		SpatializationPlugin->Shutdown();
		SpatializationPlugin = nullptr;
	}
}

void FAudioDevice::Suspend(bool bGameTicking)
{
	HandlePause( bGameTicking, true );
}

void FAudioDevice::CountBytes(FArchive& Ar)
{
	Sources.CountBytes(Ar);
	// The buffers are stored on the audio device since they are shared amongst all audio devices
	// Though we are going to count them when querying an individual audio device object about its bytes
	GEngine->GetAudioDeviceManager()->Buffers.CountBytes(Ar);
	FreeSources.CountBytes(Ar);
	WaveInstanceSourceMap.CountBytes(Ar);
	Ar.CountBytes(sizeof(FWaveInstance) * WaveInstanceSourceMap.Num(), sizeof(FWaveInstance) * WaveInstanceSourceMap.Num());
	SoundClasses.CountBytes(Ar);
	SoundMixModifiers.CountBytes(Ar);
}

void FAudioDevice::AddReferencedObjects( FReferenceCollector& Collector )
{	
	Collector.AddReferencedObject(DefaultBaseSoundMix);

	for( TMap< USoundMix*, FSoundMixState >::TIterator It( SoundMixModifiers ); It; ++It )
	{
		Collector.AddReferencedObject(It.Key());
	}

	Effects->AddReferencedObjects(Collector);

	for( int32 i = 0; i < ActiveSounds.Num(); ++i )
	{
		ActiveSounds[i]->AddReferencedObjects(Collector);
	}
	for( int32 i = 0; i < PrevPassiveSoundMixModifiers.Num(); ++i )
	{
		Collector.AddReferencedObject(PrevPassiveSoundMixModifiers[i]);
	}
}

void FAudioDevice::ResetInterpolation( void )
{
	for (FListener& Listener : Listeners)
	{
		Listener.InteriorStartTime = 0.0;
		Listener.InteriorEndTime = 0.0;
		Listener.ExteriorEndTime = 0.0;
		Listener.InteriorLPFEndTime = 0.0;
		Listener.ExteriorLPFEndTime = 0.0;

		Listener.InteriorVolumeInterp = 0.0f;
		Listener.InteriorLPFInterp = 0.0f;
		Listener.ExteriorVolumeInterp = 0.0f;
		Listener.ExteriorLPFInterp = 0.0f;
	}

	// Reset sound class properties to defaults
	for( TMap<USoundClass*, FSoundClassProperties>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			It.Value() = SoundClass->Properties;
		}
	}

	SoundMixModifiers.Empty();
	PrevPassiveSoundMixModifiers.Empty();
	BaseSoundMix = NULL;

	// reset audio effects
	Effects->ResetInterpolation();
}

void FAudioDevice::EnableRadioEffect( bool bEnable )
{
	if( bEnable )
	{
		DebugState = DEBUGSTATE_None;
	}
	else
	{
		UE_LOG(LogAudio, Log, TEXT( "Radio disabled for all sources" ) );
		DebugState = DEBUGSTATE_DisableRadio;
	}
}

#if !UE_BUILD_SHIPPING
bool FAudioDevice::HandleShowSoundClassHierarchyCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	ShowSoundClassHierarchy( Ar );
	return true;
}

bool FAudioDevice::HandleListWavesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FWaveInstance*> WaveInstances;
	int32 FirstActiveIndex = GetSortedActiveWaveInstances( WaveInstances, ESortedActiveWaveGetType::QueryOnly );

	for( int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
		AActor* SoundOwner = WaveInstance->ActiveSound->AudioComponent.IsValid() ? WaveInstance->ActiveSound->AudioComponent->GetOwner() : NULL;
		Ar.Logf( TEXT( "%4i.    %s %6.2f %6.2f  %s   %s"), InstanceIndex, Source ? TEXT( "Yes" ) : TEXT( " No" ), WaveInstance->ActiveSound->PlaybackTime, WaveInstance->GetActualVolume(), *WaveInstance->WaveData->GetPathName(), SoundOwner ? *SoundOwner->GetName() : TEXT("None") );
	}

	Ar.Logf( TEXT("Total: %i"), WaveInstances.Num()-FirstActiveIndex );
	return true;
}

void FAudioDevice::GetSoundClassInfo( TMap<FName, FAudioClassInfo>& AudioClassInfos )
{
	// Iterate over all sound cues to get a unique map of sound node waves to class names
	TMap<USoundWave*, FName> SoundWaveClasses;

	for( TObjectIterator<USoundCue> CueIt; CueIt; ++CueIt )
	{
		TArray<USoundNodeWavePlayer*> WavePlayers;

		USoundCue* SoundCue = *CueIt;
		SoundCue->RecursiveFindNode<USoundNodeWavePlayer>( SoundCue->FirstNode, WavePlayers );

		for( int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex )
		{
			// Presume one class per sound node wave
			USoundWave *SoundWave = WavePlayers[ WaveIndex ]->SoundWave;
			if (SoundWave && SoundCue->GetSoundClass())
			{
				SoundWaveClasses.Add( SoundWave, SoundCue->GetSoundClass()->GetFName() );
			}
		}
	}

	// Add any sound node waves that are not referenced by sound cues
	for( TObjectIterator<USoundWave> WaveIt; WaveIt; ++WaveIt )
	{
		USoundWave* SoundWave = *WaveIt;
		if( SoundWaveClasses.Find( SoundWave ) == NULL )
		{
			SoundWaveClasses.Add( SoundWave, NAME_UnGrouped );
		}
	}

	// Collate the data into something useful
	for( TMap<USoundWave*, FName>::TIterator MapIter( SoundWaveClasses ); MapIter; ++MapIter )
	{
		USoundWave* SoundWave = MapIter.Key();
		FName ClassName = MapIter.Value();

		FAudioClassInfo* AudioClassInfo = AudioClassInfos.Find( ClassName );
		if( AudioClassInfo == NULL )
		{
			FAudioClassInfo NewAudioClassInfo;

			NewAudioClassInfo.NumResident = 0;
			NewAudioClassInfo.SizeResident = 0;
			NewAudioClassInfo.NumRealTime = 0;
			NewAudioClassInfo.SizeRealTime = 0;

			AudioClassInfos.Add( ClassName, NewAudioClassInfo );

			AudioClassInfo = AudioClassInfos.Find( ClassName );
			check( AudioClassInfo );
		}

#if !WITH_EDITOR
		AudioClassInfo->SizeResident += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
		AudioClassInfo->NumResident++;
#else
		switch( SoundWave->DecompressionType )
		{
		case DTYPE_Native:
		case DTYPE_Preview:
			AudioClassInfo->SizeResident += SoundWave->RawPCMDataSize;
			AudioClassInfo->NumResident++;
			break;

		case DTYPE_RealTime:
			AudioClassInfo->SizeRealTime += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
			AudioClassInfo->NumRealTime++;
			break;

		case DTYPE_Streaming:
			// Add these to real time count for now - eventually compressed data won't be loaded &
			// might have a class info entry of their own
			AudioClassInfo->SizeRealTime += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
			AudioClassInfo->NumRealTime++;
			break;

		case DTYPE_Setup:
		case DTYPE_Invalid:
		default:
			break;
		}
#endif
	}
}

bool FAudioDevice::HandleListSoundClassesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TMap<FName, FAudioClassInfo> AudioClassInfos;

	GetSoundClassInfo( AudioClassInfos );

	Ar.Logf( TEXT( "Listing all sound classes." ) );

	// Display the collated data
	int32 TotalSounds = 0;
	for( TMap<FName, FAudioClassInfo>::TIterator ACIIter( AudioClassInfos ); ACIIter; ++ACIIter )
	{
		FName ClassName = ACIIter.Key();
		FAudioClassInfo* ACI = AudioClassInfos.Find( ClassName );

		FString Line = FString::Printf( TEXT( "Class '%s' has %d resident sounds taking %.2f kb" ), *ClassName.ToString(), ACI->NumResident, ACI->SizeResident / 1024.0f );
		TotalSounds += ACI->NumResident;
		if( ACI->NumRealTime > 0 )
		{
			Line += FString::Printf( TEXT( ", and %d real time sounds taking %.2f kb " ), ACI->NumRealTime, ACI->SizeRealTime / 1024.0f );
			TotalSounds += ACI->NumRealTime;
		}

		Ar.Logf( *Line );
	}

	Ar.Logf( TEXT( "%d total sounds in %d classes" ), TotalSounds, AudioClassInfos.Num() );
	return true;
}

void FAudioDevice::ShowSoundClassHierarchy( FOutputDevice& Ar, USoundClass* InSoundClass, int32 Indent  ) const
{
	TArray<USoundClass*> SoundClassesToShow;
	if (InSoundClass)
	{
		SoundClassesToShow.Add(InSoundClass);
	}
	else
	{
		for( TMap<USoundClass*, FSoundClassProperties>::TConstIterator It( SoundClasses ); It; ++It )
		{
			USoundClass* SoundClass = It.Key();
			if (SoundClass && SoundClass->ParentClass == NULL)
			{
				SoundClassesToShow.Add(SoundClass);
			}
		}
	}

	for (int32 Index=0; Index < SoundClassesToShow.Num(); ++Index)
	{
		USoundClass* SoundClass = SoundClassesToShow[Index];
		if (Indent > 0)
		{
			Ar.Logf(TEXT("%s|- %s"), FCString::Spc(Indent*2), *SoundClass->GetName());
		}
		else
		{
			Ar.Logf(*SoundClass->GetName());
		}
		for (int i = 0; i < SoundClass->ChildClasses.Num(); ++i)
		{
			if (SoundClass->ChildClasses[i])
			{
				ShowSoundClassHierarchy( Ar, SoundClass->ChildClasses[i], Indent+1);
			}
		}
	}
}

int32 PrecachedRealtime = 0;
int32 PrecachedNative = 0;
int32 TotalNativeSize = 0;
float AverageNativeLength = 0.f;
TMap<int32, int32> NativeChannelCount;
TMap<int32, int32> NativeSampleRateCount;

bool FAudioDevice::HandleDumpSoundInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf(TEXT("Native Count: %d\nRealtime Count: %d\n"), PrecachedNative, PrecachedRealtime);
	float AverageSize = 0.0f;
	if( PrecachedNative != 0 )
	{
		PrecachedNative = TotalNativeSize / PrecachedNative;
	}
	Ar.Logf(TEXT("Average Length: %.3g\nTotal Size: %d\nAverage Size: %.3g\n"), AverageNativeLength, TotalNativeSize, PrecachedNative );
	Ar.Logf(TEXT("Channel counts:\n"));
	for (auto CountIt = NativeChannelCount.CreateConstIterator(); CountIt; ++CountIt)
	{
		Ar.Logf(TEXT("\t%d: %d"),CountIt.Key(), CountIt.Value());
	}
	Ar.Logf(TEXT("Sample rate counts:\n"));
	for (auto SampleRateIt = NativeSampleRateCount.CreateConstIterator(); SampleRateIt; ++SampleRateIt)
	{
		Ar.Logf(TEXT("\t%d: %d"), SampleRateIt.Key(), SampleRateIt.Value());
	}
	return true;
}


bool FAudioDevice::HandleListSoundClassVolumesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf(TEXT("SoundClass Volumes: (Volume, Pitch)"));

	for( TMap<USoundClass*, FSoundClassProperties>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			const FSoundClassProperties& CurClass = It.Value();

			Ar.Logf( TEXT("Cur (%3.2f, %3.2f) for SoundClass %s"), CurClass.Volume, CurClass.Pitch, *SoundClass->GetName() );
		}
	}

	return true;
}

bool FAudioDevice::HandleListAudioComponentsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 Count = 0;
	Ar.Logf( TEXT( "AudioComponent Dump" ) );
	for( TObjectIterator<UAudioComponent> It; It; ++It )
	{
		UAudioComponent* AudioComponent = *It;
		UObject* Outer = It->GetOuter();
		UObject* Owner = It->GetOwner();
		Ar.Logf( TEXT("    0x%p: %s, %s, %s, %s"),
			AudioComponent,
			*( It->GetPathName() ),
			It->Sound ? *( It->Sound->GetPathName() ) : TEXT( "NO SOUND" ),
			Outer ? *( Outer->GetPathName() ) : TEXT( "NO OUTER" ),
			Owner ? *( Owner->GetPathName() ) : TEXT( "NO OWNER" ) );
		Ar.Logf( TEXT( "        bAutoDestroy....................%s" ), AudioComponent->bAutoDestroy ? TEXT( "true" ) : TEXT( "false" ) );
		Ar.Logf( TEXT( "        bStopWhenOwnerDestroyed.........%s" ), AudioComponent->bStopWhenOwnerDestroyed ? TEXT( "true" ) : TEXT( "false" ) );
		Ar.Logf( TEXT( "        bShouldRemainActiveIfDropped....%s" ), AudioComponent->bShouldRemainActiveIfDropped ? TEXT( "true" ) : TEXT( "false" ) );
		Ar.Logf( TEXT( "        bIgnoreForFlushing..............%s" ), AudioComponent->bIgnoreForFlushing ? TEXT( "true" ) : TEXT( "false" ) );
		Count++;
	}
	Ar.Logf( TEXT( "AudioComponent Total = %d" ), Count );

	Ar.Logf( TEXT( "AudioDevice 0x%p has %d ActiveSounds" ),
		this, ActiveSounds.Num());
	for( int32 ASIndex = 0; ASIndex < ActiveSounds.Num(); ASIndex++ )
	{
		const FActiveSound* ActiveSound = ActiveSounds[ASIndex];
		UAudioComponent* AComp = ActiveSound->AudioComponent.Get();
		if( AComp )
		{
			Ar.Logf( TEXT( "    0x%p: %4d - %s, %s, %s, %s" ),
				AComp,
				ASIndex,
				*( AComp->GetPathName() ),
				ActiveSound->Sound ? *( ActiveSound->Sound->GetPathName() ) : TEXT( "NO SOUND" ),
				AComp->GetOuter() ? *( AComp->GetOuter()->GetPathName() ) : TEXT( "NO OUTER" ),
				AComp->GetOwner() ? *( AComp->GetOwner()->GetPathName() ) : TEXT( "NO OWNER" ) );
		}
		else
		{
			Ar.Logf( TEXT( "    %4d - %s, %s" ), 
				ASIndex, 
				ActiveSound->Sound ? *( ActiveSound->Sound->GetPathName() ) : TEXT( "NO SOUND" ),
				TEXT( "NO COMPONENT" ) );
		}
	}
	return true;
}

bool FAudioDevice::HandleListSoundDurationsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf(TEXT( "Sound,Duration,Channels" ) );
	for( TObjectIterator<USoundWave> It; It; ++It )
	{
		USoundWave* SoundWave = *It;
		Ar.Logf(TEXT( "%s,%f,%i" ), *SoundWave->GetPathName(), SoundWave->Duration, SoundWave->NumChannels );
	}
	return true;
}


bool FAudioDevice::HandlePlaySoundCueCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Stop any existing sound playing
	if( !TestAudioComponent.IsValid() )
	{
		TestAudioComponent = NewObject<UAudioComponent>();
	}

	UAudioComponent* AudioComp = TestAudioComponent.Get();
	if( AudioComp != NULL )
	{
		AudioComp->Stop();

		// Load up an arbitrary cue
		USoundCue* Cue = LoadObject<USoundCue>( NULL, Cmd, NULL, LOAD_None, NULL );
		if( Cue != NULL )
		{
			AudioComp->Sound = Cue;
			AudioComp->bAllowSpatialization = false;
			AudioComp->bAutoDestroy = true;
			AudioComp->Play();

			TArray<USoundNodeWavePlayer*> WavePlayers;
			Cue->RecursiveFindNode<USoundNodeWavePlayer>( Cue->FirstNode, WavePlayers );
			for( int32 i = 0; i < WavePlayers.Num(); ++i )
			{
				USoundWave* SoundWave = WavePlayers[ i ]->SoundWave;
				if (SoundWave)
				{
					SoundWave->LogSubtitle( Ar );
				}
			}
		}
	}
	return true;
}

bool FAudioDevice::HandlePlaySoundWaveCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Stop any existing sound playing
	if( !TestAudioComponent.IsValid() )
	{
		TestAudioComponent = NewObject<UAudioComponent>();
	}

	UAudioComponent* AudioComp = TestAudioComponent.Get();
	if( AudioComp != NULL )
	{
		AudioComp->Stop();

		// Load up an arbitrary wave
		USoundWave* Wave = LoadObject<USoundWave>( NULL, Cmd, NULL, LOAD_None, NULL );
		if( Wave != NULL )
		{
			AudioComp->Sound = Wave;
			AudioComp->bAllowSpatialization = false;
			AudioComp->bAutoDestroy = true;
			AudioComp->Play();

			Wave->LogSubtitle( Ar );
		}
	}
	return true;
}

bool FAudioDevice::HandleSetBaseSoundMixCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Ar.Logf( TEXT( "Setting base sound mix '%s'" ), Cmd );
	FName NewMix = FName( Cmd );
	USoundMix* SoundMix = NULL;

	for( TObjectIterator<USoundMix> It; It; ++It )
	{
		if (NewMix == It->GetFName())
		{
			SoundMix = *It;
			break;
		}
	}

	if (SoundMix)
	{
		SetBaseSoundMix( SoundMix );
	}
	else
	{
		Ar.Logf(TEXT("Unknown SoundMix: %s"), *NewMix.ToString());
	}
	return true;
}

bool FAudioDevice::HandleIsolateDryAudioCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "Dry audio isolated" ) );
	DebugState = DEBUGSTATE_IsolateDryAudio;
	return true;
}

bool FAudioDevice::HandleIsolateReverbCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "Reverb audio isolated" ) );
	DebugState = DEBUGSTATE_IsolateReverb;
	return true;
}

bool FAudioDevice::HandleTestLPFCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "LPF set to max for all sources" ) );
	DebugState =  DEBUGSTATE_TestLPF;
	return true;
}

bool FAudioDevice::HandleTestStereoBleedCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "StereoBleed set to max for all sources" ) );
	DebugState =  DEBUGSTATE_TestStereoBleed;
	return true;
}

bool FAudioDevice::HandleTestLFEBleedCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "LFEBleed set to max for all sources" ) );
	DebugState = DEBUGSTATE_TestLFEBleed;
	return true;
}

bool FAudioDevice::HandleDisableLPFCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "LPF disabled for all sources" ) );
	DebugState = DEBUGSTATE_DisableLPF;
	return true;
}

bool FAudioDevice::HandleDisableRadioCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	EnableRadioEffect(false);
	return true;
}

bool FAudioDevice::HandleEnableRadioCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	EnableRadioEffect(true);
	return true;
}

bool FAudioDevice::HandleResetSoundStateCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "All volumes reset to their defaults; all test filters removed" ) );
	DebugState = DEBUGSTATE_None;
	return true;
}

bool FAudioDevice::HandleToggleSpatializationExtensionCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	bSpatializationExtensionEnabled = !bSpatializationExtensionEnabled;
	return true;
}

bool FAudioDevice::HandleEnableHRTFForAllCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	bHRTFEnabledForAll = !bHRTFEnabledForAll;
	return true;
}

bool FAudioDevice::HandleSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Apply the solo to the given device
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetSoloDevice(DeviceHandle);
	}
	return true;
}

bool FAudioDevice::HandleClearSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetSoloDevice(INDEX_NONE);
	}
	return true;
}

#endif // !UE_BUILD_SHIPPING

EDebugState FAudioDevice::GetMixDebugState( void )
{
	return( ( EDebugState )DebugState );
}

bool FAudioDevice::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !UE_BUILD_SHIPPING
	if( FParse::Command( &Cmd, TEXT( "DumpSoundInfo" ) ) )
	{
		HandleDumpSoundInfoCommand( Cmd, Ar );
	}
	if( FParse::Command( &Cmd, TEXT( "ListSounds" ) ) )
	{
		return HandleListSoundsCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ListWaves" ) ) )
	{
		return HandleListWavesCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ListSoundClasses" ) ) )
	{
		return HandleListSoundClassesCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ShowSoundClassHierarchy" ) ) )
	{
		return HandleShowSoundClassHierarchyCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ListSoundClassVolumes" ) ) )
	{
		return HandleListSoundClassVolumesCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ListAudioComponents" ) ) )
	{
		return HandleListAudioComponentsCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ListSoundDurations" ) ) )
	{
		return HandleListSoundDurationsCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "PlaySoundCue" ) ) )
	{
		return HandlePlaySoundCueCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "PlaySoundWave" ) ) )
	{
		return HandlePlaySoundWaveCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "SetBaseSoundMix" ) ) )
	{
		return HandleSetBaseSoundMixCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "IsolateDryAudio" ) ) )
	{
		return HandleIsolateDryAudioCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "IsolateReverb" ) ) )
	{
		return HandleIsolateReverbCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "TestLPF" ) ) )
	{
		return HandleTestLPFCommand( Cmd, Ar ); 
	}
	else if( FParse::Command( &Cmd, TEXT( "TestStereoBleed" ) ) )
	{
		return HandleTestStereoBleedCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "TestLFEBleed" ) ) )
	{
		return HandleTestLPFCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "DisableLPF" ) ) )
	{
		return HandleDisableLPFCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "DisableRadio" ) ) )
	{
		return HandleDisableRadioCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "EnableRadio" ) ) )
	{
		return HandleEnableRadioCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT( "ResetSoundState" ) ) )
	{
		return HandleResetSoundStateCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleSpatExt")))
	{
		return HandleToggleSpatializationExtensionCommand( Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleHRTFForAll")))
	{
		return HandleEnableHRTFForAllCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("SoloAudio")))
	{
		return HandleSoloCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ClearSoloAudio")))
	{
		return HandleClearSoloCommand(Cmd, Ar);
	}
#endif // !UE_BUILD_SHIPPING

	return false;
}

void FAudioDevice::InitSoundClasses( void )
{
	// Reset the maps of sound class properties
	for( TObjectIterator<USoundClass> It; It; ++It )
	{
		USoundClass* SoundClass = *It;
		FSoundClassProperties& Properties = SoundClasses.Add( SoundClass, SoundClass->Properties );
	}

	// Propagate the properties down the hierarchy 
	ParseSoundClasses();
}

void FAudioDevice::InitSoundSources( void )
{
	if (Sources.Num() == 0)
	{
		// now create platform specific sources
		for (int32 SourceIndex = 0; SourceIndex < MaxChannels; SourceIndex++)
		{
			FSoundSource* Source = CreateSoundSource();
			Source->InitializeSourceEffects(SourceIndex);

			Sources.Add(Source);
			FreeSources.Add(Source);
		}
	}
}

void FAudioDevice::SetDefaultBaseSoundMix( USoundMix* SoundMix )
{
	if (SoundMix == NULL)
	{
		const FStringAssetReference DefaultBaseSoundMixName = GetDefault<UAudioSettings>()->DefaultBaseSoundMix;
		if (DefaultBaseSoundMixName.IsValid())
		{			
			SoundMix = LoadObject<USoundMix>(NULL, *DefaultBaseSoundMixName.ToString());
		}
	}

	DefaultBaseSoundMix = SoundMix;
	SetBaseSoundMix(SoundMix);
}

void FAudioDevice::RemoveSoundMix( USoundMix* SoundMix )
{
	if( SoundMix )
	{
		// Not sure if we will ever destroy the default base SoundMix
		if( SoundMix == DefaultBaseSoundMix )
		{
			DefaultBaseSoundMix = NULL;
		}

		ClearSoundMix(SoundMix);

		// Try setting to global default if base SoundMix has been cleared
		if( BaseSoundMix == NULL )
		{
			SetBaseSoundMix(DefaultBaseSoundMix);
		}
	}
}


void FAudioDevice::RecurseIntoSoundClasses( USoundClass* CurrentClass, FSoundClassProperties& ParentProperties )
{
	// Iterate over all child nodes and recurse.
	for( int32 ChildIndex = 0; ChildIndex < CurrentClass->ChildClasses.Num(); ChildIndex++ )
	{
		// Look up class and propagated properties.
		USoundClass* ChildClass = CurrentClass->ChildClasses[ChildIndex];
		FSoundClassProperties* Properties = SoundClasses.Find(ChildClass);

		// Should never be NULL for a properly set up tree.
		if( ChildClass )
		{
			if (Properties)
			{
				Properties->Volume *= ParentProperties.Volume;
				Properties->Pitch *= ParentProperties.Pitch;
				Properties->bIsUISound |= ParentProperties.bIsUISound;
				Properties->bIsMusic |= ParentProperties.bIsMusic;

				// Not all values propagate equally...
				// VoiceCenterChannelVolume, RadioFilterVolume, RadioFilterVolumeThreshold, bApplyEffects, BleedStereo, bReverb, and bCenterChannelOnly do not propagate (sub-classes can be non-zero even if parent class is zero)

				// ... and recurse into child nodes.
				RecurseIntoSoundClasses( ChildClass, *Properties );
			}
			else
			{
				UE_LOG(LogAudio, Warning, TEXT( "Couldn't find child class properties - sound class functionality will not work correctly! CurrentClass: %s ChildClass: %s" ), *CurrentClass->GetFullName(), *ChildClass->GetFullName() );
			}
		}
	}
}

void FAudioDevice::UpdateHighestPriorityReverb()
{
	for (auto It = ActivatedReverbs.CreateConstIterator(); It; ++It)
	{
		if (!HighestPriorityReverb || It.Value().Priority > HighestPriorityReverb->Priority)
		{
			HighestPriorityReverb = &It.Value();
		}
	}
}

void FAudioDevice::ParseSoundClasses()
{
	TArray<USoundClass*> RootSoundClasses;

	// Reset to known state - preadjusted by set class volume calls
	for( TMap<USoundClass*, FSoundClassProperties>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			It.Value() = SoundClass->Properties;
			if (SoundClass->ParentClass == NULL)
			{
				RootSoundClasses.Add(SoundClass);
			}
		}
	}

	for (int32 RootIndex = 0; RootIndex < RootSoundClasses.Num(); ++RootIndex)
	{
		USoundClass* RootSoundClass = RootSoundClasses[RootIndex];

		FSoundClassProperties* RootSoundClassProperties = SoundClasses.Find( RootSoundClass );
		if( RootSoundClass && RootSoundClassProperties )
		{
			// Follow the tree.
			RecurseIntoSoundClasses( RootSoundClass, *RootSoundClassProperties );
		}
	}
}


void FAudioDevice::RecursiveApplyAdjuster( const FSoundClassAdjuster& InAdjuster, USoundClass* InSoundClass )
{
	// Find the sound class properties so we can apply the adjuster
	// and find the sound class so we can recurse through the children
	FSoundClassProperties* Properties = SoundClasses.Find( InSoundClass );
	if( InSoundClass && Properties )
	{
		// Adjust this class
		Properties->Volume *= InAdjuster.VolumeAdjuster;
		Properties->Pitch *= InAdjuster.PitchAdjuster;
		Properties->VoiceCenterChannelVolume *= InAdjuster.VoiceCenterChannelVolumeAdjuster;

		// Recurse through this classes children
		for( int32 ChildIdx = 0; ChildIdx < InSoundClass->ChildClasses.Num(); ++ChildIdx )
		{
			if (InSoundClass->ChildClasses[ ChildIdx ])
			{
				RecursiveApplyAdjuster( InAdjuster, InSoundClass->ChildClasses[ ChildIdx ] );
			}
		}
	}
	else
	{
		UE_LOG(LogAudio, Warning, TEXT( "Sound class '%s' does not exist" ), *InSoundClass->GetName() );
	}
}

bool FAudioDevice::ApplySoundMix( USoundMix* NewMix, FSoundMixState* SoundMixState )
{
	if( NewMix && SoundMixState )
	{
		UE_LOG(LogAudio, Log, TEXT( "FAudioDevice::ApplySoundMix(): %s" ), *NewMix->GetName() );

		SoundMixState->StartTime = FApp::GetCurrentTime();
		SoundMixState->FadeInStartTime = SoundMixState->StartTime + NewMix->InitialDelay;
		SoundMixState->FadeInEndTime = SoundMixState->FadeInStartTime + NewMix->FadeInTime;
		SoundMixState->FadeOutStartTime = -1.0;
		SoundMixState->EndTime = -1.0;
		if( NewMix->Duration >= 0.0f )
		{
			SoundMixState->FadeOutStartTime = SoundMixState->FadeInEndTime + NewMix->Duration;
			SoundMixState->EndTime = SoundMixState->FadeOutStartTime + NewMix->FadeOutTime;
		}
		SoundMixState->InterpValue = 0.0f;

		ApplyClassAdjusters(NewMix,SoundMixState->InterpValue);

		return( true );
	}

	return( false );
}

void FAudioDevice::UpdateSoundMix(class USoundMix* SoundMix, FSoundMixState* SoundMixState)
{
	// If this SoundMix will automatically end, add some more time
	if (SoundMixState->FadeOutStartTime >= 0.0)
	{
		if (SoundMixState->CurrentState == ESoundMixState::FadingOut)
		{
			// In process of fading out, need to adjust timing to fade in again.
			SoundMixState->FadeInStartTime = FApp::GetCurrentTime() - (SoundMixState->InterpValue * SoundMix->FadeInTime);
			SoundMixState->FadeInEndTime = SoundMixState->FadeInStartTime + SoundMix->FadeInTime;
			SoundMixState->FadeOutStartTime = -1.0;
			SoundMixState->EndTime = -1.0;
			if( SoundMix->Duration >= 0.0f )
			{
				SoundMixState->FadeOutStartTime = SoundMixState->FadeInEndTime + SoundMix->Duration;
				SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
			}

			// Might have already started fading EQ effect so try setting again
			Effects->SetMixSettings( SoundMix );
		}
		else if (SoundMixState->CurrentState == ESoundMixState::Active)
		{
			// Duration may be -1, this guarantees the fade will at least start at the current time
			double NewFadeOutStartTime = FMath::Max(FApp::GetCurrentTime(), FApp::GetCurrentTime() + SoundMix->Duration);
			if (NewFadeOutStartTime > SoundMixState->FadeOutStartTime)
			{
				SoundMixState->FadeOutStartTime = NewFadeOutStartTime;
				SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
			}
		}
	}
}

void FAudioDevice::UpdatePassiveSoundMixModifiers(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex)
{
	TArray<USoundMix*> CurrPassiveSoundMixModifiers;

	// Find all passive SoundMixes from currently active wave instances
	for( int32 WaveIndex = FirstActiveIndex; WaveIndex < WaveInstances.Num(); WaveIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[WaveIndex];
		if (WaveInstance)
		{
			USoundClass* SoundClass = WaveInstance->SoundClass;
			if( SoundClass ) 
			{
				const float WaveInstanceActualVolume = WaveInstance->GetActualVolume();
				// Check each SoundMix individually for volume levels
				for (const FPassiveSoundMixModifier& PassiveSoundMixModifier : SoundClass->PassiveSoundMixModifiers)
				{
					if (WaveInstanceActualVolume >= PassiveSoundMixModifier.MinVolumeThreshold && WaveInstanceActualVolume <= PassiveSoundMixModifier.MaxVolumeThreshold)
					{
						CurrPassiveSoundMixModifiers.AddUnique( PassiveSoundMixModifier.SoundMix );
					}
				}
			}
		}
	}

	// Push SoundMixes that weren't previously active
	for (USoundMix* CurrPassiveSoundMixModifier : CurrPassiveSoundMixModifiers)
	{
		if (PrevPassiveSoundMixModifiers.Find(CurrPassiveSoundMixModifier) == INDEX_NONE)
		{
			PushSoundMixModifier(CurrPassiveSoundMixModifier, true);
		}
	}

	// Pop SoundMixes that are no longer active
	for (int32 MixIdx = PrevPassiveSoundMixModifiers.Num() - 1; MixIdx >= 0; MixIdx--)
	{
		USoundMix* PrevPassiveSoundMixModifier = PrevPassiveSoundMixModifiers[MixIdx];
		if (CurrPassiveSoundMixModifiers.Find(PrevPassiveSoundMixModifier) == INDEX_NONE)
		{
			PopSoundMixModifier(PrevPassiveSoundMixModifier, true);
		}
	}

	PrevPassiveSoundMixModifiers = CurrPassiveSoundMixModifiers;
}

bool FAudioDevice::TryClearingSoundMix(USoundMix* SoundMix, FSoundMixState* SoundMixState)
{
	if (SoundMix && SoundMixState)
	{
		if (SoundMixState->ActiveRefCount == 0 && SoundMixState->PassiveRefCount == 0 && SoundMixState->IsBaseSoundMix == false)
		{
			// do whatever is needed to remove influence of this SoundMix
			if (SoundMix->FadeOutTime > 0.0)
			{
				if (SoundMixState->CurrentState == ESoundMixState::Inactive)
				{
					// Haven't even started fading up, can kill immediately
					ClearSoundMix(SoundMix);
				}
				else if (SoundMixState->CurrentState == ESoundMixState::FadingIn)
				{
					// Currently fading up, force fade in to complete and start fade out from current fade level
					SoundMixState->FadeOutStartTime = FApp::GetCurrentTime() - ((1.0f - SoundMixState->InterpValue) * SoundMix->FadeOutTime);
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
					SoundMixState->StartTime = SoundMixState->FadeInStartTime = SoundMixState->FadeInEndTime = SoundMixState->FadeOutStartTime - 1.0;

					TryClearingEQSoundMix(SoundMix);
				}
				else if (SoundMixState->CurrentState == ESoundMixState::Active)
				{
					// SoundMix active, start fade out early
					SoundMixState->FadeOutStartTime = FApp::GetCurrentTime();
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;

					TryClearingEQSoundMix(SoundMix);
				}
				else
				{
					// Already fading out, do nothing
				}
			}
			else
			{
				ClearSoundMix(SoundMix);
			}
			return true;
		}
	}

	return false;
}

bool FAudioDevice::TryClearingEQSoundMix(class USoundMix* SoundMix)
{
	if (SoundMix && Effects->GetCurrentEQMix() == SoundMix)
	{
		USoundMix* NextEQMix = FindNextHighestEQPrioritySoundMix(SoundMix);
		if (NextEQMix)
		{
			// Need to ignore priority when setting as it will be less than current
			Effects->SetMixSettings(NextEQMix, true);
		}
		else
		{
			Effects->ClearMixSettings();
		}

		return true;
	}

	return false;
}

USoundMix* FAudioDevice::FindNextHighestEQPrioritySoundMix(USoundMix* IgnoredSoundMix)
{
	// find the mix with the next highest priority that was added first
	USoundMix* NextEQMix = NULL;
	FSoundMixState* NextState = NULL;

	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		if (It.Key() != IgnoredSoundMix && It.Value().CurrentState < ESoundMixState::FadingOut
			&& (NextEQMix == NULL
				|| (It.Key()->EQPriority > NextEQMix->EQPriority
					|| (It.Key()->EQPriority == NextEQMix->EQPriority
						&& It.Value().StartTime < NextState->StartTime)
					)
				)
			)
		{
			NextEQMix = It.Key();
			NextState = &(It.Value());
		}
	}

	return NextEQMix;
}

void FAudioDevice::ClearSoundMix(class USoundMix* SoundMix)
{
	if( SoundMix == BaseSoundMix )
	{
		BaseSoundMix = NULL;
	}
	SoundMixModifiers.Remove( SoundMix );
	PrevPassiveSoundMixModifiers.Remove( SoundMix );

	TryClearingEQSoundMix(SoundMix);
}

void FAudioDevice::ApplyClassAdjusters(USoundMix* SoundMix, float InterpValue)
{
	if( SoundMix )
	{
		// Adjust the sound class properties non recursively
		TArray<FSoundClassAdjuster>& Adjusters = SoundMix->SoundClassEffects;

		for( int32 i = 0; i < Adjusters.Num(); i++ )
		{
			if (Adjusters[ i ].SoundClassObject)
			{
				if( Adjusters[ i ].bApplyToChildren )
				{
					// Take a copy of this adjuster and pre-calculate interpolation
					FSoundClassAdjuster InterpolatedAdjuster = Adjusters[ i ];
					InterpolatedAdjuster.VolumeAdjuster = InterpolateAdjuster(InterpolatedAdjuster.VolumeAdjuster, InterpValue);
					InterpolatedAdjuster.PitchAdjuster = InterpolateAdjuster(InterpolatedAdjuster.PitchAdjuster, InterpValue);
					InterpolatedAdjuster.VoiceCenterChannelVolumeAdjuster = InterpolateAdjuster(InterpolatedAdjuster.VoiceCenterChannelVolumeAdjuster, InterpValue);

					// Apply the adjuster the sound class specified by the adjuster and all its children
					RecursiveApplyAdjuster( InterpolatedAdjuster, Adjusters[ i ].SoundClassObject );
				}
				else
				{
					// Apply the adjuster to only the sound class specified by the adjuster
					FSoundClassProperties* Properties = SoundClasses.Find( Adjusters[ i ].SoundClassObject );
					if( Properties )
					{
						Properties->Volume *= InterpolateAdjuster(Adjusters[ i ].VolumeAdjuster, InterpValue);
						Properties->Pitch *= InterpolateAdjuster(Adjusters[ i ].PitchAdjuster, InterpValue);
						Properties->VoiceCenterChannelVolume *= InterpolateAdjuster(Adjusters[ i ].VoiceCenterChannelVolumeAdjuster, InterpValue);
					}
					else
					{
						UE_LOG(LogAudio, Warning, TEXT( "Sound class '%s' does not exist" ), *Adjusters[ i ].SoundClassObject->GetName() );
					}
				}
			}
		}
	}
}


void FAudioDevice::UpdateSoundClassProperties()
{
	// Remove SoundMix modifications and propagate the properties down the hierarchy
	ParseSoundClasses();

	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		FSoundMixState* SoundMixState = &(It.Value());

		// Initial delay before mix is applied
		if( FApp::GetCurrentTime() >= SoundMixState->StartTime && FApp::GetCurrentTime() < SoundMixState->FadeInStartTime )
		{
			SoundMixState->InterpValue = 0.0f;
			SoundMixState->CurrentState = ESoundMixState::Inactive;
		}
		else if( FApp::GetCurrentTime() >= SoundMixState->FadeInStartTime && FApp::GetCurrentTime() < SoundMixState->FadeInEndTime )
		{
			// Work out the fade in portion
			SoundMixState->InterpValue = ( float )( ( FApp::GetCurrentTime() - SoundMixState->FadeInStartTime ) / ( SoundMixState->FadeInEndTime - SoundMixState->FadeInStartTime ) );
			SoundMixState->CurrentState = ESoundMixState::FadingIn;
		}
		else if( FApp::GetCurrentTime() >= SoundMixState->FadeInEndTime
			&& ( SoundMixState->IsBaseSoundMix || SoundMixState->PassiveRefCount > 0 || SoundMixState->FadeOutStartTime < 0.0 || FApp::GetCurrentTime() < SoundMixState->FadeOutStartTime ) )
		{
			// .. ensure the full mix is applied between the end of the fade in time and the start of the fade out time
			// or if SoundMix is the base or active via a passive push - ignores duration.
			SoundMixState->InterpValue = 1.0f;
			SoundMixState->CurrentState = ESoundMixState::Active;
		}
		else if( FApp::GetCurrentTime() >= SoundMixState->FadeOutStartTime && FApp::GetCurrentTime() < SoundMixState->EndTime )
		{
			// Work out the fade out portion
			SoundMixState->InterpValue = 1.0f - ( float )( ( FApp::GetCurrentTime() - SoundMixState->FadeOutStartTime ) / ( SoundMixState->EndTime - SoundMixState->FadeOutStartTime ) );
			if( SoundMixState->CurrentState != ESoundMixState::FadingOut )
			{
				// Start fading EQ at same time
				SoundMixState->CurrentState = ESoundMixState::FadingOut;
				TryClearingEQSoundMix(It.Key());
			}
		}
		else
		{
			check( SoundMixState->EndTime >= 0.0 && FApp::GetCurrentTime() >= SoundMixState->EndTime );
			// Clear the effect of this SoundMix - may need to revisit for passive
			SoundMixState->InterpValue = 0.0f;
			SoundMixState->CurrentState = ESoundMixState::AwaitingRemoval;
		}

		ApplyClassAdjusters(It.Key(), SoundMixState->InterpValue);

		if( SoundMixState->CurrentState == ESoundMixState::AwaitingRemoval )
		{
			ClearSoundMix(It.Key());
		}
	}
}


float FListener::Interpolate( const double EndTime )
{
	if( FApp::GetCurrentTime() < InteriorStartTime )
	{
		return( 0.0f );
	}

	if( FApp::GetCurrentTime() >= EndTime )
	{
		return( 1.0f );
	}

	float InterpValue = ( float )( ( FApp::GetCurrentTime() - InteriorStartTime ) / ( EndTime - InteriorStartTime ) );
	return( InterpValue );
}


void FListener::UpdateCurrentInteriorSettings()
{
	// Store the interpolation value, not the actual value
	InteriorVolumeInterp = Interpolate( InteriorEndTime );
	ExteriorVolumeInterp = Interpolate( ExteriorEndTime );
	InteriorLPFInterp = Interpolate( InteriorLPFEndTime );
	ExteriorLPFInterp = Interpolate( ExteriorLPFEndTime );
}

void FAudioDevice::InvalidateCachedInteriorVolumes() const
{
	for (FActiveSound* ActiveSound : ActiveSounds)
	{
		ActiveSound->bGotInteriorSettings = false;
	}
}

void FListener::ApplyInteriorSettings( class AAudioVolume* InVolume, const FInteriorSettings& Settings )
{
	if( InVolume != Volume || Settings != InteriorSettings)
	{
		// Use previous/ current interpolation time if we're transitioning to the default worldsettings zone.
		InteriorStartTime = FApp::GetCurrentTime();
		InteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorTime : Settings.InteriorTime);
		ExteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorTime : Settings.ExteriorTime);
		InteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorLPFTime : Settings.InteriorLPFTime);
		ExteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorLPFTime : Settings.ExteriorLPFTime);

		Volume = InVolume;
		InteriorSettings = Settings;
	}
}


void FAudioDevice::SetListener( const int32 InViewportIndex, const FTransform& InListenerTransform, const float InDeltaSeconds, class AAudioVolume* Volume, const FInteriorSettings& InteriorSettings )
{
	FTransform ListenerTransform = InListenerTransform;
	
	if (!ensureMsgf(ListenerTransform.IsValid(), TEXT("Invalid listener transform provided to AudioDevice")))
	{
		// If we have a bad transform give it something functional if totally wrong
		ListenerTransform = FTransform::Identity;
	}

	if( InViewportIndex >= Listeners.Num() )
	{
		UE_LOG(LogAudio, Log, TEXT( "Resizing Listeners array: %d -> %d" ), Listeners.Num(), InViewportIndex );
		Listeners.AddZeroed( InViewportIndex - Listeners.Num() + 1 );
	}

	Listeners[ InViewportIndex ].Velocity = InDeltaSeconds > 0.f ? 
											(ListenerTransform.GetTranslation() - Listeners[ InViewportIndex ].Transform.GetTranslation()) / InDeltaSeconds
											: FVector::ZeroVector;

	Listeners[ InViewportIndex ].Transform = ListenerTransform;

	Listeners[ InViewportIndex ].ApplyInteriorSettings(Volume, InteriorSettings);
}


bool FAudioDevice::SetBaseSoundMix( USoundMix* NewMix )
{
	if( NewMix && NewMix != BaseSoundMix )
	{
		USoundMix* OldBaseSoundMix = BaseSoundMix;
		BaseSoundMix = NewMix;

		if( OldBaseSoundMix )
		{
			FSoundMixState* OldBaseState = SoundMixModifiers.Find(OldBaseSoundMix);
			check(OldBaseState);
			OldBaseState->IsBaseSoundMix = false;
			TryClearingSoundMix(OldBaseSoundMix, OldBaseState);
		}

		// Check whether this SoundMix is already active
		FSoundMixState* ExistingState = SoundMixModifiers.Find(NewMix);
		if( !ExistingState )
		{
			// First time this mix has been set - add it and setup mix modifications
			ExistingState = &SoundMixModifiers.Add(NewMix, FSoundMixState());

			// Setup SoundClass modifications
			ApplySoundMix(NewMix, ExistingState);

			// Use it to set EQ Settings, which will check its priority
			Effects->SetMixSettings( NewMix );
		}

		ExistingState->IsBaseSoundMix = true;

		return true;
	}

	return false;
}

void FAudioDevice::PushSoundMixModifier(USoundMix* SoundMix, bool bIsPassive)
{
	if (SoundMix)
	{
		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (!SoundMixState)
		{
			// First time this mix has been pushed - add it and setup mix modifications
			SoundMixState = &SoundMixModifiers.Add(SoundMix, FSoundMixState());

			// Setup SoundClass modifications
			ApplySoundMix(SoundMix, SoundMixState);

			// Use it to set EQ Settings, which will check its priority
			Effects->SetMixSettings( SoundMix );
		}
		else
		{
			UpdateSoundMix(SoundMix, SoundMixState);
		}

		// Increase the relevant ref count - we know pointer exists by this point
		if (bIsPassive)
		{
			SoundMixState->PassiveRefCount++;
		}
		else
		{
			SoundMixState->ActiveRefCount++;
		}
	}
}

void FAudioDevice::PopSoundMixModifier(USoundMix* SoundMix, bool bIsPassive)
{
	if (SoundMix)
	{
		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (SoundMixState)
		{
			if (bIsPassive && SoundMixState->PassiveRefCount > 0)
			{
				SoundMixState->PassiveRefCount--;
				if (SoundMixState->PassiveRefCount == 0)
				{
					// Check whether Fade out time was previously set and reset it to current time
					if (SoundMixState->FadeOutStartTime >= 0.0 && FApp::GetCurrentTime() > SoundMixState->FadeOutStartTime)
					{
						SoundMixState->FadeOutStartTime = FApp::GetCurrentTime();
						SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
					}
				}
			}
			else if (!bIsPassive && SoundMixState->ActiveRefCount > 0)
			{
				SoundMixState->ActiveRefCount--;
			}

			TryClearingSoundMix(SoundMix, SoundMixState);
		}
	}
}

void FAudioDevice::ClearSoundMixModifier(USoundMix* SoundMix)
{
	if (SoundMix)
	{
		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (SoundMixState)
		{
			SoundMixState->ActiveRefCount = 0;

			TryClearingSoundMix(SoundMix, SoundMixState);
		}
	}
}

void FAudioDevice::ClearSoundMixModifiers()
{
	// Clear all sound mix modifiers
	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		ClearSoundMixModifier(It.Key());
	}
}

void FAudioDevice::ActivateReverbEffect(class UReverbEffect* ReverbEffect, FName TagName, float Priority, float Volume, float FadeTime)
{
	FActivatedReverb* ExistingReverb = ActivatedReverbs.Find(TagName);

	if (ExistingReverb)
	{
		float OldPriority = ExistingReverb->Priority;
		ExistingReverb->ReverbSettings.ReverbEffect = ReverbEffect;
		ExistingReverb->ReverbSettings.Volume = Volume;
		ExistingReverb->ReverbSettings.FadeTime = FadeTime;
		ExistingReverb->Priority = Priority;

		if (OldPriority != Priority)
		{
			UpdateHighestPriorityReverb();
		}
	}
	else
	{
		ExistingReverb = &ActivatedReverbs.Add(TagName, FActivatedReverb());
		ExistingReverb->ReverbSettings.ReverbEffect = ReverbEffect;
		ExistingReverb->ReverbSettings.Volume = Volume;
		ExistingReverb->ReverbSettings.FadeTime = FadeTime;
		ExistingReverb->Priority = Priority;

		UpdateHighestPriorityReverb();
	}
}

void FAudioDevice::DeactivateReverbEffect(FName TagName)
{
	FActivatedReverb* ExistingReverb = ActivatedReverbs.Find(TagName);
	if (ExistingReverb)
	{
		if (ExistingReverb == HighestPriorityReverb)
		{
			HighestPriorityReverb = NULL;
		}

		ActivatedReverbs.Remove(TagName);

		if (HighestPriorityReverb == NULL)
		{
			UpdateHighestPriorityReverb();
		}
	}
}

void FAudioDevice::SetReverbSettings( class AAudioVolume* Volume, const FReverbSettings& ReverbSettings )
{
	const FReverbSettings* ActivatedReverb = &ReverbSettings;
	if (HighestPriorityReverb && (!Volume || HighestPriorityReverb->Priority > Volume->Priority))
	{
		ActivatedReverb = &HighestPriorityReverb->ReverbSettings;
	}

	Effects->SetReverbSettings( *ActivatedReverb );
}


void* FAudioDevice::InitEffect( FSoundSource* Source )
{
	return( Effects->InitEffect( Source ) );
}


void* FAudioDevice::UpdateEffect( FSoundSource* Source )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateEffects );

	return( Effects->UpdateEffect( Source ) );
}


void FAudioDevice::DestroyEffect( FSoundSource* Source )
{
	return( Effects->DestroyEffect( Source ) );
}


void FAudioDevice::HandlePause( bool bGameTicking, bool bGlobalPause )
{
	// Pause all sounds if transitioning to pause mode.
	if( !bGameTicking && bGameWasTicking )
	{
		for( int32 i = 0; i < Sources.Num(); i++ )
		{
			FSoundSource* Source = Sources[ i ];
			if( bGlobalPause || Source->IsGameOnly() )
			{
				Source->Pause();
			}
		}
	}
	// Unpause all sounds if transitioning back to game.
	else if( bGameTicking && !bGameWasTicking )
	{
		for( int32 i = 0; i < Sources.Num(); i++ )
		{
			FSoundSource* Source = Sources[ i ];
			if( bGlobalPause || Source->IsGameOnly() )
			{
				Source->Play();
			}
		}
	}

	bGameWasTicking = bGameTicking;
}


int32 FAudioDevice::GetSortedActiveWaveInstances(TArray<FWaveInstance*>& WaveInstances, const ESortedActiveWaveGetType::Type GetType)
{
	SCOPE_CYCLE_COUNTER( STAT_AudioGatherWaveInstances );

	// Tick all the active audio components.  Use a copy as some operations may remove elements from the list, but we want
	// to evaluate in the order they were added
	TArray<FActiveSound*> ActiveSoundsCopy = ActiveSounds;
	for( int32 i = 0; i < ActiveSoundsCopy.Num(); ++i )
	{
		FActiveSound* ActiveSound = ActiveSoundsCopy[i];

		if( !ActiveSound->Sound )
		{
			// No sound - cleanup and remove
			ActiveSound->Stop(this);
		}
		// If the world scene allows audio - tick wave instances.
		else if( ActiveSound->World == NULL || ActiveSound->World->AllowAudioPlayback() )
		{
			const float Duration = ActiveSound->Sound->GetDuration();
			// Divide by minimum pitch for longest possible duration
			if( Duration < INDEFINITELY_LOOPING_DURATION && ActiveSound->PlaybackTime > Duration / MIN_PITCH )
			{
				UE_LOG(LogAudio, Log, TEXT( "Sound stopped due to duration: %g > %g : %s %s" ), 
					ActiveSound->PlaybackTime, 
					Duration, 
					*ActiveSound->Sound->GetName(), 
					(ActiveSound->AudioComponent.IsValid() ? *ActiveSound->AudioComponent->GetName() : TEXT("NO COMPONENT")));
				ActiveSound->Stop(this);
			}
			else
			{
				// If not in game, do not advance sounds unless they are UI sounds.
				float UsedDeltaTime = FApp::GetDeltaTime();
				if (GetType == ESortedActiveWaveGetType::QueryOnly || (GetType == ESortedActiveWaveGetType::PausedUpdate && !ActiveSound->bIsUISound))
				{
					UsedDeltaTime = 0.0f;
				}

				ActiveSound->UpdateWaveInstances( this, WaveInstances, UsedDeltaTime );
			}
		}
	}

	// Helper function for "Sort" (higher priority sorts last).
	struct FCompareFWaveInstanceByPlayPriority
	{
		FORCEINLINE bool operator()( const FWaveInstance& A, const FWaveInstance& B) const
		{
			return A.PlayPriority < B.PlayPriority;
		}
	};

	// Sort by priority (lowest priority first).
	WaveInstances.Sort( FCompareFWaveInstanceByPlayPriority() );

	// Return the first audible waveinstance
	int32 FirstActiveIndex = FMath::Max( WaveInstances.Num() - MaxChannels, 0 );
	for( ; FirstActiveIndex < WaveInstances.Num(); FirstActiveIndex++ )
	{
		if( WaveInstances[ FirstActiveIndex ]->PlayPriority > KINDA_SMALL_NUMBER )
		{
			break;
		}
	}
	return( FirstActiveIndex );
}


void FAudioDevice::StopSources( TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex )
{
	// Touch sources that are high enough priority to play
	for( int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
		if( Source )
		{
			Source->LastUpdate = CurrentTick;

			// If they are still audible, mark them as such
			if( WaveInstance->PlayPriority > KINDA_SMALL_NUMBER )
			{
				Source->LastHeardUpdate = CurrentTick;
			}
		}
	}

	// Stop inactive sources, sources that no longer have a WaveInstance associated
	// or sources that need to be reset because Stop & Play were called in the same frame.
	for( int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
	{
		FSoundSource* Source = Sources[ SourceIndex ];

		if( Source->WaveInstance )
		{
#if STATS && WITH_EDITORONLY_DATA
			if( Source->UsesCPUDecompression() )
			{
				INC_DWORD_STAT( STAT_OggWaveInstances );
			}
#endif
			// Source was not high enough priority to play this tick
			if( Source->LastUpdate != CurrentTick )
			{
				Source->Stop();
			}
			// Source has been inaudible for several ticks
			else if( Source->LastHeardUpdate + AUDIOSOURCE_TICK_LONGEVITY < CurrentTick )
			{
				Source->Stop();
			}
		}
	}

	// Stop wave instances that are no longer playing due to priority reasons. This needs to happen AFTER
	// stopping sources as calling Stop on a sound source in turn notifies the wave instance of a buffer
	// being finished which might reset it being finished.
	for( int32 InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		WaveInstance->StopWithoutNotification();
		// UE_LOG(LogAudio, Log, TEXT( "SoundStoppedWithoutNotification due to priority reasons: %s" ), *WaveInstance->WaveData->GetName() );
	}

#if STATS
	uint32 AudibleInactiveSounds = 0;
	// Count how many sounds are not being played but were audible
	for( int32 InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		if( WaveInstance->GetActualVolume() > 0.1f )
		{
			AudibleInactiveSounds++;
		}
	}
	SET_DWORD_STAT( STAT_AudibleWavesDroppedDueToPriority, AudibleInactiveSounds );
#endif
}


void FAudioDevice::StartSources( TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex, bool bGameTicking )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioStartSources );

	// Start sources as needed.
	for( int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];

		// Editor uses bIsUISound for sounds played in the browser.
		if(	bGameTicking || WaveInstance->bIsUISound )
		{
			FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
			if( !Source &&
			  ( !WaveInstance->IsStreaming() ||
				IStreamingManager::Get().GetAudioStreamingManager().CanCreateSoundSource(WaveInstance) ) )
			{
				check( FreeSources.Num() );
				Source = FreeSources.Pop();
				check( Source);

				// Try to initialize source.
				if( Source->Init( WaveInstance ) )
				{
					IStreamingManager::Get().GetAudioStreamingManager().AddStreamingSoundSource(Source);
					// Associate wave instance with it which is used earlier in this function.
					WaveInstanceSourceMap.Add( WaveInstance, Source );
					// Playback might be deferred till the end of the update function on certain implementations.
					Source->Play();

					//UE_LOG(LogAudio, Log, TEXT( "Playing: %s" ), *WaveInstance->WaveData->GetName() );
				}
				else
				{
					// This can happen if e.g. the USoundWave pointed to by the WaveInstance is not a valid sound file.
					// If we don't stop the wave file, it will continue to try initializing the file every frame, which is a perf hit
					WaveInstance->StopWithoutNotification();
					FreeSources.Add( Source );
				}
			}
			else if (Source)
			{
				Source->Update();
			}
			else
			{
				// This can happen if the streaming manager determines that this sound should not be started.
				// We stop the wave instance to prevent it from attempting to initialize every frame
				WaveInstance->StopWithoutNotification();
			}
		}
	}
}


void FAudioDevice::Update( bool bGameTicking )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateTime );

	// Start a new frame
	CurrentTick++;

	// Handle pause/unpause for the game and editor.
	HandlePause( bGameTicking );

	// Update the audio effects - reverb, EQ etc
	Effects->Update();

	// Gets the current state of the sound classes accounting for sound mix
	UpdateSoundClassProperties();

	// Gets the current state of the interior settings
	for (FListener& Listener : Listeners)
	{
		Listener.UpdateCurrentInteriorSettings();
	}

	if (Sources.Num())
	{
		// Kill any sources that have finished
		for( int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
		{
			// Source has finished playing (it's one shot)
			if( Sources[ SourceIndex ]->IsFinished() )
			{
				Sources[ SourceIndex ]->Stop();
			}
		}

		// Poll audio components for active wave instances (== paths in node tree that end in a USoundWave)
		TArray<FWaveInstance*> WaveInstances;
		int32 FirstActiveIndex = GetSortedActiveWaveInstances( WaveInstances, (bGameTicking ? ESortedActiveWaveGetType::FullUpdate : ESortedActiveWaveGetType::PausedUpdate));

		// Stop sources that need to be stopped, and touch the ones that need to be kept alive
		StopSources( WaveInstances, FirstActiveIndex );

		// Start and/or update any sources that have a high enough priority to play
		StartSources( WaveInstances, FirstActiveIndex, bGameTicking );

		// Check which sounds are active from these wave instances and update passive SoundMixes
		UpdatePassiveSoundMixModifiers( WaveInstances, FirstActiveIndex );

		INC_DWORD_STAT_BY( STAT_WaveInstances, WaveInstances.Num() );
		INC_DWORD_STAT_BY( STAT_AudioSources, MaxChannels - FreeSources.Num() );
		INC_DWORD_STAT_BY( STAT_WavesDroppedDueToPriority, FMath::Max( WaveInstances.Num() - MaxChannels, 0 ) );
		INC_DWORD_STAT_BY( STAT_ActiveSounds, ActiveSounds.Num() );
	}

	// now let the platform perform anything it needs to handle
	UpdateHardware();

#if !UE_BUILD_SHIPPING
	// Print statistics for first non initial load allocation.
	static bool bFirstTime = true;
	if (bFirstTime && CommonAudioPoolSize != 0)
	{
		bFirstTime = false;
		if (CommonAudioPoolFreeBytes != 0)
		{
			UE_LOG(LogAudio, Log, TEXT("Audio pool size mismatch by %d bytes. Please update CommonAudioPoolSize ini setting to %d to avoid waste!"),
				CommonAudioPoolFreeBytes, CommonAudioPoolSize - CommonAudioPoolFreeBytes );
		}
	}
#endif
}


void FAudioDevice::StopAllSounds( bool bShouldStopUISounds )
{
	for (int32 SoundIndex=ActiveSounds.Num() - 1; SoundIndex >= 0; --SoundIndex)
	{
		FActiveSound* ActiveSound = ActiveSounds[SoundIndex];

		if (bShouldStopUISounds)
		{
			ActiveSound->Stop(this);
		}
		// If we're allowing UI sounds to continue then first filter on the active sounds state
		else if (!ActiveSound->bIsUISound)
		{
			// Then iterate across the wave instances.  If any of the wave instances is not a UI sound
			// then we will stop the entire active sound because it makes less sense to leave it half
			// executing
			for (auto WaveInstanceIt(ActiveSound->WaveInstances.CreateConstIterator()); WaveInstanceIt; ++WaveInstanceIt)
			{
				FWaveInstance* WaveInstance = WaveInstanceIt.Value();
				if (WaveInstance && !WaveInstance->bIsUISound)
				{
					ActiveSound->Stop(this);
					break;
				}
			}
		}
	}
}

void FAudioDevice::AddNewActiveSound( const FActiveSound& NewActiveSound )
{
	check(NewActiveSound.Sound);

	// if we have gone over the MaxConcurrentPlay
	// TODO: Consider if we should handle the bShouldRemainActiveIfDropped case
	if( ( NewActiveSound.Sound->MaxConcurrentPlayCount != 0 ) && ( NewActiveSound.Sound->CurrentPlayCount >= NewActiveSound.Sound->MaxConcurrentPlayCount ) )
	{
		FActiveSound* SoundToStop = NULL;

		switch (NewActiveSound.Sound->MaxConcurrentResolutionRule)
		{
		case EMaxConcurrentResolutionRule::StopOldest:

			for (int32 SoundIndex = 0; SoundIndex < ActiveSounds.Num(); ++SoundIndex)
			{
				FActiveSound* ActiveSound = ActiveSounds[SoundIndex];
				if (ActiveSound->Sound == NewActiveSound.Sound && (SoundToStop == NULL || ActiveSound->PlaybackTime > SoundToStop->PlaybackTime))
				{
					SoundToStop = ActiveSound;
				}
			}
			break;

		case EMaxConcurrentResolutionRule::StopFarthestThenOldest:
		case EMaxConcurrentResolutionRule::StopFarthestThenPreventNew:
		{
			int32 ClosestListenerIndex = NewActiveSound.FindClosestListener(Listeners);
			float DistanceToStopSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), NewActiveSound.Transform.GetTranslation());

			for (int32 SoundIndex = 0; SoundIndex < ActiveSounds.Num(); ++SoundIndex)
			{
				FActiveSound* ActiveSound = ActiveSounds[SoundIndex];
				if (ActiveSound->Sound == NewActiveSound.Sound)
				{
					ClosestListenerIndex = ActiveSound->FindClosestListener(Listeners);
					const float DistanceToActiveSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), ActiveSound->Transform.GetTranslation());

					if (DistanceToActiveSoundSq > DistanceToStopSoundSq)
					{
						SoundToStop = ActiveSound;
						DistanceToStopSoundSq = DistanceToActiveSoundSq;
					}
					else if (   NewActiveSound.Sound->MaxConcurrentResolutionRule == EMaxConcurrentResolutionRule::StopFarthestThenOldest 
							 && DistanceToActiveSoundSq == DistanceToStopSoundSq
							 && (SoundToStop == NULL || ActiveSound->PlaybackTime > SoundToStop->PlaybackTime))
					{
						SoundToStop = ActiveSound;
						DistanceToStopSoundSq = DistanceToActiveSoundSq;
					}
				}
			}
			break;
		}

		case EMaxConcurrentResolutionRule::PreventNew:
		default:
			break;

		}

		// If we found a sound to stop, then stop it and allow the new sound to play.  
		// Otherwise inform the system that the sound failed to start.
		if (SoundToStop)
		{
			SoundToStop->Stop(this);
		}
		else
		{
			// TODO - Audio Threading. This call would be a task back to game thread
			if (NewActiveSound.AudioComponent.IsValid())
			{
				NewActiveSound.AudioComponent->PlaybackCompleted(true);
			}
			//UE_LOG(LogAudio, Verbose, TEXT( "   %g: MaxConcurrentPlayCount AudioComponent : '%s' with Sound: '%s' Max: %d   Curr: %d " ), NewActiveSound.World ? NewActiveSound.World->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT( "NULL" ), Sound->MaxConcurrentPlayCount, Sound->CurrentPlayCount );
			return;

		}
	}

	++NewActiveSound.Sound->CurrentPlayCount;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogAudio, VeryVerbose, TEXT("New ActiveSound %s Comp: %s Loc: %s"), *NewActiveSound.Sound->GetName(), (NewActiveSound.AudioComponent.IsValid() ? *NewActiveSound.AudioComponent->GetFullName() : TEXT("No AudioComponent")), *NewActiveSound.Transform.GetTranslation().ToString() );
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	FActiveSound* ActiveSound = new FActiveSound(NewActiveSound);
	ActiveSounds.Add(ActiveSound);
}

void FAudioDevice::StopActiveSound( UAudioComponent* AudioComponent )
{
	check( AudioComponent );

	FActiveSound* ActiveSound = FindActiveSound(AudioComponent);
	if (ActiveSound)
	{
		ActiveSound->Stop(this);
	}
}

FActiveSound* FAudioDevice::FindActiveSound( UAudioComponent* AudioComponent )
{
	// find the active sound corresponding to this audiocomponent
	for( int32 Index = 0; Index < ActiveSounds.Num(); ++Index )
	{
		FActiveSound* ActiveSound = ActiveSounds[Index];
		if (ActiveSound->AudioComponent.IsValid(true) && ActiveSound->AudioComponent.Get() == AudioComponent)
		{
			return ActiveSound;
		}
	}

	return NULL;
}

void FAudioDevice::RemoveActiveSound(FActiveSound* ActiveSound)
{
	// find the active sound corresponding to this audiocomponent
	for( int32 Index = 0; Index < ActiveSounds.Num(); ++Index )
	{
		if (ActiveSound == ActiveSounds[Index])
		{
			ActiveSounds.RemoveAt(Index);
			delete ActiveSound;
			break;
		}
	}
}

bool FAudioDevice::LocationIsAudible( FVector Location, float MaxDistance )
{
	if( MaxDistance >= WORLD_MAX )
	{
		return( true );
	}

	MaxDistance *= MaxDistance;
	for( int32 i = 0; i < Listeners.Num(); i++ )
	{
		if( ( Listeners[ i ].Transform.GetTranslation() - Location ).SizeSquared() < MaxDistance )
		{
			return( true );
		}
	}

	return( false );
}

UAudioComponent* FAudioDevice::CreateComponent(USoundBase* Sound, UWorld* World, AActor* Actor, bool bPlay, bool bStopWhenOwnerDestroyed, const FVector* Location, USoundAttenuation* AttenuationSettings)
{
	UAudioComponent* AudioComponent = NULL;

	if( Sound && GEngine && GEngine->UseSound() )
	{
		// Get the world's audio device if there is a world, otherwise get the main audio device
		FAudioDevice* AudioDevice = nullptr;
		if (World)
		{
			AudioDevice = World->GetAudioDevice();
		}
		else
		{
			AudioDevice = GEngine->GetMainAudioDevice();
		}
		check(AudioDevice != nullptr);

		// Avoid creating component if we're trying to play a sound on an already destroyed actor.
		if (Actor && Actor->IsPendingKill())
		{
			// Don't create component on destroyed actor.
		}
		// Either no actor or actor is still alive.
		else if (Location && !Sound->IsAudibleSimple(AudioDevice, *Location, AttenuationSettings))
		{
			// Don't create a sound component for short sounds that start out of range of any listener
			UE_LOG(LogAudio, Log, TEXT( "AudioComponent not created for out of range Sound %s" ), *Sound->GetName() );
		}
		else
		{


			// Use actor as outer if we have one.
			if( Actor )
			{
				AudioComponent = NewObject<UAudioComponent>(Actor);
			}
			// Let engine pick the outer (transient package).
			else
			{
				AudioComponent = NewObject<UAudioComponent>();
			}

			check( AudioComponent );

			AudioComponent->Sound = Sound;
			AudioComponent->bAutoActivate = false;
			AudioComponent->bIsUISound = false;
			AudioComponent->bAutoDestroy = bPlay;
			AudioComponent->bStopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
#if WITH_EDITORONLY_DATA
			AudioComponent->bVisualizeComponent	= false;
#endif
			AudioComponent->AttenuationSettings = AttenuationSettings;
			if (Location)
			{
				AudioComponent->SetWorldLocation(*Location);
			}

			// AudioComponent used in PlayEditorSound sets World to NULL to avoid situations where the world becomes invalid
			// and the component is left with invalid pointer.
			if( World )
			{
				AudioComponent->RegisterComponentWithWorld(World);
			}

			if( bPlay )
			{
				AudioComponent->Play();
			}
		}
	}

	return( AudioComponent );
}


void FAudioDevice::Flush( UWorld* WorldToFlush, bool bClearActivatedReverb )
{
	// Stop all audio components attached to the scene
	bool bFoundIgnoredComponent = false;
	for( int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index )
	{
		FActiveSound* ActiveSound = ActiveSounds[Index];
		// if we are in the editor we want to always flush the ActiveSounds
		if( ActiveSound->bIgnoreForFlushing )
		{
			bFoundIgnoredComponent = true;
		}
		else
		{
			if( WorldToFlush == nullptr || ActiveSound->World == nullptr || ActiveSound->World == WorldToFlush )
			{
				ActiveSound->Stop(this);
			}
		}
	}

	// Anytime we flush, make sure to clear all the listeners.  We'll get the right ones soon enough.
	Listeners.Empty();
	Listeners.Add(FListener());

	// Clear all the activated reverb effects
	if (bClearActivatedReverb)
	{
		ActivatedReverbs.Empty();
		HighestPriorityReverb = nullptr;
	}

	if( WorldToFlush == nullptr )
	{
		// Make sure sounds are fully stopped.
		if( bFoundIgnoredComponent )
		{
			// We encountered an ignored component, so address the sounds individually.
			// There's no need to individually clear WaveInstanceSourceMap elements,
			// because FSoundSource::Stop(...) takes care of this.
			for( int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
			{
				const FWaveInstance* WaveInstance = Sources[SourceIndex]->GetWaveInstance();
				if( WaveInstance == NULL || !WaveInstance->ActiveSound->bIgnoreForFlushing )
				{
					Sources[ SourceIndex ]->Stop();
				}
			}
		}
		else
		{
			// No components were ignored, so stop all sounds.
			for( int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
			{
				Sources[ SourceIndex ]->Stop();
			}

			WaveInstanceSourceMap.Empty();
		}
	}
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundWave	Resource to be precached.
 */

void FAudioDevice::Precache(USoundWave* SoundWave, bool bSynchronous, bool bTrackMemory)
{
	if( SoundWave == NULL )
	{
		return;
	}

	// calculate the decompression type
	// @todo audio: maybe move this into SoundWave?
	if( SoundWave->NumChannels == 0 )
	{
		// No channels - no way of knowing what to play back
		SoundWave->DecompressionType = DTYPE_Invalid;
	}
	else if( SoundWave->RawPCMData )
	{
		// Run time created audio; e.g. editor preview data
		SoundWave->DecompressionType = DTYPE_Preview;
	}
	else if ( SoundWave->bProcedural )
	{
		// Streaming data, created programmatically.
		SoundWave->DecompressionType = DTYPE_Procedural;
	}
	else if ( HasCompressedAudioInfoClass(SoundWave) )
	{
		const FSoundGroup& SoundGroup = GetDefault<USoundGroups>()->GetSoundGroup(SoundWave->SoundGroup);

		// handle audio decompression
		if (FPlatformProperties::SupportsAudioStreaming() && SoundWave->IsStreaming())
		{
			SoundWave->DecompressionType = DTYPE_Streaming;
		}
		else if (SupportsRealtimeDecompression() && 
			(bDisableAudioCaching || (!SoundGroup.bAlwaysDecompressOnLoad && SoundWave->Duration > SoundGroup.DecompressedDuration)))
		{
			// Store as compressed data and decompress in realtime
			SoundWave->DecompressionType = DTYPE_RealTime;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			++PrecachedRealtime;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
		else
		{
			// Fully expand loaded audio data into PCM
			SoundWave->DecompressionType = DTYPE_Native;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			++PrecachedNative;
			AverageNativeLength = (AverageNativeLength * (PrecachedNative - 1) + SoundWave->Duration) / PrecachedNative;
			NativeSampleRateCount.FindOrAdd(SoundWave->SampleRate)++;
			NativeChannelCount.FindOrAdd(SoundWave->NumChannels)++;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}

		// Grab the compressed audio data
		SoundWave->InitAudioResource(GetRuntimeFormat(SoundWave));

		if (SoundWave->AudioDecompressor == NULL && SoundWave->DecompressionType == DTYPE_Native)
		{
			check(!SoundWave->AudioDecompressor); // should not have had a valid pointer at this point
			// Create a worker to decompress the audio data
			if (bSynchronous)
			{
				// Create a worker to decompress the vorbis data
				FAsyncAudioDecompress TempDecompress(SoundWave);
				TempDecompress.StartSynchronousTask();
			}
			else
			{
				SoundWave->AudioDecompressor = new FAsyncAudioDecompress(SoundWave);
				SoundWave->AudioDecompressor->StartBackgroundTask();
			}

			static FName NAME_OGG(TEXT("OGG"));
			SoundWave->bDecompressedFromOgg = GetRuntimeFormat(SoundWave) == NAME_OGG;

			// the audio decompressor will track memory
			bTrackMemory = false;
		}
	}
	else
	{
		// Preserve old behavior if there is no compressed audio info class for this audio format
		SoundWave->DecompressionType = DTYPE_Native;
	}

	if (bTrackMemory)
	{
		const int32 ResourceSize = SoundWave->GetResourceSize(EResourceSizeMode::Exclusive);
		SoundWave->TrackedMemoryUsage += ResourceSize;

		// If we aren't decompressing it above, then count the memory
		INC_DWORD_STAT_BY( STAT_AudioMemorySize, ResourceSize );
		INC_DWORD_STAT_BY( STAT_AudioMemory, ResourceSize );
	}
}

void FAudioDevice::StopSourcesUsingBuffer(FSoundBuffer * SoundBuffer)
{
	if (SoundBuffer)
	{
		for (int32 SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++)
		{
			FSoundSource* Src = Sources[SrcIndex];
			if (Src && Src->Buffer == SoundBuffer)
			{
				// Make sure the buffer is no longer referenced by anything
				Src->Stop();
				break;
			}
		}
	}
}

void FAudioDevice::RegisterSoundClass(USoundClass* InSoundClass)
{
	if (InSoundClass)
	{
		// If the sound class wasn't already registered get it in to the system.
		if (!SoundClasses.Contains(InSoundClass))
		{
			SoundClasses.Add(InSoundClass, FSoundClassProperties());
		}
	}
}

void FAudioDevice::UnregisterSoundClass(USoundClass* SoundClass)
{
	if (SoundClass)
	{
		SoundClasses.Remove(SoundClass);
	}
}

FSoundClassProperties* FAudioDevice::GetSoundClassCurrentProperties(class USoundClass* InSoundClass)
{
	if (InSoundClass)
	{
		FSoundClassProperties* Properties = SoundClasses.Find(InSoundClass);
		return Properties;
	}
	return nullptr;
}


#if !UE_BUILD_SHIPPING
// sort memory usage from large to small 
struct FSortSoundBuffersBySize
{
	FORCEINLINE bool operator()(FSoundBuffer& A, FSoundBuffer& B) const
	{
		return B.GetSize() < A.GetSize();
	}
};

// sort memory usage by name
struct FSortSoundBuffersByName
{
	FORCEINLINE bool operator()(FSoundBuffer& A, FSoundBuffer& B) const
	{
		return A.ResourceName < B.ResourceName;
	}
};

/** 
 * Displays debug information about the loaded sounds
 */
bool FAudioDevice::HandleListSoundsCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// does the user want to sort by name instead of size?
	bool bAlphaSort = FParse::Param(Cmd, TEXT("ALPHASORT"));
	bool bUseLongNames = FParse::Param(Cmd, TEXT("LONGNAMES"));

	int32 TotalResident = 0;
	int32 ResidentCount = 0;

	Ar.Logf(TEXT("Listing all sounds:"));

	// Get audio device manager since thats where sound buffers are stored
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	TArray<FSoundBuffer*> AllSounds;
	for (int32 BufferIndex = 0; BufferIndex < AudioDeviceManager->Buffers.Num(); BufferIndex++)
	{
		AllSounds.Add(AudioDeviceManager->Buffers[BufferIndex]);
	}

	// sort by name or size, depending on flag
	if (bAlphaSort)
	{
		AllSounds.Sort(FSortSoundBuffersByName());
	}
	else
	{
		AllSounds.Sort(FSortSoundBuffersBySize());
	}

	// now list the sorted sounds
	for (int32 BufferIndex = 0; BufferIndex < AllSounds.Num(); BufferIndex++)
	{
		FSoundBuffer* Buffer = AllSounds[BufferIndex];

		// format info string
		Ar.Logf(TEXT("%s"), *Buffer->Describe(bUseLongNames));

		// track memory and count
		TotalResident += Buffer->GetSize();
		ResidentCount++;
	}

	Ar.Logf(TEXT("%8.2f Kb for %d resident sounds"), TotalResident / 1024.0f, ResidentCount);
	return true;
}
#endif // !UE_BUILD_SHIPPING

void FAudioDevice::StopSoundsUsingResource(USoundWave* SoundWave, TArray<UAudioComponent*>& StoppedComponents)
{
	bool bStoppedSounds = false;

	for (int32 ActiveSoundIndex = ActiveSounds.Num() - 1; ActiveSoundIndex >= 0; --ActiveSoundIndex)
	{
		FActiveSound* ActiveSound = ActiveSounds[ActiveSoundIndex];
		for (auto WaveInstanceIt(ActiveSound->WaveInstances.CreateConstIterator()); WaveInstanceIt; ++WaveInstanceIt)
		{
			// If anything the ActiveSound uses the wave then we stop the sound
			FWaveInstance* WaveInstance = WaveInstanceIt.Value();
			if (WaveInstance->WaveData == SoundWave)
			{
				if (ActiveSound->AudioComponent.IsValid())
				{
					StoppedComponents.Add(ActiveSound->AudioComponent.Get());
				}
				ActiveSound->Stop(this);
				bStoppedSounds = true;
				break;
			}
		}
	}

	if (!GIsEditor && bStoppedSounds)
	{
		UE_LOG(LogAudio, Warning, TEXT( "All Sounds using SoundWave '%s' have been stopped" ), *SoundWave->GetName() );
	}
}

#if WITH_EDITOR
void FAudioDevice::OnBeginPIE(const bool bIsSimulating)
{
	for (TObjectIterator<USoundNode> It; It; ++It)
	{
		USoundNode* SoundNode = *It;
		SoundNode->OnBeginPIE(bIsSimulating);
	}
}

void FAudioDevice::OnEndPIE(const bool bIsSimulating)
{
	for (TObjectIterator<USoundNode> It; It; ++It)
	{
		USoundNode* SoundNode = *It;
		SoundNode->OnEndPIE(bIsSimulating);
	}
}
#endif
