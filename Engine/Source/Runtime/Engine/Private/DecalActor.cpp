// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Components/BoxComponent.h"


#if WITH_EDITOR
namespace DecalEditorConstants
{
	/** Scale factor to apply to get nice scaling behaviour in-editor when using percentage-based scaling */
	static const float PercentageScalingMultiplier = 5.0f;

	/** Scale factor to apply to get nice scaling behaviour in-editor when using additive-based scaling */
	static const float AdditiveScalingMultiplier = 50.0f;
}
#endif

ADecalActor::ADecalActor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("NewDecalComponent"));
	Decal->RelativeScale3D = FVector(128.0f, 256.0f, 256.0f);

	Decal->RelativeRotation = FRotator(-90, 0, 0);

	RootComponent = Decal;

#if WITH_EDITORONLY_DATA
	BoxComponent = CreateEditorOnlyDefaultSubobject<UBoxComponent>(TEXT("DrawBox0"));
	if (BoxComponent != nullptr)
	{
		BoxComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

		BoxComponent->ShapeColor = FColor(80, 80, 200, 255);
		BoxComponent->bDrawOnlyIfSelected = true;
		BoxComponent->InitBoxExtent(FVector(1.0f, 1.0f, 1.0f));

		BoxComponent->AttachParent = Decal;
	}

	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FName ID_Decals;
			FText NAME_Decals;
			FConstructorStatics()
				: DecalTexture(TEXT("/Engine/EditorResources/S_DecalActorIcon"))
				, ID_Decals(TEXT("Decals"))
				, NAME_Decals(NSLOCTEXT("SpriteCategory", "Decals", "Decals"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (ArrowComponent)
		{
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->ArrowSize = 1.0f;
			ArrowComponent->ArrowColor = FColor(80, 80, 200, 255);
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Decals;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Decals;
			ArrowComponent->AttachParent = Decal;
			ArrowComponent->bAbsoluteScale = true;
			ArrowComponent->bIsScreenSizeScaled = true;
		}

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->AttachParent = Decal;
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->bAbsoluteScale = true;
			SpriteComponent->bReceivesDecals = false;
		}
	}
#endif // WITH_EDITORONLY_DATA

	bCanBeDamaged = false;
}

#if WITH_EDITOR
void ADecalActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (Decal)
	{
		Decal->RecreateRenderState_Concurrent();
	}
}

void ADecalActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Decal)
	{
		Decal->RecreateRenderState_Concurrent();
	}
}

void ADecalActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * (AActor::bUsePercentageBasedScaling ? DecalEditorConstants::PercentageScalingMultiplier : DecalEditorConstants::AdditiveScalingMultiplier);

	Super::EditorApplyScale(ModifiedScale, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
}

bool ADecalActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (Decal->DecalMaterial != nullptr)
	{
		Objects.Add(Decal->DecalMaterial);
	}

	return true;
}

#endif // WITH_EDITOR

void ADecalActor::SetDecalMaterial(class UMaterialInterface* NewDecalMaterial)
{
	if (Decal)
	{
		Decal->SetDecalMaterial(NewDecalMaterial);
	}
}

class UMaterialInterface* ADecalActor::GetDecalMaterial() const
{
	return Decal ? Decal->GetDecalMaterial() : NULL;
}

class UMaterialInstanceDynamic* ADecalActor::CreateDynamicMaterialInstance()
{
	return Decal ? Decal->CreateDynamicMaterialInstance() : NULL;
}

/** Returns Decal subobject **/
UDecalComponent* ADecalActor::GetDecal() const { return Decal; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
UArrowComponent* ADecalActor::GetArrowComponent() const { return ArrowComponent; }
/** Returns SpriteComponent subobject **/
UBillboardComponent* ADecalActor::GetSpriteComponent() const { return SpriteComponent; }
/** Returns BoxComponent subobject **/
UBoxComponent* ADecalActor::GetBoxComponent() const { return BoxComponent; }
#endif
