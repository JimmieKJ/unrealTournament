// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTHUD.h"

AUTGameMode::AUTGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UBlueprint> PlayerPawnObject(TEXT("/Game/Blueprints/BaseUTCharacter"));
	if (PlayerPawnObject.Object != NULL)
	{
		DefaultPawnClass = (UClass*)PlayerPawnObject.Object->GeneratedClass;
	}

	// use our custom HUD class
	HUDClass = AUTHUD::StaticClass();
}
