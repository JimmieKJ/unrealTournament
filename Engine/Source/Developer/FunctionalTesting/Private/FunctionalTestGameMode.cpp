// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "FunctionalTestGameMode.h"

AFunctionalTestGameMode::AFunctionalTestGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpectatorClass = ASpectatorPawn::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
