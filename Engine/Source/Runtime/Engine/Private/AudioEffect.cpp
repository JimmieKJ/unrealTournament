// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioEffect.cpp: Unreal base audio.
=============================================================================*/

#include "EnginePrivate.h" 
#include "AudioEffect.h"
#include "Sound/ReverbEffect.h"

/** 
 * Default settings for a null reverb effect
 */
FAudioReverbEffect::FAudioReverbEffect( void )
{
	Time = 0.0;
	Volume = 0.0f;

	Density = 1.0f;					
	Diffusion = 1.0f;				
	Gain = 0.32f;					
	GainHF = 0.89f;					
	DecayTime = 1.49f;				
	DecayHFRatio = 0.83f;			
	ReflectionsGain = 0.05f;		
	ReflectionsDelay = 0.007f;		
	LateGain = 1.26f;				
	LateDelay = 0.011f;				
	AirAbsorptionGainHF = 0.994f;	
	RoomRolloffFactor = 0.0f;		
}

/** 
 * Construct generic reverb settings based in the I3DL2 standards
 */
FAudioReverbEffect::FAudioReverbEffect( float InRoom, 
								 		float InRoomHF, 
								 		float InRoomRolloffFactor,	
								 		float InDecayTime,			
								 		float InDecayHFRatio,		
								 		float InReflections,		
								 		float InReflectionsDelay,	
								 		float InReverb,				
								 		float InReverbDelay,		
								 		float InDiffusion,			
								 		float InDensity,			
								 		float InAirAbsorption )
{
	Time = 0.0;
	Volume = 0.0f;

	Density = InDensity;
	Diffusion = InDiffusion;
	Gain = InRoom;
	GainHF = InRoomHF;
	DecayTime = InDecayTime;
	DecayHFRatio = InDecayHFRatio;
	ReflectionsGain = InReflections;
	ReflectionsDelay = InReflectionsDelay;
	LateGain = InReverb;
	LateDelay = InReverbDelay;
	RoomRolloffFactor = InRoomRolloffFactor;
	AirAbsorptionGainHF = InAirAbsorption;
}

FAudioReverbEffect& FAudioReverbEffect::operator=(class UReverbEffect* InReverbEffect)
{
	if( InReverbEffect )
	{
		Density = InReverbEffect->Density;
		Diffusion = InReverbEffect->Diffusion;
		Gain = InReverbEffect->Gain;
		GainHF = InReverbEffect->GainHF;
		DecayTime = InReverbEffect->DecayTime;
		DecayHFRatio = InReverbEffect->DecayHFRatio;
		ReflectionsGain = InReverbEffect->ReflectionsGain;
		ReflectionsDelay = InReverbEffect->ReflectionsDelay;
		LateGain = InReverbEffect->LateGain;
		LateDelay = InReverbEffect->LateDelay;
		AirAbsorptionGainHF = InReverbEffect->AirAbsorptionGainHF;
		RoomRolloffFactor = InReverbEffect->RoomRolloffFactor;
	}

	return *this;
}

/** 
 * Get interpolated reverb parameters
 */
void FAudioReverbEffect::Interpolate( float InterpValue, const FAudioReverbEffect& Start, const FAudioReverbEffect& End )
{
	float InvInterpValue = 1.0f - InterpValue;
	Time = FApp::GetCurrentTime();
	Volume = ( Start.Volume * InvInterpValue ) + ( End.Volume * InterpValue );
	Density = ( Start.Density * InvInterpValue ) + ( End.Density * InterpValue );				
	Diffusion = ( Start.Diffusion * InvInterpValue ) + ( End.Diffusion * InterpValue );				
	Gain = ( Start.Gain * InvInterpValue ) + ( End.Gain * InterpValue );					
	GainHF = ( Start.GainHF * InvInterpValue ) + ( End.GainHF * InterpValue );					
	DecayTime = ( Start.DecayTime * InvInterpValue ) + ( End.DecayTime * InterpValue );				
	DecayHFRatio = ( Start.DecayHFRatio * InvInterpValue ) + ( End.DecayHFRatio * InterpValue );			
	ReflectionsGain = ( Start.ReflectionsGain * InvInterpValue ) + ( End.ReflectionsGain * InterpValue );		
	ReflectionsDelay = ( Start.ReflectionsDelay * InvInterpValue ) + ( End.ReflectionsDelay * InterpValue );		
	LateGain = ( Start.LateGain * InvInterpValue ) + ( End.LateGain * InterpValue );				
	LateDelay = ( Start.LateDelay * InvInterpValue ) + ( End.LateDelay * InterpValue );				
	AirAbsorptionGainHF = ( Start.AirAbsorptionGainHF * InvInterpValue ) + ( End.AirAbsorptionGainHF * InterpValue );	
	RoomRolloffFactor = ( Start.RoomRolloffFactor * InvInterpValue ) + ( End.RoomRolloffFactor * InterpValue );		
}

