// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"
#include "SamplePlayerState.h"

ASamplePlayerState::ASamplePlayerState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void ASamplePlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASamplePlayerState, PlayerLevel);
}