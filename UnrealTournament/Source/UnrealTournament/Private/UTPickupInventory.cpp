// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UnrealNetwork.h"
#include "UTPickupMessage.h"
#include "UTWorldSettings.h"
#include "UTFlagRunGameState.h"

AUTPickupInventory::AUTPickupInventory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	FloatHeight = 50.0f;
	bAllowRotatingPickup = true;
	bHasTacComView = true;
	bHasEverSpawned = false;
	bNotifySpawnForOffense = true;
	bNotifySpawnForDefense = true;
}

void AUTPickupInventory::BeginPlay()
{
	AActor::BeginPlay(); // skip UTPickup as SetInventoryType() will handle delayed spawn

	if (BaseEffect != NULL && BaseTemplateAvailable != NULL)
	{
		BaseEffect->SetTemplate(BaseTemplateAvailable);
	}

	if (Role == ROLE_Authority)
	{
		SetInventoryType(InventoryType);
	}
	else
	{
		InventoryTypeUpdated();
	}

	AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
	if (NavData != NULL)
	{
		NavData->AddToNavigation(this);
	}
}

#if WITH_EDITOR
void AUTPickupInventory::CreateEditorPickupMesh()
{
	if (GetWorld() != NULL && GetWorld()->WorldType == EWorldType::Editor)
	{
		CreatePickupMesh(this, EditorMesh, InventoryType, FloatHeight, RotationOffset, false);
		if (EditorMesh != NULL)
		{
			EditorMesh->SetHiddenInGame(true);
		}
	}
}
void AUTPickupInventory::PreEditUndo()
{
	UnregisterComponentTree(EditorMesh);
	EditorMesh = NULL;
	Super::PreEditUndo();
}
void AUTPickupInventory::PostEditUndo()
{
	Super::PostEditUndo();
	CreateEditorPickupMesh();
}
void AUTPickupInventory::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	CreateEditorPickupMesh();
}
void AUTPickupInventory::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// we have a special class for weapons so don't allow setting them here
	if (PropertyChangedEvent.Property == NULL || PropertyChangedEvent.Property->GetFName() == FName(TEXT("InventoryType")))
	{
		if (InventoryType->IsChildOf(AUTWeapon::StaticClass()))
		{
			InventoryType = NULL;
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Use UTPickupWeapon for weapon pickups.")));
		}
	}

	CreateEditorPickupMesh();
}
#endif

void AUTPickupInventory::SetInventoryType(TSubclassOf<AUTInventory> NewType)
{
	InventoryType = NewType;
	if (InventoryType != NULL)
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		RespawnTime = Game ? Game->OverrideRespawnTime(this, InventoryType) : InventoryType.GetDefaultObject()->RespawnTime;
		bDelayedSpawn = InventoryType.GetDefaultObject()->bDelayedSpawn;
		BaseDesireability = InventoryType.GetDefaultObject()->BasePickupDesireability;
		bFixedRespawnInterval = InventoryType.GetDefaultObject()->bFixedRespawnInterval;
	}
	else
	{
		RespawnTime = 0.0f;
		bFixedRespawnInterval = false;
	}
	InventoryTypeUpdated();
	if (Role == ROLE_Authority && GetWorld()->GetAuthGameMode<AUTGameMode>() != NULL && GetWorld()->GetAuthGameMode<AUTGameMode>()->HasMatchStarted())
	{
		if (InventoryType == NULL || bDelayedSpawn)
		{
			StartSleeping();
		}
		else
		{
			WakeUp();
		}
	}
}

