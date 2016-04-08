// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTSecurityCameraComponent.h"
#include "UTCharacter.h"
#include "UTCarriedObject.h"

UUTSecurityCameraComponent::UUTSecurityCameraComponent()
{
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.SetTickFunctionEnable(true);
	DetectionRadius = 5000.f;
	bCameraEnabled = true;
}

void UUTSecurityCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	//UTOwner = Cast<AUTCharacter>(GetOwner());
}

void UUTSecurityCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner())
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return;
	}

	if (bCameraEnabled)
	{
		static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
		FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, GetOwner());
		FVector CameraLoc = K2_GetComponentLocation();
		if (DetectedFlag)
		{
			// verify if still visible
			if (GetWorld()->LineTraceTestByChannel(CameraLoc, DetectedFlag->GetActorLocation() + FVector(0.f, 0.f,60.f), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParams))
			{
				OnFlagCarrierDetectionLost(DetectedFlagCarrier);
				DetectedFlag = nullptr;
			}
		}
		else
		{
			// look for a flag carrier
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>((*It).Get());
				if (UTChar && UTChar->GetCarriedObject() && ((UTChar->GetActorLocation() - CameraLoc).SizeSquared() < DetectionRadius * DetectionRadius))
				{
					if (!GetWorld()->LineTraceTestByChannel(CameraLoc, UTChar->GetActorLocation() + FVector(0.f, 0.f, 60.f), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParams)
						&& UTChar->GetCarriedObject()->SetDetectingCamera(this))
					{
						DetectedFlag = UTChar->GetCarriedObject();
						DetectedFlagCarrier = UTChar;
						OnFlagCarrierDetected(DetectedFlagCarrier);
						break;
					}
				}
			}
		}
		DetectedFlagCarrier = DetectedFlag ? DetectedFlag->HoldingPawn : nullptr;
	}
	else
	{
		DetectedFlag = nullptr;
		DetectedFlagCarrier = nullptr;
	}
}