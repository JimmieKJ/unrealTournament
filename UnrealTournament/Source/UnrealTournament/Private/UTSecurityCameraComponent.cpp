// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTSecurityCameraComponent.h"
#include "UTCharacter.h"
#include "UTCarriedObject.h"
#include "UTRepulsorBubble.h"

UUTSecurityCameraComponent::UUTSecurityCameraComponent()
{
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.SetTickFunctionEnable(true);
	DetectionRadius = 5000.f;
	bCameraEnabled = true;
	CameraPingedDuration = 1.f;
}

void UUTSecurityCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	//UTOwner = Cast<AUTCharacter>(GetOwner());
}

void UUTSecurityCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner() == nullptr)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return;
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (bCameraEnabled && GS && GS->IsMatchInProgress() && !GS->IsMatchIntermission())
	{
		static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
		FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, GetOwner());

		//Add all Repulsor bubbles to ignore actors.
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTRepulsorBubble* Repulsor = Cast<AUTRepulsorBubble>(*It);
			if (Repulsor)
			{
				CollisionParams.AddIgnoredActor(Repulsor);
			}
		}

		FVector CameraLoc = K2_GetComponentLocation();
		if (DetectedFlag && (DetectedFlag->GetDetectingCamera() == this))
		{
			// verify if still visible
			if (((DetectedFlag->GetActorLocation() - CameraLoc).SizeSquared() > DetectionRadius * DetectionRadius) || GetWorld()->LineTraceTestByChannel(CameraLoc, DetectedFlag->GetActorLocation() + FVector(0.f, 0.f,60.f), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParams))
			{
				OnFlagCarrierDetectionLost(DetectedFlagCarrier);
				if (DetectedFlag)
				{
					DetectedFlag->SetDetectingCamera(nullptr);
				}
				DetectedFlag = nullptr;
				DetectedFlagCarrier = nullptr;
			}
		}
		else
		{
			// look for a flag carrier
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>((*It).Get());
				if (UTChar && UTChar->GetCarriedObject() && ((UTChar->GetCarriedObject()->GetActorLocation() - CameraLoc).SizeSquared() < DetectionRadius * DetectionRadius))
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
		if (DetectedFlag && (DetectedFlag->Role == ROLE_Authority))
		{
			DetectedFlag->LastPingedTime = FMath::Max(DetectedFlag->LastPingedTime, GetWorld()->GetTimeSeconds() - DetectedFlag->PingedDuration + FMath::Max(0.2f, CameraPingedDuration));
		}
	}
	else
	{
		if (DetectedFlag)
		{
			OnFlagCarrierDetectionLost(DetectedFlagCarrier);
			DetectedFlag->SetDetectingCamera(nullptr);
		}
		DetectedFlag = nullptr;
		DetectedFlagCarrier = nullptr;
	}
}