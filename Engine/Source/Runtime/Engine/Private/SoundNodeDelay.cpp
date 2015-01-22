// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeDelay.h"

/*-----------------------------------------------------------------------------
         USoundNodeDelay implementation.
-----------------------------------------------------------------------------*/
USoundNodeDelay::USoundNodeDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DelayMin = 0.0f;
	DelayMax = 0.0f;

}

void USoundNodeDelay::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( float ) );
	DECLARE_SOUNDNODE_ELEMENT( float, EndOfDelay );

	// Check to see if this is the first time through.
	if( *RequiresInitialization )
	{
		*RequiresInitialization = false;

		const float ActualDelay = FMath::Max(0.f, DelayMax + ( ( DelayMin - DelayMax ) * FMath::SRand() ));

		if (ParseParams.StartTime > ActualDelay)
		{
			FSoundParseParameters UpdatedParams = ParseParams;
			UpdatedParams.StartTime -= ActualDelay;
			EndOfDelay = -1.f;
			Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances );
			return;
		}
		else
		{
			EndOfDelay = ActiveSound.PlaybackTime + ActualDelay - ParseParams.StartTime;
		}
	}

	// If we have not waited long enough then just keep waiting.
	if( EndOfDelay > ActiveSound.PlaybackTime )
	{
		// We're not finished even though we might not have any wave instances in flight.
		ActiveSound.bFinished = false;
	}
	// Go ahead and play the sound.
	else
	{
		Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances );
	}
}

float USoundNodeDelay::GetDuration()
{
	// Get length of child node.
	float ChildDuration = 0.0f;
	if( ChildNodes[ 0 ] )
	{
		ChildDuration = ChildNodes[ 0 ]->GetDuration();
	}

	// And return the two together.
	return( ChildDuration + DelayMax );
}