/** recursively instance anything attached to the pickup mesh template */
static void CreatePickupMeshAttachments(AActor* Pickup, UClass* PickupInventoryType, USceneComponent* CurrentAttachment, FName TemplateName, const TArray<USceneComponent*>& NativeCompList, const TArray<USCS_Node*>& BPNodes)
{
	for (int32 i = 0; i < NativeCompList.Num(); i++)
	{
		if (NativeCompList[i]->GetAttachParent() == CurrentAttachment)
		{
			USceneComponent* NewComp = NewObject<USceneComponent>(Pickup, NativeCompList[i]->GetClass(), NAME_None, RF_NoFlags, NativeCompList[i]);			
			NewComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
						
			TArray<USceneComponent*> ChildComps = NewComp->GetAttachChildren();
			for (USceneComponent* Child : ChildComps)
			{
				Child->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}

			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(NewComp);
			if (Prim != NULL)
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prim->bShouldUpdatePhysicsVolume = false;
			}
			NewComp->RegisterComponent();
			NewComp->AttachToComponent(CurrentAttachment, FAttachmentTransformRules::KeepRelativeTransform, NewComp->GetAttachSocketName());
			// recurse
			CreatePickupMeshAttachments(Pickup, PickupInventoryType, NewComp, NativeCompList[i]->GetFName(), NativeCompList, BPNodes);
		}
	}
	for (int32 i = 0; i < BPNodes.Num(); i++)
	{
		USceneComponent* ComponentTemplate = Cast<USceneComponent>(BPNodes[i]->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(PickupInventoryType)));
		if (BPNodes[i]->ComponentTemplate != NULL && BPNodes[i]->ParentComponentOrVariableName == TemplateName)
		{
			USceneComponent* NewComp = NewObject<USceneComponent>(Pickup, BPNodes[i]->ComponentTemplate->GetClass(), NAME_None, RF_NoFlags, BPNodes[i]->ComponentTemplate);			
			NewComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

			TArray<USceneComponent*> ChildComps = NewComp->GetAttachChildren();
			for (USceneComponent* Child : ChildComps)
			{
				Child->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}

			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(NewComp);
			if (Prim != NULL)
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prim->bShouldUpdatePhysicsVolume = false;
			}
			NewComp->bAutoRegister = false;
			NewComp->RegisterComponent();
			NewComp->AttachToComponent(CurrentAttachment, FAttachmentTransformRules::KeepRelativeTransform, BPNodes[i]->AttachToName);
			// recurse
			CreatePickupMeshAttachments(Pickup, PickupInventoryType, NewComp, BPNodes[i]->GetVariableName(), NativeCompList, BPNodes);

			// The behavior of PIE has changed in 4.13. When the object was duplicated for PIE in previous versions, it would not have the particle system components.
			// Just make the editor ones invisible, it breaks real time preview, but would rather have PIE work.
			if (Cast<UParticleSystemComponent>(NewComp) && Pickup->GetWorld()->WorldType == EWorldType::Editor)
			{
				Cast<UParticleSystemComponent>(NewComp)->SetVisibility(false, true);
			}
		}
	}
}

