// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGhostFlag.h"

AUTGhostFlag::AUTGhostFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetReplicates(true);
	NetPriority = 3.0;
	bReplicateMovement = true;
}