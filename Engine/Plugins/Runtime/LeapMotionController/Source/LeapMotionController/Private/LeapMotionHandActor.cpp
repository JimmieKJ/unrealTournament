// Fill out your copyright notice in the Description page of Project Settings.

#include "LeapMotionControllerPrivatePCH.h"
#include "LeapMotionHandActor.h"

#include "LeapMotionControllerComponent.h"
#include "LeapMotionDevice.h"
#include "LeapMotionBoneActor.h"
#include "LeapMotionTypes.h"


static bool ConvertDeltaRotationsToAxisAngle(const FRotator& StartOrientation, const FRotator& EndOrientation, FVector& OutAxis, float& OutAngle);

ALeapMotionHandActor::ALeapMotionHandActor(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootSceneComponent"));
	Scale = 5.0f;

	PrimaryActorTick.bCanEverTick = true;
}

ULeapMotionControllerComponent* ALeapMotionHandActor::GetParentingControllerComponent() const
{
	return Cast<ULeapMotionControllerComponent>(RootComponent->AttachParent);
}

ALeapMotionBoneActor* ALeapMotionHandActor::GetBoneActor(ELeapBone LeapBone) const
{
	if (BoneActors.Num())
	{
		bool HasArm = (BoneActors.Num() == int(ELeapBone::Finger4Tip) + 1);
		int Index = (int)LeapBone - 1 + HasArm;
		if (0 <= Index && Index < BoneActors.Num())
		{
			return BoneActors[Index];
		}
	}
	return nullptr;
}

void ALeapMotionHandActor::Init(int32 HandIdParam, const TSubclassOf<class ALeapMotionBoneActor>& BoneBlueprint)
{
	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (Device && Device->IsConnected())
	{
		Leap::Hand Hand = Device->Frame().hand(HandIdParam);

		HandId = HandIdParam;
		bool IsLeft;
		Device->GetIsHandLeft(HandId, IsLeft);
		HandSide = IsLeft ? ELeapSide::Left : ELeapSide::Right;
	}

	CreateBones(BoneBlueprint);

	OnHandAdded.Broadcast(HandId);
}

void ALeapMotionHandActor::Update(float DeltaSeconds)
{
	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (Device && Device->IsConnected())
	{
		Device->GetPinchStrength(HandId, PinchStrength);
		Device->GetGrabStrength(HandId, GrabStrength);
	}

	UpdateBones(DeltaSeconds);

	OnHandUpdated.Broadcast(HandId, DeltaSeconds);
}

void ALeapMotionHandActor::UpdateVisibility()
{
	for (ALeapMotionBoneActor* Bone : BoneActors)
	{
		if (Bone)
		{
			Bone->bShowCollider = bShowCollider;
			Bone->bShowMesh = bShowMesh;
			Bone->UpdateVisibility();
		}
	}
}

#if WITH_EDITOR
void ALeapMotionHandActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateVisibility();
}
#endif

void ALeapMotionHandActor::Destroyed()
{
	OnHandRemoved.Broadcast(HandId);
	DestroyBones();
	Super::Destroyed();
}

void ALeapMotionHandActor::CreateBones(const TSubclassOf<class ALeapMotionBoneActor>& BoneBlueprint)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = GetInstigator();

	float CombinedScale = GetCombinedScale();

	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (Device && Device->IsConnected())
	{
		for (ELeapBone LeapBone = bShowArm ? ELeapBone::Forearm : ELeapBone::Palm; LeapBone <= ELeapBone::Finger4Tip; ((int8&)LeapBone)++)
		{
			FVector Position;
			FRotator Orientation;
			float Width;
			float Length;

			bool Success = Device->GetBonePostionAndOrientation(HandId, LeapBone, Position, Orientation);
			Success &= Device->GetBoneWidthAndLength(HandId, LeapBone, Width, Length);
			if (Success)
			{
				const FQuat RefQuat = GetRootComponent()->GetComponentQuat();
				Position = RefQuat * Position * CombinedScale + GetRootComponent()->GetComponentLocation();
				Orientation = (RefQuat * Orientation.Quaternion()).Rotator();

				UClass* BoneBlueprintClass = BoneBlueprint;
				ALeapMotionBoneActor* BoneActor = GetWorld()->SpawnActor<ALeapMotionBoneActor>(BoneBlueprintClass != nullptr ? BoneBlueprintClass : ALeapMotionBoneActor::StaticClass(), Position, Orientation, SpawnParams);
				if (BoneActor) 
				{
					BoneActors.Add(BoneActor);
#					if WITH_EDITOR
						BoneActor->SetActorLabel(*FString::Printf(TEXT("LeapBone:%s"), ANSI_TO_TCHAR(LEAP_GET_BONE_NAME(LeapBone))));
#					endif
					BoneActor->AttachRootComponentToActor(this, NAME_None, EAttachLocation::KeepWorldPosition, true);
					BoneActor->Init(LeapBone, CombinedScale, Width, Length, bShowCollider, bShowMesh);
				}
			}
		}
	}
}

void ALeapMotionHandActor::DestroyBones()
{
	for (AActor* Bone : BoneActors)
	{
		if (Bone) Bone->Destroy();
	}
	BoneActors.Empty();
}

void ALeapMotionHandActor::UpdateBones(float DeltaSeconds)
{
	if (BoneActors.Num() == 0) { return; }

	float CombinedScale = GetCombinedScale();

	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (Device && Device->IsConnected())
	{
		int BoneArrayIndex = 0;
		for (ELeapBone LeapBone = bShowArm ? ELeapBone::Forearm : ELeapBone::Palm; LeapBone <= ELeapBone::Finger4Tip; ((int8&)LeapBone)++)
		{
			FVector TargetPosition;
			FRotator TargetOrientation;

			bool Success = Device->GetBonePostionAndOrientation(HandId, LeapBone, TargetPosition, TargetOrientation);
			if (Success)
			{
				// Offset target position & rotation by the SpawnReference actor's transform
				const FQuat RefQuat = GetRootComponent()->GetComponentQuat();
				TargetPosition = RefQuat * TargetPosition * CombinedScale + GetRootComponent()->GetComponentLocation();
				TargetOrientation = (RefQuat * TargetOrientation.Quaternion()).Rotator();

				// Get current position & rotation
				ALeapMotionBoneActor* BoneActor = BoneActors[BoneArrayIndex++];
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(BoneActor->GetRootComponent());
				if (PrimitiveComponent)
				{
					PrimitiveComponent->SetRelativeLocationAndRotation(TargetPosition, TargetOrientation, true);
				}
			}
		}
	}
}

float ALeapMotionHandActor::GetCombinedScale()
{
	return Scale * GWorld->GetWorldSettings()->AWorldSettings::WorldToMeters / 100.0f;
}

static bool ConvertDeltaRotationsToAxisAngle(const FRotator& StartOrientation, const FRotator& EndOrientation, FVector& OutAxis, float& OutAngle)
{
	FQuat deltaRotation = EndOrientation.Quaternion() * StartOrientation.Quaternion().Inverse();
	deltaRotation.ToAxisAndAngle(OutAxis, OutAngle);
	return true;
}


