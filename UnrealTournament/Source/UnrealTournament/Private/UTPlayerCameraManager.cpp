// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPlayerCameraManager.h"


AUTPlayerCameraManager::AUTPlayerCameraManager(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	FreeCamOffset = FVector(-256,0,90);
	bUseClientSideCameraUpdates = false;
}

void AUTPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	FName SavedCameraStyle = CameraStyle;
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState != NULL)
	{
		CameraStyle = GameState->OverrideCameraStyle(PCOwner, CameraStyle);
	}
	Super::UpdateViewTarget(OutVT, DeltaTime);
	CameraStyle = SavedCameraStyle;
}