void AUTPickupInventory::CreatePickupMesh(AActor* Pickup, UMeshComponent*& PickupMesh, TSubclassOf<AUTInventory> PickupInventoryType, float MeshFloatHeight, const FRotator& RotationOffset, bool bAllowRotating)
{
	if (PickupInventoryType == NULL)
	{
		if (PickupMesh != NULL)
		{
			UnregisterComponentTree(PickupMesh);
			PickupMesh = NULL;
		}
	}
	else
	{
		FVector OverrideScale(FVector::ZeroVector);
		UMeshComponent* NewMesh = PickupInventoryType.GetDefaultObject()->GetPickupMeshTemplate(OverrideScale);
		if (NewMesh == NULL)
		{
			if (PickupMesh != NULL)
			{
				UnregisterComponentTree(PickupMesh);
				PickupMesh = NULL;
			}
		}
		else
		{
			USkeletalMeshComponent* SkelTemplate = Cast<USkeletalMeshComponent>(NewMesh);
			UStaticMeshComponent* StaticTemplate = Cast<UStaticMeshComponent>(NewMesh);
			if (PickupMesh == NULL || PickupMesh->GetClass() != NewMesh->GetClass() || PickupMesh->GetMaterials() != NewMesh->GetMaterials() ||
				(SkelTemplate != NULL && ((USkeletalMeshComponent*)PickupMesh)->SkeletalMesh != SkelTemplate->SkeletalMesh) ||
				(StaticTemplate != NULL && ((UStaticMeshComponent*)PickupMesh)->GetStaticMesh() != StaticTemplate->GetStaticMesh()))
			{
				if (PickupMesh != NULL)
				{
					UnregisterComponentTree(PickupMesh);
					PickupMesh = NULL;
				}
				PickupMesh = NewObject<UMeshComponent>(Pickup, NewMesh->GetClass(), NAME_None, RF_NoFlags, NewMesh);
				PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PickupMesh->bShouldUpdatePhysicsVolume = false;
				PickupMesh->bUseAttachParentBound = false;
				PickupMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

				TArray<USceneComponent*> ChildComps = PickupMesh->GetAttachChildren();
				for (USceneComponent* Child : ChildComps)
				{
					Child->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				}

				PickupMesh->RelativeRotation = FRotator::ZeroRotator;
				PickupMesh->RelativeLocation.Z += MeshFloatHeight;
				if (!OverrideScale.IsZero())
				{
					PickupMesh->SetWorldScale3D(OverrideScale);
				}
				if (SkelTemplate != NULL)
				{
					((USkeletalMeshComponent*)PickupMesh)->bForceRefpose = true;
				}
				PickupMesh->bAutoRegister = false;
				PickupMesh->RegisterComponent();
				PickupMesh->AttachToComponent(Pickup->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				FVector Offset = Pickup->GetRootComponent()->ComponentToWorld.InverseTransformVectorNoScale(PickupMesh->Bounds.Origin - PickupMesh->GetComponentToWorld().GetLocation());
				PickupMesh->SetRelativeLocation(PickupMesh->GetRelativeTransform().GetLocation() - Offset);
				PickupMesh->SetRelativeRotation(PickupMesh->RelativeRotation + RotationOffset);
				// if there's a rotation component, set it up to rotate the pickup mesh
				if (bAllowRotating && Pickup->GetWorld()->WorldType != EWorldType::Editor) // not if editor preview, because that will cause the preview component to exist in game and we want it to be editor only
				{
					TArray<URotatingMovementComponent*> RotationComps;
					Pickup->GetComponents<URotatingMovementComponent>(RotationComps);
					if (RotationComps.Num() > 0)
					{
						RotationComps[0]->SetUpdatedComponent(PickupMesh);
						RotationComps[0]->PivotTranslation = Offset;
					}
				}

				// see if the pickup mesh has any attached children we should also instance
				TArray<USceneComponent*> NativeCompList;
				PickupInventoryType.GetDefaultObject()->GetComponents<USceneComponent>(NativeCompList);
				TArray<USCS_Node*> ConstructionNodes;
				{
					TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
					UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(PickupInventoryType, ParentBPClassStack);
					for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
					{
						const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
						if (CurrentBPGClass->SimpleConstructionScript)
						{
							ConstructionNodes += CurrentBPGClass->SimpleConstructionScript->GetAllNodes();
						}
					}
				}
				CreatePickupMeshAttachments(Pickup, PickupInventoryType, PickupMesh, NewMesh->GetFName(), NativeCompList, ConstructionNodes);
			}
			else if (PickupMesh->GetAttachParent() != Pickup->GetRootComponent())
			{
				PickupMesh->AttachToComponent(Pickup->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				FVector Offset = Pickup->GetRootComponent()->ComponentToWorld.InverseTransformVectorNoScale(PickupMesh->Bounds.Origin - PickupMesh->GetComponentToWorld().GetLocation());
				PickupMesh->SetRelativeLocation(PickupMesh->GetRelativeTransform().GetLocation() - Offset + FVector(0.0f, 0.0f, MeshFloatHeight));
			}
		}
	}
}

void AUTPickupInventory::InventoryTypeUpdated_Implementation()
{
	CreatePickupMesh(this, Mesh, InventoryType, FloatHeight, RotationOffset, bAllowRotatingPickup);
	if (Mesh)
	{
		Mesh->bShouldUpdatePhysicsVolume = false;
	}
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
	if (Mesh != NULL && !State.bActive && Role < ROLE_Authority)
	{
		// if State was received first whole actor might have been hidden, reset everything
		StartSleeping();
		OnRep_RespawnTimeRemaining();
	}

	IconColor = (InventoryType != NULL) ? InventoryType.GetDefaultObject()->IconColor : FLinearColor::White;
	HUDIcon = (InventoryType != NULL) ? InventoryType.GetDefaultObject()->HUDIcon : FCanvasIcon();
	MinimapIcon = (InventoryType != NULL) ? InventoryType.GetDefaultObject()->MinimapIcon : FCanvasIcon();
	TakenSound = (InventoryType != NULL) ? TakenSound = InventoryType.GetDefaultObject()->PickupSound : GetClass()->GetDefaultObject<AUTPickupInventory>()->TakenSound;

	if ((GetWorld()->GetNetMode() != NM_DedicatedServer) && InventoryType && InventoryType.GetDefaultObject()->PickupSpawnAnnouncement)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->AddPostRenderedActor(this);
		}
	}
}

