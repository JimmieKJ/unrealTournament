// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.h"
#include "UTGib.h"

#include "UTGibStaticMesh.generated.h"

UCLASS(Abstract, BlueprintType)
class UNREALTOURNAMENT_API AUTGibStaticMesh : public AUTGib
{
	GENERATED_BODY()
public:
	AUTGibStaticMesh(const FObjectInitializer& OI)
		: Super(OI)
	{
		Mesh = OI.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
		if (Mesh != NULL)
		{
			Mesh->bReceivesDecals = false;
			Mesh->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
			Mesh->SetCollisionObjectType(ECC_PhysicsBody);
			Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Mesh->SetNotifyRigidBodyCollision(true);
			Mesh->OnComponentHit.AddDynamic(this, &AUTGib::OnPhysicsCollision);
		}
		RootComponent = Mesh;
	}
	/** list of alternate meshes to randomly apply to Mesh instead of the default */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Gib)
	TArray<class UStaticMesh*> MeshChoices;

	virtual void PreInitializeComponents() override
	{
		UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Mesh);
		if (MeshChoices.Num() > 0 && SMComp != NULL)
		{
			if (SMComp->GetStaticMesh() != NULL)
			{
				MeshChoices.AddUnique(SMComp->GetStaticMesh());
			}
			SMComp->SetStaticMesh(MeshChoices[FMath::RandHelper(MeshChoices.Num())]);
		}
		Super::PreInitializeComponents();
	}
};
