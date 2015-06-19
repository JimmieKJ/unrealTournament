// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraAnim.h"
#include "Net/UnrealNetwork.h"
#include "MessageLog.h"
#include "UObjectToken.h"

#define LOCTEXT_NAMESPACE "CameraActor"

//////////////////////////////////////////////////////////////////////////
// ACameraActor

ACameraActor::ACameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Setup camera defaults
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->FieldOfView = 90.0f;
	CameraComponent->bConstrainAspectRatio = true;
	CameraComponent->AspectRatio = 1.777778f;
	CameraComponent->PostProcessBlendWeight = 1.0f;

	// Make the camera component the root component
	RootComponent = CameraComponent;

	// Initialize deprecated properties (needed for backwards compatibility due to delta serialization)
	FOVAngle_DEPRECATED = 90.0f;
	bConstrainAspectRatio_DEPRECATED = true;
	AspectRatio_DEPRECATED = 1.777778f;
	PostProcessBlendWeight_DEPRECATED = 1.0f;
	// End of deprecated property initialization

	PrimaryActorTick.bCanEverTick = true;
}

void ACameraActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if ((Ar.UE4Ver() < VER_UE4_CAMERA_ACTOR_USING_CAMERA_COMPONENT) && Ar.IsLoading())
	{
		CameraComponent->bConstrainAspectRatio = bConstrainAspectRatio_DEPRECATED;
		CameraComponent->ProjectionMode = ECameraProjectionMode::Perspective;
		CameraComponent->AspectRatio = AspectRatio_DEPRECATED;
		CameraComponent->FieldOfView = FOVAngle_DEPRECATED;
		CameraComponent->PostProcessBlendWeight = PostProcessBlendWeight_DEPRECATED;
		CameraComponent->PostProcessSettings = PostProcessSettings_DEPRECATED;
	}
}

void ACameraActor::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	if (GetLinkerUE4Version() < VER_UE4_CAMERA_ACTOR_USING_CAMERA_COMPONENT)
	{
		USceneComponent* OldRoot = RootComponent;
		USceneComponent* OldAttachParent = OldRoot->AttachParent;

		Super::PostLoadSubobjects(OuterInstanceGraph);

		CameraComponent->AttachParent = OldAttachParent;
		OldRoot->AttachParent = NULL;
	}
	else
	{
		Super::PostLoadSubobjects(OuterInstanceGraph);
	}
}

#if WITH_EDITOR
void ACameraActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PreviewedCameraAnim.IsValid() && CameraComponent)
	{
		PreviewedCameraAnim->BaseFOV = CameraComponent->FieldOfView;
		PreviewedCameraAnim->BasePostProcessSettings = CameraComponent->PostProcessSettings;
		PreviewedCameraAnim->BasePostProcessBlendWeight = CameraComponent->PostProcessBlendWeight;
	}
}
#endif


int32 ACameraActor::GetAutoActivatePlayerIndex() const
{
	if (AutoActivateForPlayer != EAutoReceiveInput::Disabled)
	{
		const int32 PlayerIndex = int32(AutoActivateForPlayer.GetValue()) - 1;
		return PlayerIndex;
	}
	else
	{
		return INDEX_NONE;
	}
}

void ACameraActor::BeginPlay()
{
	if (AutoActivateForPlayer != EAutoReceiveInput::Disabled && GetNetMode() != NM_Client)
	{
		const int32 PlayerIndex = GetAutoActivatePlayerIndex();
		
		// Always put it in the pool of available auto-activate cameras.
		GetWorld()->RegisterAutoActivateCamera(this, PlayerIndex);

		// If we find a matching PC, bind to it immediately.
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex);
		if (PC)
		{
			PC->SetViewTarget(this);
		}
	}

	Super::BeginPlay();
}

#undef LOCTEXT_NAMESPACE

/** Returns CameraComponent subobject **/
UCameraComponent* ACameraActor::GetCameraComponent() const { return CameraComponent; }