void AUTPickupInventory::Reset_Implementation()
{
	bHasEverSpawned = false;
	GetWorldTimerManager().ClearTimer(SpawnVoiceLineTimer);
	if (InventoryType == NULL)
	{
		StartSleeping();
	}
	else
	{
		Super::Reset_Implementation();
	}
}

void AUTPickupInventory::PlayRespawnEffects()
{
	Super::PlayRespawnEffects();
	if (InventoryType && (InventoryType.GetDefaultObject()->PickupSpawnAnnouncement || (InventoryType.GetDefaultObject()->PickupAnnouncementName != NAME_None)) && (Role==ROLE_Authority))
	{
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GM && GM->bAllowPickupAnnouncements && GM->IsMatchInProgress())
		{
			if (InventoryType.GetDefaultObject()->PickupSpawnAnnouncement)
			{
				GM->BroadcastLocalized(this, InventoryType.GetDefaultObject()->PickupSpawnAnnouncement, InventoryType.GetDefaultObject()->PickupAnnouncementIndex, nullptr, nullptr, InventoryType.GetDefaultObject());
				bHasEverSpawned = true;
			}
			if (InventoryType.GetDefaultObject()->PickupAnnouncementName != NAME_None)
			{
				GetWorldTimerManager().SetTimer(SpawnVoiceLineTimer, this, &AUTPickupInventory::PlaySpawnVoiceLine, bHasEverSpawned ? 30.f : 10.f);
			}
			bHasEverSpawned = true;
		}
	}
}

