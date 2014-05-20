// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHud.h"
#include "UTPlayerState.h"
#include "UTPlayerController.h"

AUTPlayerController::AUTPlayerController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void AUTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AUTPlayerController::OnFire);
	InputComponent->BindAction("StopFire", IE_Released, this, &AUTPlayerController::OnStopFire);
	InputComponent->BindAction("StartAltFire", IE_Pressed, this, &AUTPlayerController::OnAltFire);
	InputComponent->BindAction("StopAltFire", IE_Released, this, &AUTPlayerController::OnStopAltFire);
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AUTPlayerController::TouchStarted);

}

/* Cache a copy of the PlayerState cast'd to AUTPlayerState for easy reference.  Do it both here and when the replicated copy of APlayerState arrives in OnRep_PlayerState */
void AUTPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	UTCharacter = Cast<AUTCharacter>(InPawn);
}

void AUTPlayerController::OnFire()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->StartFire(0);
	}
	else
	{
		ServerRestartPlayer();
	}

}
void AUTPlayerController::OnStopFire()
{
	if (UTCharacter)
	{
		UTCharacter->StopFire(0);
	}
}
void AUTPlayerController::OnAltFire()
{
	if (UTCharacter)
	{
		UTCharacter->StartFire(1);
	}
}
void AUTPlayerController::OnStopAltFire()
{
	if (UTCharacter)
	{
		UTCharacter->StopFire(1);
	}
}

void AUTPlayerController::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// only fire for first finger down
	if (FingerIndex == 0)
	{
		OnFire();
	}
}

