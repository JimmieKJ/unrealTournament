// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.h"
#include "UTGib.h"

#include "UTGibSkeletalMesh.generated.h"

UCLASS(Abstract, BlueprintType)
class UNREALTOURNAMENT_API AUTGibSkeletalMesh : public AUTGib
{
	GENERATED_BODY()
public:
	AUTGibSkeletalMesh(const FObjectInitializer& OI)
		: Super(OI)
	{
		Mesh = OI.CreateOptionalDefaultSubobject<USkeletalMeshComponent>(this, FName(TEXT("Mesh")));
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
	TArray<class USkeletalMesh*> MeshChoices;

	virtual void PreInitializeComponents() override
	{
		USkeletalMeshComponent* SKComp = Cast<USkeletalMeshComponent>(Mesh);
		if (MeshChoices.Num() > 0 && SKComp != NULL)
		{
			if (SKComp->SkeletalMesh != NULL)
			{
				MeshChoices.AddUnique(SKComp->SkeletalMesh);
			}
			SKComp->SetSkeletalMesh(MeshChoices[FMath::RandHelper(MeshChoices.Num())]);
		}
		Super::PreInitializeComponents();
	}
};