void AUTPickupInventory::PlaySpawnVoiceLine()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && (GS->IsMatchIntermission() || !GS->IsMatchInProgress()))
	{
		return;
	}

	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GM && GM->bAllowPickupAnnouncements)
	{
		// find player to announce this pickup 
		AUTPlayerState* Speaker = nullptr;
		bool bHasPlayedForRed = false;
		bool bHasPlayedForBlue = false;

		// maybe don't announce for one team
		AUTFlagRunGameState* FRGameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
		if (FRGameState)
		{
			bHasPlayedForRed = FRGameState->bRedToCap ? !bNotifySpawnForOffense : !bNotifySpawnForDefense;
			bHasPlayedForBlue = FRGameState->bRedToCap ? !bNotifySpawnForDefense : !bNotifySpawnForOffense;
			if (bHasPlayedForRed && bHasPlayedForBlue)
			{
				return;
			}
		}
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>((*Iterator)->PlayerState);
			if (UTPS && UTPS->Team)
			{
				if (!bHasPlayedForRed && (UTPS->Team->TeamIndex == 0))
				{
					UTPS->AnnounceStatus(InventoryType.GetDefaultObject()->PickupAnnouncementName, 0);
					bHasPlayedForRed = true;
				}
				else if (!bHasPlayedForBlue && (UTPS->Team->TeamIndex == 1))
				{
					UTPS->AnnounceStatus(InventoryType.GetDefaultObject()->PickupAnnouncementName, 0);
					bHasPlayedForBlue = true;
				}
				if (bHasPlayedForRed && bHasPlayedForBlue)
				{
					break;
				}
			}
		}
		GetWorldTimerManager().SetTimer(SpawnVoiceLineTimer, this, &AUTPickupInventory::PlaySpawnVoiceLine, 25.f + 10.f*FMath::FRand());
	}
}

bool AUTPickupInventory::FlashOnMinimap_Implementation()
{
	return (State.bActive && InventoryType && InventoryType.GetDefaultObject()->PickupSpawnAnnouncement);
}

void AUTPickupInventory::SetPickupHidden(bool bNowHidden)
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

bool AUTPickupInventory::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	// TODO: vehicle consideration
	bDefaultAllowPickup = bDefaultAllowPickup && Cast<AUTCharacter>(Other) != NULL && !((AUTCharacter*)Other)->IsRagdoll() && ((AUTCharacter*)Other)->bCanPickupItems && ((InventoryType == nullptr) || InventoryType.GetDefaultObject()->AllowPickupBy(((AUTCharacter*)Other)));
	bool bAllowPickup = bDefaultAllowPickup;
	AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	return (UTGameMode == NULL || !UTGameMode->OverridePickupQuery(Other, InventoryType, this, bAllowPickup)) ? bDefaultAllowPickup : bAllowPickup;
}

void AUTPickupInventory::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL && InventoryType != NULL)
	{
		if (!InventoryType.GetDefaultObject()->HandleGivenTo(P))
		{
			AUTInventory* Existing = P->FindInventoryType(InventoryType, true);
			if (Existing == NULL || !Existing->StackPickup(NULL))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.Instigator = P;
				P->AddInventory(GetWorld()->SpawnActor<AUTInventory>(InventoryType, GetActorLocation(), GetActorRotation(), Params), true);
			}
		}
		P->DeactivateSpawnProtection();
		AnnouncePickup(P);

		//Add to the stats pickup count
		const AUTInventory* Inventory = InventoryType.GetDefaultObject();
		if (Inventory->StatsNameCount != NAME_None)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
			if (PS)
			{
				PS->ModifyStatsValue(Inventory->StatsNameCount, 1);
				if (PS->Team)
				{
					PS->Team->ModifyStatsValue(Inventory->StatsNameCount, 1);
				}

				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != nullptr)
				{
					GS->ModifyStatsValue(Inventory->StatsNameCount, 1);
				}

				//Send the pickup message to the spectators
				AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
				if (UTGameMode != nullptr)
				{
					UTGameMode->BroadcastSpectatorPickup(PS, Inventory->StatsNameCount, Inventory->GetClass());
				}
			}
		}
	}
}

void AUTPickupInventory::PlayTakenEffects(bool bReplicate)
{
	Super::PlayTakenEffects(bReplicate);

	if (GetNetMode() != NM_DedicatedServer && InventoryType != NULL && Mesh != NULL)
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
		if (WS == NULL || WS->EffectIsRelevant(this, Mesh->GetComponentLocation(), true, false, 10000.0f, 1000.0f))
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, InventoryType.GetDefaultObject()->PickupEffect, Mesh->GetComponentLocation(), Mesh->GetComponentRotation());
		}
	}
}

