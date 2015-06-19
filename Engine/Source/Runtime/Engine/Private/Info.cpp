// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

AInfo::AInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_Info;
			FText NAME_Info;
			FConstructorStatics()
				: SpriteTexture(TEXT("/Engine/EditorResources/S_Actor"))
				, ID_Info(TEXT("Info"))
				, NAME_Info(NSLOCTEXT("SpriteCategory", "Info", "Info"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.SpriteTexture.Get();
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Info;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Info;
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	NetUpdateFrequency = 10.0f;
	bHidden = true;
	bReplicateMovement = false;
	bCanBeDamaged = false;
}

#if WITH_EDITORONLY_DATA
/** Returns SpriteComponent subobject **/
UBillboardComponent* AInfo::GetSpriteComponent() const { return SpriteComponent; }
#endif
