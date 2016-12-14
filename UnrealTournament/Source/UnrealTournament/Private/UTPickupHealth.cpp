// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupHealth.h"
#include "UTSquadAI.h"

AUTPickupHealth::AUTPickupHealth(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HealAmount = 25;
	BaseDesireability = 0.4f;
	PickupMessageString = NSLOCTEXT("PickupMessage", "HealthPickedUp", "Health +50");
}

void AUTPickupHealth::BeginPlay()
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

int32 AUTPickupHealth::GetHealMax_Implementation(AUTCharacter* P)
{
	if (P == NULL)
	{
		return 0;
	}
	else
	{
		return bSuperHeal ? P->SuperHealthMax : P->HealthMax;
	}
}

bool AUTPickupHealth::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	AUTCharacter* P = Cast<AUTCharacter>(Other);
	return Super::AllowPickupBy_Implementation(Other, bDefaultAllowPickup && P != NULL && !P->IsRagdoll() && (bSuperHeal || P->Health < GetHealMax(P)));
}

void AUTPickupHealth::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		AUTPickup::GiveTo_Implementation(Target);
		P->Health = FMath::Max<int32>(P->Health, FMath::Min<int32>(P->Health + HealAmount, GetHealMax(P)));
		if (P->Health >= 100)
		{
			P->HealthRemovalAssists.Empty();
		}
		//Add to the stats pickup count
		AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
		if (PS != nullptr && StatsNameCount != NAME_None)
		{
			PS->ModifyStatsValue(StatsNameCount, 1);
			if (PS->Team)
			{
				PS->Team->ModifyStatsValue(StatsNameCount, 1);
			}

			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS != nullptr)
			{
				GS->ModifyStatsValue(StatsNameCount, 1);
			}

			//Send the pickup message to the spectators
			AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if ((UTGameMode != nullptr) && (HealAmount > 50))
			{
				UTGameMode->BroadcastSpectatorPickup(PS, StatsNameCount, GetClass());
			}
		}
	}
}

float AUTPickupHealth::BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float PathDistance)
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	else
	{
		AUTBot* B = Cast<AUTBot>(RequestOwner);

		float Desire = FMath::Min<int32>(P->Health + HealAmount, GetHealMax(P)) - P->Health;

		if (P->GetWeapon() != NULL && P->GetWeapon()->BaseAISelectRating > 0.5f)
		{
			Desire *= 1.7f;
		}
		if (bSuperHeal || P->Health < 45)
		{
			Desire = FMath::Min<float>(0.025f * Desire, 2.2);
			if (bSuperHeal && B != NULL && B->Skill + B->Personality.Tactics >= 4.0f) // TODO: work off of whether bot is powerup timing, since it's a related strategy
			{
				// high skill bots keep considering powerups that they don't need if they can still pick them up
				// to deny the enemy any chance of getting them
				Desire = FMath::Max<float>(Desire, 0.001f);
			}
			return Desire;
		}
		else
		{
			if (Desire > 6.0f)
			{
				Desire = FMath::Max<float>(Desire, 25.0f);
			}
			// TODO
			//else if (UTBot(C) != None && UTBot(C).bHuntPlayer)
			//	return 0;
				
			return FMath::Min<float>(0.017f * Desire, 2.0);
		}
	}
}
float AUTPickupHealth::DetourWeight_Implementation(APawn* Asker, float PathDistance)
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	else
	{
		// reduce distance for low value health pickups
		// TODO: maybe increase value if multiple adjacent pickups?
		int32 ActualHeal = FMath::Min<int32>(P->Health + HealAmount, GetHealMax(P)) - P->Health;
		if (PathDistance > float(ActualHeal * 200))
		{
			return 0.0f;
		}
		else
		{
			AUTBot* B = Cast<AUTBot>(P->Controller);
			if (B != NULL && B->GetSquad() != NULL && B->GetSquad()->HasHighPriorityObjective(B) && P->Health > P->HealthMax * 0.65f)
			{
				return ActualHeal * 0.01f;
			}
			else
			{
				return ActualHeal * 0.02f;
			}
		}
	}
}

void AUTPickupHealth::SetPickupHidden(bool bNowHidden)
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