/** 
 * Validate all settings are in range
 */
void FAudioEQEffect::ClampValues( void )
{
	HFFrequency = FMath::Clamp<float>( HFFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	HFGain = FMath::Clamp<float>( HFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );
	MFCutoffFrequency = FMath::Clamp<float>( MFCutoffFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	MFBandwidth = FMath::Clamp<float>( MFBandwidth, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH );
	MFGain = FMath::Clamp<float>( MFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );
	LFFrequency = FMath::Clamp<float>( LFFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	LFGain = FMath::Clamp<float>( LFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );
}

/** 
 * Interpolate EQ settings based on time
 */
void FAudioEQEffect::Interpolate( float InterpValue, const FAudioEQEffect& Start, const FAudioEQEffect& End )
{
	float InvInterpValue = 1.0f - InterpValue;
	RootTime = FApp::GetCurrentTime();
	HFFrequency = ( Start.HFFrequency * InvInterpValue ) + ( End.HFFrequency * InterpValue );
	HFGain = ( Start.HFGain * InvInterpValue ) + ( End.HFGain * InterpValue );
	MFCutoffFrequency = ( Start.MFCutoffFrequency * InvInterpValue ) + ( End.MFCutoffFrequency * InterpValue );
	MFBandwidth = ( Start.MFBandwidth * InvInterpValue ) + ( End.MFBandwidth * InterpValue );
	MFGain = ( Start.MFGain * InvInterpValue ) + ( End.MFGain * InterpValue );
	LFFrequency = ( Start.LFFrequency * InvInterpValue ) + ( End.LFFrequency * InterpValue );
	LFGain = ( Start.LFGain * InvInterpValue ) + ( End.LFGain * InterpValue );
}

/**
 * Converts and volume (0.0f to 1.0f) to a deciBel value
 */
int64 FAudioEffectsManager::VolumeToDeciBels( float Volume )
{
	int64 DeciBels = -100;

	if( Volume > 0.0f )
	{
		DeciBels = FMath::Clamp<int64>( ( int64 )( 20.0f * log10f( Volume ) ), -100, 0 ) ;
	}

	return( DeciBels );
}


/**
 * Converts and volume (0.0f to 1.0f) to a MilliBel value (a Hundredth of a deciBel)
 */
int64 FAudioEffectsManager::VolumeToMilliBels( float Volume, int32 MaxMilliBels )
{
	int64 MilliBels = -10000;

	if( Volume > 0.0f )
	{
		MilliBels = FMath::Clamp<int64>( ( int64)( 2000.0f * log10f( Volume ) ), -10000, MaxMilliBels );
	}

	return( MilliBels );
}

/** 
 * Gets the parameters for reverb based on settings and time
 */
void FAudioEffectsManager::Interpolate( FAudioReverbEffect& Current, const FAudioReverbEffect& Start, const FAudioReverbEffect& End )
{
	float InterpValue = 1.0f;
	if( End.Time - Start.Time > 0.0 )
	{
		InterpValue = ( float )( ( FApp::GetCurrentTime() - Start.Time ) / ( End.Time - Start.Time ) );
	}

	if( InterpValue >= 1.0f )
	{
		Current = End;
		return;
	}

	if( InterpValue <= 0.0f )
	{
		Current = Start;
		return;
	}

	Current.Interpolate( InterpValue, Start, End );
}

/** 
 * Gets the parameters for EQ based on settings and time
 */
void FAudioEffectsManager::Interpolate( FAudioEQEffect& Current, const FAudioEQEffect& Start, const FAudioEQEffect& End )
{
	float InterpValue = 1.0f;
	if( End.RootTime - Start.RootTime > 0.0 )
	{
		InterpValue = ( float )( ( FApp::GetCurrentTime() - Start.RootTime ) / ( End.RootTime - Start.RootTime ) );
	}

	if( InterpValue >= 1.0f )
	{
		Current = End;
		return;
	}

	if( InterpValue <= 0.0f )
	{
		Current = Start;
		return;
	}

	Current.Interpolate( InterpValue, Start, End );
}

/** 
 * Clear out any reverb and EQ settings
 */
FAudioEffectsManager::FAudioEffectsManager( FAudioDevice* InDevice )
:	AudioDevice( InDevice )
,	bEffectsInitialised( false )
,	CurrentReverbAsset( NULL )
,	CurrentEQMix( NULL )
{
	InitAudioEffects();
}

void FAudioEffectsManager::ResetInterpolation()
{
	InitAudioEffects();
}

/** 
 * Sets up default reverb and eq settings
 */
void FAudioEffectsManager::InitAudioEffects( void )
{
	// Clear out the default reverb settings
	FReverbSettings ReverbSettings;
	ReverbSettings.ReverbEffect = NULL;
	ReverbSettings.Volume = 0.0f;
	ReverbSettings.FadeTime = 0.1f;
	SetReverbSettings( ReverbSettings );

	ClearMixSettings();
}

void FAudioEffectsManager::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(CurrentReverbAsset);
}

/**
 * Called every tick from UGameViewportClient::Draw
 * 
 * Sets new reverb mode if necessary. Otherwise interpolates to the current settings and calls SetEffect to handle
 * the platform specific aspect.
 */
void FAudioEffectsManager::SetReverbSettings( const FReverbSettings& ReverbSettings )
{
	/** Update the settings if the reverb type has changed */
	if( ReverbSettings.bApplyReverb && ReverbSettings.ReverbEffect != CurrentReverbAsset )
	{
		FString CurrentReverbName = CurrentReverbAsset ? CurrentReverbAsset->GetName() : TEXT("None");
		FString NextReverbName = ReverbSettings.ReverbEffect ? ReverbSettings.ReverbEffect->GetName() : TEXT("None");
		UE_LOG(LogAudio, Log, TEXT( "FAudioDevice::SetReverbSettings(): Old - %s  New - %s:%f (%f)" ),
			*CurrentReverbName, *NextReverbName, ReverbSettings.Volume, ReverbSettings.FadeTime );

		if( ReverbSettings.Volume > 1.0f )
		{
			UE_LOG(LogAudio, Warning, TEXT( "FAudioDevice::SetReverbSettings(): Illegal volume %g (should be 0.0f <= Volume <= 1.0f)" ), ReverbSettings.Volume );
		}

		SourceReverbEffect = CurrentReverbEffect;
		SourceReverbEffect.Time = FApp::GetCurrentTime();

		DestinationReverbEffect = ReverbSettings.ReverbEffect;
		DestinationReverbEffect.Time = FApp::GetCurrentTime() + ReverbSettings.FadeTime;
		DestinationReverbEffect.Volume = ReverbSettings.Volume;
		if( !ReverbSettings.ReverbEffect )
		{
			DestinationReverbEffect.Volume = 0.0f;
		}

		CurrentReverbAsset = ReverbSettings.ReverbEffect;
	}
}

/**
 * Sets new EQ mix if necessary. Otherwise interpolates to the current settings and calls SetEffect to handle
 * the platform specific aspect.
 */
void FAudioEffectsManager::SetMixSettings( USoundMix* NewMix, bool bIgnorePriority )
{
	if( NewMix && NewMix != CurrentEQMix )
	{
		// Check whether the priority of this SoundMix is higher than existing one
		if (CurrentEQMix == NULL || bIgnorePriority || NewMix->EQPriority > CurrentEQMix->EQPriority)
		{
			UE_LOG(LogAudio, Log, TEXT( "FAudioEffectsManager::SetMixSettings(): %s" ), *NewMix->GetName() );

			SourceEQEffect = CurrentEQEffect;
			SourceEQEffect.RootTime = FApp::GetCurrentTime();

			if( NewMix->bApplyEQ )
			{
				DestinationEQEffect = NewMix->EQSettings;
			}
			else
			{
				// it doesn't have EQ settings, so interpolate back to default
				DestinationEQEffect = FAudioEQEffect();
			}

			DestinationEQEffect.RootTime = FApp::GetCurrentTime() + NewMix->FadeInTime;
			DestinationEQEffect.ClampValues();

			CurrentEQMix = NewMix;
		}
	}
}

/**
 * If there is an active SoundMix, clear it and any EQ settings it applied
 */
void FAudioEffectsManager::ClearMixSettings()
{
	if (CurrentEQMix)
	{
		UE_LOG(LogAudio, Log, TEXT( "FAudioEffectsManager::ClearMixSettings(): %s" ), *CurrentEQMix->GetName() );

		SourceEQEffect = CurrentEQEffect;
		SourceEQEffect.RootTime = FApp::GetCurrentTime();

		// interpolate back to default
		DestinationEQEffect = FAudioEQEffect();
		DestinationEQEffect.RootTime = FApp::GetCurrentTime() + CurrentEQMix->FadeOutTime;

		CurrentEQMix = NULL;
	}
}

/** 
 * Feed in new settings to the audio effect system
 */
void FAudioEffectsManager::Update( void )
{
	/** Interpolate the settings depending on time */
	Interpolate( CurrentReverbEffect, SourceReverbEffect, DestinationReverbEffect );

	/** Call the platform-specific code to set the effect */
	SetReverbEffectParameters( CurrentReverbEffect );

	/** Interpolate the settings depending on time */
	Interpolate( CurrentEQEffect, SourceEQEffect, DestinationEQEffect );

	/** Call the platform-specific code to set the effect */
	SetEQEffectParameters( CurrentEQEffect );
}

// end 
