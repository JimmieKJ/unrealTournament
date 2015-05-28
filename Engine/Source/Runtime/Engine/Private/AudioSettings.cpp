// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlayerInput.cpp: Unreal input system.
=============================================================================*/

#include "EnginePrivate.h"
#include "Sound/AudioSettings.h"


UAudioSettings::UAudioSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SectionName = TEXT("Audio");
}
