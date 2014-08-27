// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"
#include "SampleGameState.h"

ASampleGameState::ASampleGameState(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void ASampleGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASampleGameState, bPlayerReachedLastLevel);
}