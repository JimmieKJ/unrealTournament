// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "VectorField/VectorFieldVolume.h"
#include "Components/VectorFieldComponent.h"
#include "Components/BillboardComponent.h"

AVectorFieldVolume::AVectorFieldVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VectorFieldComponent = CreateDefaultSubobject<UVectorFieldComponent>(TEXT("VectorFieldComponent0"));
	RootComponent = VectorFieldComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> EffectsTextureObject;
			FName ID_Effects;
			FText NAME_Effects;
			FConstructorStatics()
				: EffectsTextureObject(TEXT("/Engine/EditorResources/S_VectorFieldVol"))
				, ID_Effects(TEXT("Effects"))
				, NAME_Effects(NSLOCTEXT("SpriteCategory", "Effects", "Effects"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.EffectsTextureObject.Get();
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Effects;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Effects;
		SpriteComponent->bAbsoluteScale = true;
		SpriteComponent->AttachParent = VectorFieldComponent;
		SpriteComponent->bReceivesDecals = false;
	}
#endif // WITH_EDITORONLY_DATA
}


/** Returns VectorFieldComponent subobject **/
UVectorFieldComponent* AVectorFieldVolume::GetVectorFieldComponent() const { return VectorFieldComponent; }
#if WITH_EDITORONLY_DATA
/** Returns SpriteComponent subobject **/
UBillboardComponent* AVectorFieldVolume::GetSpriteComponent() const { return SpriteComponent; }
#endif
