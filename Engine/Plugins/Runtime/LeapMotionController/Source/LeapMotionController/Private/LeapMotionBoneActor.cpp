// Fill out your copyright notice in the Description page of Project Settings.

#include "LeapMotionControllerPrivatePCH.h"
#include "LeapMotionBoneActor.h"

#include "LeapMotionHandActor.h"
#include "LeapMotionControllerComponent.h"



ALeapMotionBoneActor::ALeapMotionBoneActor(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowCollider = true;
	bShowMesh = true;

	// Initialize root primitive component. It only serves as a reference point. Collision & visual meshes are offset from it.
	USphereComponent* SphereComponent = ObjectInitializer.CreateDefaultSubobject<USphereComponent>(this, TEXT("RootSphereComponent"));
	SphereComponent->SetSphereRadius(0.001);
	SphereComponent->SetPhysicsMaxAngularVelocity(7200.0f);
	SphereComponent->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	SphereComponent->SetVisibility(false);

	RootComponent = SphereComponent;


	// Initalize colliding shape
	PrimitiveComponent = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("CapsuleComponent"));
	PrimitiveComponent->AttachTo(RootComponent);
	PrimitiveComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PrimitiveComponent->SetVisibility(true);
	PrimitiveComponent->SetHiddenInGame(false);

	// Initialize visual mesh
	StaticMeshComponent = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("StaticMeshComponent"));
	StaticMeshComponent->AttachTo(RootComponent);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComponent->SetVisibility(true);

	PrimaryActorTick.bCanEverTick = true;
}

ULeapMotionControllerComponent* ALeapMotionBoneActor::GetParentingControllerComponent() const
{
	ALeapMotionHandActor* HandActor = GetHandActor();
	return HandActor ? HandActor->GetParentingControllerComponent() : nullptr;
}

ALeapMotionHandActor* ALeapMotionBoneActor::GetHandActor() const
{
	return Cast<ALeapMotionHandActor>(GetAttachParentActor());
}

void ALeapMotionBoneActor::Init(ELeapBone LeapBone, float Scale, float Width, float Length, bool ShowCollider, bool ShowMesh)
{
	LeapBoneType = LeapBone;

	bShowCollider = ShowCollider;
	bShowMesh = ShowMesh;

	PrimitiveComponent->SetVisibility(bShowCollider);
	StaticMeshComponent->SetVisibility(bShowMesh);

	switch (LeapBone)
	{
	case ELeapBone::Forearm: 
		SetMeshForArm(); 
		Width *= 0.75f;
		break;
	case ELeapBone::Palm: 
		SetMeshForPalm(); 
		Width *= 0.75f;
		Length = 0.0f;
		break;
	default: 
		SetMeshForFinger();
		break;
	}

	const float PalmCenterOffset = 2.5f;
	float offset = LeapBone == ELeapBone::Palm ? -PalmCenterOffset * Scale : -Length * Scale * 0.5f;
	FVector BoneOffset(0.0f, 0.0f, offset);

	// Find capsule components & resize them
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* c : Components)
	{
		if (c != GetRootComponent())
		{
			UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(c);
			if (Capsule)
			{
				Capsule->SetCapsuleRadius(Scale * 0.5f * Width);
				Capsule->SetCapsuleHalfHeight(Scale * 0.5f * (Length + Width));
				if (LeapBone == ELeapBone::Palm)
				{
					Capsule->SetCapsuleRadius(Scale * 0.5f * Width * 0.5f);
					Capsule->SetCapsuleHalfHeight(Scale * 0.5f * (Length + Width) * 0.5f);
				}
				Capsule->AddLocalOffset(BoneOffset);
				Capsule->SetPhysicsMaxAngularVelocity(7200.0f);
			}

			UStaticMeshComponent* NonRootStaticMeshComponent = Cast<UStaticMeshComponent>(c);
			UStaticMesh* StaticMesh = NonRootStaticMeshComponent ? NonRootStaticMeshComponent->StaticMesh : nullptr;
			if (StaticMesh)
			{
				FBoxSphereBounds Bounds = StaticMesh->GetBounds();
				float ScaleX = 0.5f * Scale * Width / Bounds.BoxExtent.X;
				float ScaleY = 0.5f * Scale * Width / Bounds.BoxExtent.Y;
				float ScaleZ = 0.5f * Scale * (Length + Width) / Bounds.BoxExtent.Z;
				if (LeapBone == ELeapBone::Forearm)
				{
					ScaleX = 0.125f * Scale * Width / Bounds.BoxExtent.X;
					ScaleZ = 0.5f * Scale * (Length) / Bounds.BoxExtent.Z;
				}
				else if (LeapBone == ELeapBone::Palm)
				{
					ScaleX = 0.06125f * Scale * Width / Bounds.BoxExtent.X;
				}
				NonRootStaticMeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
				NonRootStaticMeshComponent->AddLocalOffset(BoneOffset);
				NonRootStaticMeshComponent->SetPhysicsMaxAngularVelocity(7200.0f);
			}
		}
	}
}

void ALeapMotionBoneActor::UpdateVisibility()
{
	PrimitiveComponent->SetVisibility(bShowCollider);
	StaticMeshComponent->SetVisibility(bShowMesh);
}

#if WITH_EDITOR
void ALeapMotionBoneActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateVisibility();
}
#endif


void ALeapMotionBoneActor::SetMeshForFinger()
{
	ULeapMotionControllerComponent* ControllerComponent = GetParentingControllerComponent();
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->FingerMesh) { StaticMeshComponent->SetStaticMesh(ControllerComponent->FingerMesh); }
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->Material) { StaticMeshComponent->SetMaterial(0, ControllerComponent->Material); }
}

void ALeapMotionBoneActor::SetMeshForPalm()
{
	ULeapMotionControllerComponent* ControllerComponent = GetParentingControllerComponent();
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->PalmMesh) { StaticMeshComponent->SetStaticMesh(ControllerComponent->PalmMesh); }
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->Material) { StaticMeshComponent->SetMaterial(0, ControllerComponent->Material); }
}

void ALeapMotionBoneActor::SetMeshForArm()
{
	ULeapMotionControllerComponent* ControllerComponent = GetParentingControllerComponent();
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->ArmMesh) { StaticMeshComponent->SetStaticMesh(ControllerComponent->ArmMesh); }
	if (StaticMeshComponent && ControllerComponent && ControllerComponent->Material) { StaticMeshComponent->SetMaterial(0, ControllerComponent->Material); }
}

