// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/TriggerBase.h"

ATriggerBase::ATriggerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTextureObject;
		FName ID_Triggers;
		FText NAME_Triggers;
		FConstructorStatics()
			: TriggerTextureObject(TEXT("/Engine/EditorResources/S_Trigger"))
			, ID_Triggers(TEXT("Triggers"))
			, NAME_Triggers(NSLOCTEXT( "SpriteCategory", "Triggers", "Triggers" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bHidden = true;
	bCanBeDamaged = false;

	// ATriggerBase is requesting UShapeComponent which is abstract, however it is responsibility
	// of a derived class to override this type with ObjectInitializer.SetDefaultSubobjectClass.
	CollisionComponent = CreateAbstractDefaultSubobject<UShapeComponent>(TEXT("CollisionComp"));
	if (CollisionComponent)
	{
		RootComponent = CollisionComponent;
		CollisionComponent->bHiddenInGame = false;
	}

	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->Sprite = ConstructorStatics.TriggerTextureObject.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = false;
		SpriteComponent->AlwaysLoadOnClient = false;
		SpriteComponent->AlwaysLoadOnServer = false;
#if WITH_EDITORONLY_DATA
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Triggers;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Triggers;
#endif
		SpriteComponent->bIsScreenSizeScaled = true;
	}
}

/** Returns CollisionComponent subobject **/
UShapeComponent* ATriggerBase::GetCollisionComponent() const { return CollisionComponent; }
/** Returns SpriteComponent subobject **/
UBillboardComponent* ATriggerBase::GetSpriteComponent() const { return SpriteComponent; }
