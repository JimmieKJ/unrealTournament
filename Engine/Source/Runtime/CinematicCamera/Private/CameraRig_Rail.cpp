// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CinematicCameraPrivate.h"
#include "CameraRig_Rail.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

#define LOCTEXT_NAMESPACE "CameraRig_Rail"

ACameraRig_Rail::ACameraRig_Rail(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CurrentPositionOnRail = 0.f;

	// Create components
	TransformComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TransformComponent"));

	// Make the scene component the root component
	RootComponent = TransformComponent;

	RailSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("RailSplineComponent"));
	RailSplineComponent->AttachParent = TransformComponent;

	RailCameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("RailCameraMount"));
	RailCameraMount->AttachParent = RailSplineComponent;

	if (!IsRunningCommandlet() && !IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> RailMesh(TEXT("/Engine/EditorMeshes/Camera/SM_RailRig_Track.SM_RailRig_Track"));
		PreviewRailStaticMesh = RailMesh.Object;

		static ConstructorHelpers::FObjectFinder<UStaticMesh> MountMesh(TEXT("/Engine/EditorMeshes/Camera/SM_RailRig_Mount.SM_RailRig_Mount"));
		PreviewMesh_Mount = CreateOptionalDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh_Mount"));
		if (PreviewMesh_Mount)
		{
			PreviewMesh_Mount->SetStaticMesh(MountMesh.Object);
			PreviewMesh_Mount->AlwaysLoadOnClient = false;
			PreviewMesh_Mount->AlwaysLoadOnServer = false;
			PreviewMesh_Mount->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			PreviewMesh_Mount->bHiddenInGame = true;
			PreviewMesh_Mount->CastShadow = false;
			PreviewMesh_Mount->PostPhysicsComponentTick.bCanEverTick = false;

			PreviewMesh_Mount->AttachParent = RailCameraMount;
		}
	}
}

USplineMeshComponent* ACameraRig_Rail::CreateSplinePreviewSegment()
{
	USplineMeshComponent* const Segment = NewObject<USplineMeshComponent>(this);
	if (Segment)
	{
		Segment->SetStaticMesh(PreviewRailStaticMesh);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->AlwaysLoadOnClient = false;
		Segment->AlwaysLoadOnServer = false;
		Segment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Segment->bHiddenInGame = true;
		Segment->CastShadow = false;
		Segment->PostPhysicsComponentTick.bCanEverTick = false;
	 
		Segment->AttachParent = TransformComponent;
		Segment->RegisterComponent();
	}

	return Segment;
}

void ACameraRig_Rail::UpdatePreviewMeshes()
{
	if (PreviewRailStaticMesh)
	{
		int32 const NumSplinePoints = RailSplineComponent->GetNumberOfSplinePoints();
		int32 const NumNeededPreviewMeshes = NumSplinePoints - 1;

		// make sure our preview mesh array is correctly sized and populated
		{
			int32 const NumExistingPreviewMeshes = PreviewRailMeshSegments.Num();
			if (NumExistingPreviewMeshes > NumNeededPreviewMeshes)
			{
				// we have too many meshes, remove some
				int32 const NumToRemove = NumExistingPreviewMeshes - NumNeededPreviewMeshes;
				for (int Idx = 0; Idx < NumToRemove; ++Idx)
				{
					USplineMeshComponent* const ElementToRemove = PreviewRailMeshSegments.Pop();
					ElementToRemove->UnregisterComponent();
				}
			}
			else if (NumExistingPreviewMeshes < NumNeededPreviewMeshes)
			{
				int32 const NumToAdd = NumNeededPreviewMeshes - NumExistingPreviewMeshes;

				for (int32 Idx = 0; Idx < NumToAdd; ++Idx)
				{
					USplineMeshComponent* PreviewMesh = CreateSplinePreviewSegment();
					PreviewRailMeshSegments.Add(PreviewMesh);
				}
			}
			check(PreviewRailMeshSegments.Num() == NumNeededPreviewMeshes);
		}

		for (int PtIdx = 0; PtIdx < NumSplinePoints - 1; ++PtIdx)
		{
			FVector StartLoc, StartTangent, EndLoc, EndTangent;
			RailSplineComponent->GetLocationAndTangentAtSplinePoint(PtIdx, StartLoc, StartTangent, ESplineCoordinateSpace::Local);
			RailSplineComponent->GetLocationAndTangentAtSplinePoint(PtIdx + 1, EndLoc, EndTangent, ESplineCoordinateSpace::Local);

			USplineMeshComponent* const SplineMeshComp = PreviewRailMeshSegments[PtIdx];
			SplineMeshComp->SetStartScale(FVector2D(0.1f, 0.1f), false);
			SplineMeshComp->SetEndScale(FVector2D(0.1f, 0.1f), false);
			SplineMeshComp->SetForwardAxis(ESplineMeshAxis::Z);
			SplineMeshComp->SetStartAndEnd(StartLoc, StartTangent, EndLoc, EndTangent, true);
		}
	}
}

void ACameraRig_Rail::UpdateRailComponents()
{
	if (RailSplineComponent && RailCameraMount)
	{
		FVector const MountPos = RailSplineComponent->GetLocationAtDistanceAlongSpline(CurrentPositionOnRail, ESplineCoordinateSpace::World);
		RailCameraMount->SetWorldLocation(MountPos);
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// set up preview mesh to match #todo
		UpdatePreviewMeshes();
	}
#endif
}

#if WITH_EDITOR
USceneComponent* ACameraRig_Rail::GetDefaultAttachComponent() const
{
	return RailCameraMount;
}

void ACameraRig_Rail::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateRailComponents();

	if (RailSplineComponent)
	{
		// clamp position to spline limits
		float const SplineLen = RailSplineComponent->GetSplineLength();
		CurrentPositionOnRail = FMath::Clamp(CurrentPositionOnRail, 0.f, SplineLen);
	}
}
#endif

void ACameraRig_Rail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// feed exposed API into underlying components
	UpdateRailComponents();
}

bool ACameraRig_Rail::ShouldTickIfViewportsOnly() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
