// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickupEnergy.h"
#include "UTSquadAI.h"

AUTPickupEnergy::AUTPickupEnergy(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	EnergyAmount = 0.04f;
	BaseDesireability = 0.4f;
	PickupMessageString = NSLOCTEXT("PickupMessage", "EnergyPickedUp", "Boost Energy");
}

void AUTPickupEnergy::BeginPlay()
{
	Super::BeginPlay();

	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	Mesh = (MeshComponents.Num() > 0) ? MeshComponents[0] : nullptr;
	if (GhostMeshMaterial != NULL)
	{
		if (GhostMesh != NULL)
		{
			UnregisterComponentTree(GhostMesh);
			GhostMesh = NULL;
		}
		if (Mesh != NULL)
		{
			GhostMesh = DuplicateObject<UMeshComponent>(Mesh, this);
			GhostMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

			TArray<USceneComponent*> ChildComps = GhostMesh->GetAttachChildren();
			for (USceneComponent* Child : ChildComps)
			{
				Child->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}

			GhostMesh->bRenderCustomDepth = false;
			GhostMesh->bRenderInMainPass = true;
			GhostMesh->CastShadow = false;
			for (int32 i = 0; i < GhostMesh->GetNumMaterials(); i++)
			{
				GhostMesh->SetMaterial(i, GhostMeshMaterial);
				static FName NAME_Normal(TEXT("Normal"));
				UMaterialInterface* OrigMat = Mesh->GetMaterial(i);
				UTexture* NormalTex = NULL;
				if (OrigMat != NULL && OrigMat->GetTextureParameterValue(NAME_Normal, NormalTex))
				{
					UMaterialInstanceDynamic* MID = GhostMesh->CreateAndSetMaterialInstanceDynamic(i);
					MID->SetTextureParameterValue(NAME_Normal, NormalTex);
				}
			}
			GhostMesh->RegisterComponent();
			GhostMesh->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
			if (GhostMesh->bAbsoluteScale) // SnapToTarget doesn't handle absolute...
			{
				GhostMesh->SetWorldScale3D(Mesh->GetComponentScale());
			}
			GhostMesh->SetVisibility(!State.bActive, true);
			GhostMesh->bShouldUpdatePhysicsVolume = false;
		}
	}
}

bool AUTPickupEnergy::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	AUTCharacter* P = Cast<AUTCharacter>(Other);
	AUTPlayerState* OtherPS = Cast<AUTPlayerState>(Other->PlayerState);
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	return Super::AllowPickupBy_Implementation(Other, bDefaultAllowPickup && P != nullptr && !P->IsRagdoll() && OtherPS != nullptr && GS != nullptr && OtherPS->GetRemainingBoosts() < GS->BoostRechargeMaxCharges);
}

void AUTPickupEnergy::GiveTo_Implementation(APawn* Target)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(Target->PlayerState);
	if (PS != nullptr)
	{
		AUTPickup::GiveTo_Implementation(Target);
		PS->BoostRechargePct += EnergyAmount;
		//Add to the stats pickup count
		if (StatsNameCount != NAME_None)
		{
			PS->ModifyStatsValue(StatsNameCount, 1);
			if (PS->Team)
			{
				PS->Team->ModifyStatsValue(StatsNameCount, 1);
			}

			AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
			if (GS != nullptr)
			{
				GS->ModifyStatsValue(StatsNameCount, 1);
			}
		}
	}
}

float AUTPickupEnergy::BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float PathDistance)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(Asker->PlayerState);
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	return (PS != nullptr && GS != nullptr && PS->GetRemainingBoosts() < GS->BoostRechargeMaxCharges) ? Super::BotDesireability_Implementation(Asker, RequestOwner, PathDistance) : 0.0f;
}
float AUTPickupEnergy::DetourWeight_Implementation(APawn* Asker, float PathDistance)
{
	return BotDesireability_Implementation(Asker, Asker->Controller, PathDistance);
}

void AUTPickupEnergy::SetPickupHidden(bool bNowHidden)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (Mesh != NULL)
		{
			if (GhostMesh != NULL)
			{
				Mesh->SetRenderInMainPass(!bNowHidden);
				Mesh->SetRenderCustomDepth(bNowHidden);
				Mesh->CastShadow = !bNowHidden;
				for (USceneComponent* Child : Mesh->GetAttachChildren())
				{
					Child->SetVisibility(!bNowHidden, true);
				}
				GhostMesh->SetVisibility(bNowHidden, true);
			}
			else
			{
				Mesh->SetHiddenInGame(bNowHidden, true);
				Mesh->SetVisibility(!bNowHidden, true);
			}

			// toggle audio components
			TArray<USceneComponent*> ChildComps;
			Mesh->GetChildrenComponents(true, ChildComps);
			for (int32 i = 0; i < ChildComps.Num(); i++)
			{
				UAudioComponent* AC = Cast<UAudioComponent>(ChildComps[i]);
				if (AC != NULL)
				{
					if (bNowHidden)
					{
						AC->Stop();
					}
					else
					{
						AC->Play();
					}
				}
			}
			// if previously there was no InventoryType or no Mesh then the whole Actor might have been hidden
			SetActorHiddenInGame(false);
		}
		else
		{
			Super::SetPickupHidden(bNowHidden);
		}
	}
}