void AUTPickupInventory::AnnouncePickup(AUTCharacter* P)
{
	GetWorldTimerManager().ClearTimer(SpawnVoiceLineTimer);
	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	AUTPlayerController* UTPC = (P != nullptr) ? Cast<AUTPlayerController>(P->GetController()) : nullptr;
	if (UTPC)
	{
		UTPC->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, InventoryType);
		if (GM && GM->bBasicTrainingGame && !GM->bDamageHurtsHealth)
		{
			for (int32 Index = 0; Index < TutorialAnnouncements.Num(); Index++)
			{
				UTPC->PlayTutorialAnnouncement(Index, this);
			}
			for (int32 Index = 0; Index < InventoryType->GetDefaultObject<AUTInventory>()->TutorialAnnouncements.Num(); Index++)
			{
				UTPC->PlayTutorialAnnouncement(Index, InventoryType->GetDefaultObject());
			}
		}
	}
	if (GM && GM->bAllowPickupAnnouncements && InventoryType && (InventoryType.GetDefaultObject()->PickupAnnouncementName != NAME_None))
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
		if (PS)
		{
			PS->AnnounceStatus(InventoryType.GetDefaultObject()->PickupAnnouncementName, 1, true);
		}
	}
}

void AUTPickupInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTPickupInventory, InventoryType, COND_None);
}

void AUTPickupInventory::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	if (!InventoryType || !InventoryType.GetDefaultObject()->PickupSpawnAnnouncement || !State.bActive)
	{
		return;
	}
	AUTPlayerState* ViewerPS = PC ? Cast <AUTPlayerState>(PC->PlayerState) : nullptr;
	AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	if (!ViewerPS || !UTGS || !ViewerPS->Team || !UTPC || UTGS->IsMatchIntermission() || UTGS->HasMatchEnded() || !UTGS->HasMatchStarted() || (UTPC->MyUTHUD == nullptr) || UTPC->MyUTHUD->bShowScores)
	{
		return;
	}
	bool bViewerOnOffense = (UTGS->bRedToCap == (ViewerPS->Team->TeamIndex == 0));
	if (bViewerOnOffense ? !bNotifySpawnForOffense : !bNotifySpawnForDefense)
	{
		return;
	}

	const bool bIsViewTarget = (PC->GetViewTarget() == this);
	FVector WorldPosition = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	float TextXL, YL;
	float Scale = Canvas->ClipX / 1920.f;
	UFont* SmallFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->SmallFont;
	FText RedeemerText = NSLOCTEXT("Redeemer", "Redeemer", " Redeemer");
	Canvas->TextSize(SmallFont, RedeemerText.ToString(), TextXL, YL, Scale, Scale);
	FVector ViewDir = UTPC->GetControlRotation().Vector();
	float Dist = (CameraPosition - GetActorLocation()).Size();
	bool bDrawEdgeArrow = false;
	FVector ScreenPosition = GetAdjustedScreenPosition(Canvas, WorldPosition, CameraPosition, ViewDir, Dist, 20.f, bDrawEdgeArrow);
	float XPos = ScreenPosition.X - 0.5f*TextXL;
	float YPos = ScreenPosition.Y - YL - 16.f*Scale;
	if (XPos < Canvas->ClipX || XPos + TextXL < 0.0f)
	{
		FLinearColor TeamColor = FLinearColor::Yellow;
		float CenterFade = 1.f;
		float PctFromCenter = (ScreenPosition - FVector(0.5f*Canvas->ClipX, 0.5f*Canvas->ClipY, 0.f)).Size() / Canvas->ClipX;
		CenterFade = CenterFade * FMath::Clamp(10.f*PctFromCenter, 0.15f, 1.f);
		TeamColor.A = 0.2f * CenterFade;
		UTexture* BarTexture = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HUDAtlas;

		Canvas->SetLinearDrawColor(TeamColor);
		float Border = 2.f*Scale;
		float Height = 0.75*YL + 0.7f * YL;
		float Width = TextXL + 2.f*Border;
		Canvas->DrawTile(Canvas->DefaultTexture, XPos - Border, YPos - YL - Border, Width, Height + 2.f*Border, 0, 0, 1, 1);

		FLinearColor BeaconTextColor = FLinearColor::White;
		BeaconTextColor.A = 0.6f * CenterFade;
		FUTCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + YPos - 1.2f*YL)), RedeemerText, SmallFont, BeaconTextColor, NULL);
		TextItem.Scale = FVector2D(Scale, Scale);
		TextItem.BlendMode = SE_BLEND_Translucent;
		FLinearColor ShadowColor = FLinearColor::Black;
		ShadowColor.A = BeaconTextColor.A;
		TextItem.EnableShadow(ShadowColor);
		TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
		Canvas->DrawItem(TextItem);

		FFormatNamedArguments Args;
		FText NumberText = FText::AsNumber(int32(0.01f*Dist));
		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		Canvas->TextSize(TinyFont, TEXT("XX meters"), TextXL, YL, Scale, Scale);
		Args.Add("Dist", NumberText);
		FText DistText = NSLOCTEXT("UTRallyPoint", "DistanceText", "{Dist} meters");
		TextItem.Font = TinyFont;
		TextItem.Text = FText::Format(DistText, Args);
		TextItem.Position.X = ScreenPosition.X - 0.5f*TextXL;
		TextItem.Position.Y += 0.9f*YL;
		Canvas->DrawItem(TextItem);
	}
}

