// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UnrealNetwork.h"
#include "UTPickupMessage.h"

AUTPickupInventory::AUTPickupInventory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	FloatHeight = 50.0f;
	bAllowRotatingPickup = true;
	bHasTacComView = true;
}

void AUTPickupInventory::BeginPlay()
{
	AActor::BeginPlay(); // skip UTPickup as SetInventoryType() will handle delayed spawn

	if (BaseEffect != NULL && BaseTemplateAvailable != NULL)
	{
		BaseEffect->SetTemplate(BaseTemplateAvailable);
	}

	SetInventoryType(InventoryType);

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
		RespawnTime = InventoryType.GetDefaultObject()->RespawnTime;
		bDelayedSpawn = InventoryType.GetDefaultObject()->bDelayedSpawn;
	}
	else
	{
		RespawnTime = 0.0f;
	}
	InventoryTypeUpdated();
	if (Role == ROLE_Authority && GetWorld()->GetAuthGameMode<AUTGameMode>()->HasMatchStarted())
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
static void CreatePickupMeshAttachments(AActor* Pickup, USceneComponent* CurrentAttachment, FName TemplateName, const TArray<USceneComponent*>& NativeCompList, const TArray<USCS_Node*>& BPNodes)
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
			}
			NewComp->RegisterComponent();
			NewComp->AttachTo(CurrentAttachment, NewComp->AttachSocketName);
			// recurse
			CreatePickupMeshAttachments(Pickup, NewComp, NativeCompList[i]->GetFName(), NativeCompList, BPNodes);
		}
	}
	for (int32 i = 0; i < BPNodes.Num(); i++)
	{
		if (BPNodes[i]->ComponentTemplate != NULL && BPNodes[i]->ParentComponentOrVariableName == TemplateName)
		{
			USceneComponent* NewComp = NewObject<USceneComponent>(Pickup, BPNodes[i]->ComponentTemplate->GetClass(), NAME_None, RF_NoFlags, BPNodes[i]->ComponentTemplate);
			NewComp->AttachParent = NULL;
			NewComp->AttachChildren.Empty();
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(NewComp);
			if (Prim != NULL)
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			NewComp->RegisterComponent();
			NewComp->AttachTo(CurrentAttachment, BPNodes[i]->AttachToName);
			// recurse
			CreatePickupMeshAttachments(Pickup, NewComp, BPNodes[i]->VariableName, NativeCompList, BPNodes);
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
				CreatePickupMeshAttachments(Pickup, PickupMesh, NewMesh->GetFName(), NativeCompList, ConstructionNodes);
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
	if (Mesh != NULL && !State.bActive && Role < ROLE_Authority)
	{
		// if State was received first whole actor might have been hidden, reset everything
		StartSleeping();
		OnRep_RespawnTimeRemaining();
	}

	TakenSound = (InventoryType != NULL) ? TakenSound = InventoryType.GetDefaultObject()->PickupSound : GetClass()->GetDefaultObject<AUTPickupInventory>()->TakenSound;
}

void AUTPickupInventory::Reset_Implementation()
{
	if (InventoryType == NULL)
	{
		StartSleeping();
	}
	else
	{
		Super::Reset_Implementation();
	}
}

void AUTPickupInventory::SetPickupHidden(bool bNowHidden)
{
	if (Mesh != NULL)
	{
		Mesh->SetHiddenInGame(bNowHidden, true);
		Mesh->SetVisibility(!bNowHidden, true);
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

bool AUTPickupInventory::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	// TODO: vehicle consideration
	bDefaultAllowPickup = bDefaultAllowPickup && Cast<AUTCharacter>(Other) != NULL && !((AUTCharacter*)Other)->IsRagdoll();
	bool bAllowPickup = bDefaultAllowPickup;
	AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	return (UTGameMode == NULL || !UTGameMode->OverridePickupQuery(Other, InventoryType, this, bAllowPickup)) ? bDefaultAllowPickup : bAllowPickup;
}

void AUTPickupInventory::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL && InventoryType != NULL)
	{
		AUTInventory* Existing = P->FindInventoryType(InventoryType, true);
		if (Existing == NULL || !Existing->StackPickup(NULL))
		{
			FActorSpawnParameters Params;
			Params.bNoCollisionFail = true;
			Params.Instigator = P;
			P->AddInventory(GetWorld()->SpawnActor<AUTInventory>(InventoryType, GetActorLocation(), GetActorRotation(), Params), true);
			AnnouncePickup(P);
		}
	}
}

void AUTPickupInventory::AnnouncePickup(AUTCharacter* P)
{
	if (Cast<APlayerController>(P->GetController()))
	{
		Cast<APlayerController>(P->GetController())->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, InventoryType);
	}
}

void AUTPickupInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTPickupInventory, InventoryType, COND_None);
}