// user-selectable content for a player character (mesh, etc)
// this is not done as UTCharacter subclasses for networking reasons (don't want players to be invisible if there is packet loss, loading issues, etc)
// defined as an Actor for the friendly editor tools but is never spawned directly (similar to UTImpactEffect)
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacterContent.generated.h"

UCLASS(BlueprintType, Abstract, NotPlaceable)
class UNREALTOURNAMENT_API AUTCharacterContent : public AActor
{
	GENERATED_BODY()
public:
	friend class AUTCharacter;

	AUTCharacterContent(const FObjectInitializer& OI)
		: Super(OI)
	{
		RootComponent = OI.CreateDefaultSubobject<USceneComponent>(this, FName(TEXT("DummyRoot"))); // needed so Mesh has RelativeLocation/RelativeRotation in the editor
		Mesh = OI.CreateDefaultSubobject<USkeletalMeshComponent>(this, FName(TEXT("Mesh")));
		Mesh->AttachParent = RootComponent;
		Mesh->AlwaysLoadOnClient = true;
		Mesh->AlwaysLoadOnServer = true;
		Mesh->bCastDynamicShadow = true;
		Mesh->bAffectDynamicIndirectLighting = true;
		Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		Mesh->bChartDistanceFactor = true;
		Mesh->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
		Mesh->bGenerateOverlapEvents = false;
		Mesh->bCanEverAffectNavigation = false;
		Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->bEnablePhysicsOnDedicatedServer = true; // needed for feign death; death ragdoll shouldn't be invoked on server
		Mesh->bReceivesDecals = false;

		DisplayName = NSLOCTEXT("UT", "UntitledCharacter", "Untitled Character");
	}

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, AssetRegistrySearchable)
	FText DisplayName;

	/** character gender */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bIsFemale;

	/** if set hide from the menus (i.e. intended for testing or built-in to a specific gametype or mod) */
	UPROPERTY(EditDefaultsOnly)
	bool bHideInUI;

	/** if set a UTProfileItem is required for this character to be available */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Meta = (DisplayName = "Requires Online Item"))
	bool bRequiresItem;
	/** if set this achievement is required for this character to be available
	 * (note: achievements are currently client side only and not validated by server)
	 */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Meta = (DisplayName = "Required Offline Achievement"))
	FName RequiredAchievement;
	
	USkeletalMeshComponent* GetMesh() { return Mesh; }

	/** mesh to swap in when the character is skeletized */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	USkeletalMesh* SkeletonMesh;

protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	USkeletalMeshComponent* Mesh;

	//FIXME: TEMP FOR GDC: material overrides when playing a team game (NULL for an entry means use default)
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<UMaterialInterface*> TeamMaterials;
};