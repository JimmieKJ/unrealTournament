// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"
#include "UTPickupAmmo.h"
#include "UTPickupMessage.h"

#include "UTDroppedAmmoBox.generated.h"

UCLASS()
class AUTDroppedAmmoBox : public AUTDroppedPickup
{
	GENERATED_BODY()
public:
	AUTDroppedAmmoBox(const FObjectInitializer& OI)
		: Super(OI)
	{
		SMComp = OI.CreateDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("SMComp")));
		SMComp->AttachParent = Collision;
		static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(TEXT("/Game/RestrictedAssets/Environments/Liandri/Meshes/SM_TestCubeFaceWeighted.SM_TestCubeFaceWeighted"));
		SMComp->SetStaticMesh(BoxMesh.Object);
		SMComp->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
		SMComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UStaticMeshComponent* SMComp;

	/** ammo in the box */
	UPROPERTY(BlueprintReadWrite)
	TArray<FStoredAmmo> Ammo;

	void GiveTo_Implementation(APawn* Target) override
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(Target);
		if (UTC != NULL)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(UTC->GetController());
			TArray<UClass*> AmmoClasses;
			GetDerivedClasses(AUTPickupAmmo::StaticClass(), AmmoClasses);
			for (const FStoredAmmo& AmmoItem : Ammo)
			{
				UTC->AddAmmo(AmmoItem);
				if (PC != NULL)
				{
					// send message per ammo type
					UClass** AmmoType = AmmoClasses.FindByPredicate([&](UClass* TestClass) { return AmmoItem.Type == TestClass->GetDefaultObject<AUTPickupAmmo>()->Ammo.Type; });
					if (AmmoType != NULL)
					{
						PC->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, NULL, NULL, *AmmoType);
					}
				}
			}
		}
	}
};