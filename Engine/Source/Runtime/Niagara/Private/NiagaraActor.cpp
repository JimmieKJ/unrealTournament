// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "NiagaraPrivate.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEffect.h"

ANiagaraActor::ANiagaraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent0"));

	RootComponent = NiagaraComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject;
			FName ID_Effects;
			FText NAME_Effects;
			FConstructorStatics()
				: SpriteTextureObject(TEXT("/Engine/EditorResources/S_Emitter"))
				, ID_Effects(TEXT("Effects"))
				, NAME_Effects(NSLOCTEXT("SpriteCategory", "Effects", "Effects"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.SpriteTextureObject.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->bHiddenInGame = true;
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Effects;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Effects;
			SpriteComponent->AttachParent = NiagaraComponent;
			SpriteComponent->bReceivesDecals = false;
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(0, 255, 128);

			ArrowComponent->ArrowSize = 1.5f;
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Effects;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Effects;
			ArrowComponent->AttachParent = NiagaraComponent;
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Returns NiagaraComponent subobject **/
UNiagaraComponent* ANiagaraActor::GetNiagaraComponent() const { return NiagaraComponent; }
#if WITH_EDITORONLY_DATA
/** Returns SpriteComponent subobject **/
UBillboardComponent* ANiagaraActor::GetSpriteComponent() const { return SpriteComponent; }
/** Returns ArrowComponent subobject **/
UArrowComponent* ANiagaraActor::GetArrowComponent() const { return ArrowComponent; }
#endif

#if WITH_EDITOR
bool ANiagaraActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (UNiagaraEffect* Effect = NiagaraComponent->GetAsset())
	{
		Objects.Add(Effect);
	}

	return true;
}
#endif // WITH_EDITOR

