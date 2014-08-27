// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"
#include "SamplePlayerState.h"

ASamplePlayerState::ASamplePlayerState(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void ASamplePlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASamplePlayerState, PlayerLevel);
}