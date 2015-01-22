// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

//////////////////////////////////////////////////////////////////////////
// UCameraShake

UCameraShake::UCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AnimPlayRate = 1.0f;
	AnimScale = 1.0f;
	AnimBlendInTime = 0.2f;
	AnimBlendOutTime = 0.2f;
	OscillationBlendInTime = 0.1f;
	OscillationBlendOutTime = 0.2f;
}
