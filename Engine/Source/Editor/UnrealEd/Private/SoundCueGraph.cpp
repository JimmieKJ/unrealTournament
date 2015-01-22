// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/////////////////////////////////////////////////////
// USoundCueGraph

#include "UnrealEd.h"
#include "SoundDefinitions.h"
#include "Sound/SoundCue.h"

USoundCueGraph::USoundCueGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USoundCue* USoundCueGraph::GetSoundCue() const
{
	return CastChecked<USoundCue>(GetOuter());
}
