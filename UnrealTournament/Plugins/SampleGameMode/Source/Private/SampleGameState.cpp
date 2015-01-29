// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"
#include "SampleGameState.h"

ASampleGameState::ASampleGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void ASampleGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASampleGameState, bPlayerReachedLastLevel);
}