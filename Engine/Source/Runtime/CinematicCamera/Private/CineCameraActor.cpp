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
	CineCameraComponent = Cast<UCineCameraComponent>(GetCameraComponent());

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(ShouldTickForTracking());
}

#if WITH_EDITOR
void ACineCameraActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetActorTickEnabled(ShouldTickForTracking());
}
#endif

void ACineCameraActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetActorTickEnabled(ShouldTickForTracking());

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

static const FColor DebugFocusPointSolidColor(102, 26, 204, 153);		// purple
static const FColor DebugFocusPointOutlineColor = FColor::Black;

void ACineCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CameraComponent && ShouldTickForTracking())
	{
		if (LookatTrackingSettings.bEnableLookAtTracking)
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

		if (CineCameraComponent->FocusSettings.TrackingFocusSettings.bDrawDebugTrackingFocusPoint)
		{
			AActor const* const TrackedActor = CineCameraComponent->FocusSettings.TrackingFocusSettings.ActorToTrack;

			FVector FocusPoint;
			if (TrackedActor)
			{
				FTransform const BaseTransform = TrackedActor->GetActorTransform();
				FocusPoint = BaseTransform.TransformPosition(CineCameraComponent->FocusSettings.TrackingFocusSettings.RelativeOffset);
			}
			else
			{
				FocusPoint = CineCameraComponent->FocusSettings.TrackingFocusSettings.RelativeOffset;
			}
			
			::DrawDebugSolidBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointSolidColor);
			::DrawDebugBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointOutlineColor);
		}

#if WITH_EDITORONLY_DATA
		if (CineCameraComponent->FocusSettings.bDrawDebugFocusPlane)
		{
			CineCameraComponent->UpdateDebugFocusPlane();
		}
#endif // WITH_EDITORONLY_DATA
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

bool ACineCameraActor::ShouldTickForTracking() const
{
	bool bShouldTickForTracking = 
		LookatTrackingSettings.bEnableLookAtTracking || 
		CineCameraComponent->FocusSettings.TrackingFocusSettings.bDrawDebugTrackingFocusPoint;

#if WITH_EDITORONLY_DATA
	if (CineCameraComponent->FocusSettings.bDrawDebugFocusPlane)
	{
		bShouldTickForTracking = true;
	}
#endif // WITH_EDITORONLY_DATA

	return bShouldTickForTracking;
}


#undef LOCTEXT_NAMESPACE