FVector AUTPickupInventory::GetAdjustedScreenPosition(UCanvas* Canvas, const FVector& WorldPosition, const FVector& ViewPoint, const FVector& ViewDir, float Dist, float Edge, bool& bDrawEdgeArrow)
{
	FVector Cross = (ViewDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
	FVector DrawScreenPosition;
	float ExtraPadding = 0.065f * Canvas->ClipX;
	DrawScreenPosition = Canvas->Project(WorldPosition);
	FVector FlagDir = WorldPosition - ViewPoint;
	if ((ViewDir | FlagDir) < 0.f)
	{
		bool bWasLeft = bBeaconWasLeft;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bWasLeft ? Edge + ExtraPadding : Canvas->ClipX - Edge - ExtraPadding;
		DrawScreenPosition.Y = 0.5f*Canvas->ClipY;
		DrawScreenPosition.Z = 0.0f;
		return DrawScreenPosition;
	}
	else if ((DrawScreenPosition.X < 0.f) || (DrawScreenPosition.X > Canvas->ClipX))
	{
		bool bLeftOfScreen = (DrawScreenPosition.X < 0.f);
		float OffScreenDistance = bLeftOfScreen ? -1.f*DrawScreenPosition.X : DrawScreenPosition.X - Canvas->ClipX;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bLeftOfScreen ? Edge + ExtraPadding : Canvas->ClipX - Edge - ExtraPadding;
		//Y approaches 0.5*Canvas->ClipY as further off screen
		float MaxOffscreenDistance = Canvas->ClipX;
		DrawScreenPosition.Y = 0.4f*Canvas->ClipY + FMath::Clamp((MaxOffscreenDistance - OffScreenDistance) / MaxOffscreenDistance, 0.f, 1.f) * (DrawScreenPosition.Y - 0.6f*Canvas->ClipY);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, 0.25f*Canvas->ClipY, 0.75f*Canvas->ClipY);
		bBeaconWasLeft = bLeftOfScreen;
	}
	else
	{
		bBeaconWasLeft = false;
		DrawScreenPosition.X = FMath::Clamp(DrawScreenPosition.X, Edge, Canvas->ClipX - Edge);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, Edge, Canvas->ClipY - Edge);
		DrawScreenPosition.Z = 0.0f;
	}
	return DrawScreenPosition;
}