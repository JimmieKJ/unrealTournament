// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCosmetic.h"


AUTCosmetic::AUTCosmetic(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRootComponent(ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneRootComp")));

	GetRootComponent()->Mobility = EComponentMobility::Movable;

	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CosmeticName = TEXT("Unnamed Cosmetic");
	CosmeticAuthor = TEXT("Anonymous");
}

void AUTCosmetic::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	CosmeticWearer = Cast<AUTCharacter>(GetOwner());

	if (GetRootComponent())
	{
		bool bRootNotPrimitiveComponent = true;

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		if (PrimComponent)
		{
			PrimComponent->bReceivesDecals = false;
			PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			bRootNotPrimitiveComponent = false;
			PrimComponent->bLightAttachmentsAsGroup = true;
			PrimComponent->bUseAsOccluder = false;
			PrimComponent->MarkRenderStateDirty();
		}

		TArray<USceneComponent*> Children;
		GetRootComponent()->GetChildrenComponents(true, Children);
		for (auto Child : Children)
		{
			PrimComponent = Cast<UPrimitiveComponent>(Child);
			if (PrimComponent)
			{
				if (bRootNotPrimitiveComponent)
				{
					SetRootComponent(PrimComponent);
					bRootNotPrimitiveComponent = false;
				}
				PrimComponent->bReceivesDecals = false;
				PrimComponent->bUseAsOccluder = false;
				PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				PrimComponent->bLightAttachmentsAsGroup = true;
				PrimComponent->MarkRenderStateDirty();
			}
		}
	}
}

void AUTCosmetic::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
	
	if (PrimComponent)
	{
		for (int32 i = 0; i < PrimComponent->GetNumMaterials(); i++)
		{
			// FIXME: NULL check is hack for editor reimport bug breaking number of materials
			if (PrimComponent->GetMaterial(i) != NULL)
			{
				CosmeticMIs.Add(PrimComponent->CreateAndSetMaterialInstanceDynamic(i));
			}
		}
	}
}

void AUTCosmetic::OnWearerHeadshot_Implementation()
{

}

void AUTCosmetic::OnWearerDeath_Implementation(TSubclassOf<UDamageType> DamageType)
{

}

void AUTCosmetic::OnVariantSelected_Implementation(int32 Variant)
{
	if (Variant >= 0 && Variant < VariantColorSwaps.Num())
	{
		for (UMaterialInstanceDynamic* MI : CosmeticMIs)
		{
			if (MI != NULL)
			{
				static FName NAME_CosmeticColor1(TEXT("CosmeticColor1"));
				static FName NAME_CosmeticColor2(TEXT("CosmeticColor2"));
				MI->SetVectorParameterValue(NAME_CosmeticColor1, VariantColorSwaps[Variant].Color1);
				MI->SetVectorParameterValue(NAME_CosmeticColor2, VariantColorSwaps[Variant].Color2);
			}
		}
	}
}