// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UnrealNetwork.h"
#include "UTPickupMessage.h"
#include "UTWorldSettings.h"
#include "UTCTFGameState.h"

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
		RespawnTime = Game ? Game->OverrideRespawnTime(InventoryType) : InventoryType.GetDefaultObject()->RespawnTime;
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
		if (NativeCompList[i]->AttachParent == CurrentAttachment)
		{
			USceneComponent* NewComp = NewObject<USceneComponent>(Pickup, NativeCompList[i]->GetClass(), NAME_None, RF_NoFlags, NativeCompList[i]);
			NewComp->AttachParent = NULL;
			NewComp->AttachChildren.Empty();
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(NewComp);
			if (Prim != NULL)
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prim->bShouldUpdatePhysicsVolume = false;
			}
			NewComp->RegisterComponent();
			NewComp->AttachTo(CurrentAttachment, NewComp->AttachSocketName);
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
			NewComp->AttachParent = NULL;
			NewComp->AttachChildren.Empty();
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(NewComp);
			if (Prim != NULL)
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prim->bShouldUpdatePhysicsVolume = false;
			}
			NewComp->bAutoRegister = false;
			NewComp->RegisterComponent();
			NewComp->AttachTo(CurrentAttachment, BPNodes[i]->AttachToName);
			// recurse
			CreatePickupMeshAttachments(Pickup, PickupInventoryType, NewComp, BPNodes[i]->VariableName, NativeCompList, BPNodes);
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
				(StaticTemplate != NULL && ((UStaticMeshComponent*)PickupMesh)->StaticMesh != StaticTemplate->StaticMesh))
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
				PickupMesh->AttachParent = NULL;
				PickupMesh->AttachChildren.Empty();
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
				PickupMesh->AttachTo(Pickup->GetRootComponent());
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
			else if (PickupMesh->AttachParent != Pickup->GetRootComponent())
			{
				PickupMesh->AttachTo(Pickup->GetRootComponent(), NAME_None, EAttachLocation::SnapToTarget);
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
			GhostMesh->AttachParent = NULL;
			GhostMesh->AttachChildren.Empty();
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
			GhostMesh->AttachTo(Mesh, NAME_None, EAttachLocation::SnapToTargetIncludingScale);
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
			}
			else if (InventoryType.GetDefaultObject()->PickupAnnouncementName != NAME_None)
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
		AUTCTFGameState* CTFGameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (CTFGameState)
		{
			bHasPlayedForRed = CTFGameState->bRedToCap ? !bNotifySpawnForOffense : !bNotifySpawnForDefense;
			bHasPlayedForBlue = CTFGameState->bRedToCap ? !bNotifySpawnForDefense : !bNotifySpawnForOffense;
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
				for (USceneComponent* Child : Mesh->AttachChildren)
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
			TArray<USceneComponent*> Children;
			Mesh->GetChildrenComponents(true, Children);
			for (int32 i = 0; i < Children.Num(); i++)
			{
				UAudioComponent* AC = Cast<UAudioComponent>(Children[i]);
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

				AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
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
	if (Cast<APlayerController>(P->GetController()))
	{
		Cast<APlayerController>(P->GetController())->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, InventoryType);
	}
	if (InventoryType && (InventoryType.GetDefaultObject()->PickupAnnouncementName != NAME_None))
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