// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CinematicCameraPrivate.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"

#define LOCTEXT_NAMESPACE "CineCameraActor"

//////////////////////////////////////////////////////////////////////////
// ACameraActor

ACineCameraActor::ACineCameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
			.SetDefaultSubobjectClass<UCineCameraComponent>(TEXT("CameraComponent"))
	)
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(LookatTrackingSettings.bEnableLookAtTracking);
}

#if WITH_EDITOR
void ACineCameraActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetActorTickEnabled(LookatTrackingSettings.bEnableLookAtTracking);
}
#endif

void ACineCameraActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetActorTickEnabled(LookatTrackingSettings.bEnableLookAtTracking);

	LookatTrackingSettings.LastLookatTrackingRotation = GetActorRotation();
}

bool ACineCameraActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

FVector ACineCameraActor::GetLookatLocation() const
{
	FVector FinalLookat;
	if (LookatTrackingSettings.ActorToTrack)
	{
		FTransform const BaseTransform = LookatTrackingSettings.ActorToTrack ? LookatTrackingSettings.ActorToTrack->GetActorTransform() : FTransform::Identity;
		FinalLookat = BaseTransform.TransformPosition(LookatTrackingSettings.RelativeOffset);
	}
	else
	{
		FinalLookat = LookatTrackingSettings.RelativeOffset;
	}

	return FinalLookat;
}

static const FColor DebugLookatTrackingPointSolidColor(200, 200, 32, 128);		// yellow
static const FColor DebugLookatTrackingPointOutlineColor = FColor::Black;

void ACineCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CameraComponent && LookatTrackingSettings.bEnableLookAtTracking)
	{
		// do the lookat tracking
		// #note this will turn the whole actor, which assumes the cameracomponent's transform is the same as the root component
		// more complex component hierarchies will require different handling here
		FVector const LookatLoc = GetLookatLocation();
		FVector const ToLookat = LookatLoc - GetActorLocation();
		FRotator const FinalRot = 
			bResetInterplation
			? ToLookat.Rotation()
			: FMath::RInterpTo(LookatTrackingSettings.LastLookatTrackingRotation, ToLookat.Rotation(), DeltaTime, LookatTrackingSettings.LookAtTrackingInterpSpeed);
		SetActorRotation(FinalRot);

		// we store this ourselves in case other systems try to change our rotation, and end fighting the interpolation
		LookatTrackingSettings.LastLookatTrackingRotation = FinalRot;

		if (LookatTrackingSettings.bDrawDebugLookAtTrackingPosition)
		{
			::DrawDebugSolidBox(GetWorld(), LookatLoc, FVector(12.f), DebugLookatTrackingPointSolidColor);
			::DrawDebugBox(GetWorld(), LookatLoc, FVector(12.f), DebugLookatTrackingPointOutlineColor);
		}
	}
	else
	{
		// no tracking, no ticking
		SetActorTickEnabled(false);
	}

	bResetInterplation = false;
}


void ACineCameraActor::NotifyCameraCut()
{
	Super::NotifyCameraCut();
	
	bResetInterplation = true;
}


#undef LOCTEXT_NAMESPACE
