// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsEngine/PhysicsThrusterComponent.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/PhysicsThruster.h"


UPhysicsThrusterComponent::UPhysicsThrusterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	ThrustStrength = 100.0;
}

void UPhysicsThrusterComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Applied force to the base, so if we don't have one, do nothing.
	if( bIsActive && AttachParent )
	{
		FVector WorldForce = ThrustStrength * ComponentToWorld.TransformVectorNoScale( FVector(-1.f,0.f,0.f) );

		UPrimitiveComponent* BasePrimComp = Cast<UPrimitiveComponent>(AttachParent);
		if(BasePrimComp)
		{
			BasePrimComp->AddForceAtLocation(WorldForce, GetComponentLocation(), NAME_None);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

APhysicsThruster::APhysicsThruster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThrusterComponent = CreateDefaultSubobject<UPhysicsThrusterComponent>(TEXT("Thruster0"));
	RootComponent = ThrusterComponent;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> ThrusterTexture;
			FName ID_Physics;
			FText NAME_Physics;
			FConstructorStatics()
				: ThrusterTexture(TEXT("/Engine/EditorResources/S_Thruster"))
				, ID_Physics(TEXT("Physics"))
				, NAME_Physics(NSLOCTEXT("SpriteCategory", "Physics", "Physics"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (ArrowComponent)
		{
			ArrowComponent->ArrowSize = 1.7f;
			ArrowComponent->ArrowColor = FColor(255, 180, 0);

			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Physics;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Physics;
			ArrowComponent->AttachParent = ThrusterComponent;
			ArrowComponent->bIsScreenSizeScaled = true;
		}

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.ThrusterTexture.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Physics;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Physics;
			SpriteComponent->AttachParent = ThrusterComponent;
			SpriteComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Returns ThrusterComponent subobject **/
UPhysicsThrusterComponent* APhysicsThruster::GetThrusterComponent() const { return ThrusterComponent; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
UArrowComponent* APhysicsThruster::GetArrowComponent() const { return ArrowComponent; }
/** Returns SpriteComponent subobject **/
UBillboardComponent* APhysicsThruster::GetSpriteComponent() const { return SpriteComponent; }
#endif
