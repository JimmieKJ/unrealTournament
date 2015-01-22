// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SpectatorPawnMovement.cpp: SpectatorPawn movement implementation

=============================================================================*/

#include "EnginePrivate.h"
#include "GameFramework/SpectatorPawnMovement.h"

USpectatorPawnMovement::USpectatorPawnMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxSpeed = 1200.f;
	Acceleration = 4000.f;
	Deceleration = 12000.f;

	ResetMoveState();

	bIgnoreTimeDilation = false;
}


void USpectatorPawnMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (!PawnOwner || !UpdatedComponent)
	{
		return;
	}

	// We might want to ignore time dilation
	float AdjustedDeltaTime = DeltaTime;
	if (bIgnoreTimeDilation)
	{
		const float WorldTimeDilation = PawnOwner->GetWorldSettings()->GetEffectiveTimeDilation();
		if (WorldTimeDilation > KINDA_SMALL_NUMBER)
		{
			AdjustedDeltaTime = DeltaTime / WorldTimeDilation;
		}
	}

	Super::TickComponent(AdjustedDeltaTime, TickType, ThisTickFunction);
};

