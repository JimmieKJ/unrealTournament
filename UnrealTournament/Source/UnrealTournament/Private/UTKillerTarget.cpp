// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTKillerTarget.h"
#include "Animation/AnimInstance.h"
#include "UTPlayerController.h"

static FName NAME_ColorParam = FName(TEXT("color"));

AUTKillerTarget::AUTKillerTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//InitialLifeSpan = 2.f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	UCapsuleComponent* CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(ACharacter::CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	RootComponent = CapsuleComponent;

	bHasTicked = false;
};

void AUTKillerTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasTicked)
	{
		bHasTicked = true;
//		Mesh->TickAnimation(0.0f, false);
//		Mesh->RefreshBoneTransforms();
//		Mesh->UpdateComponentToWorld();		
		Mesh->SetMasterPoseComponent(NULL);
		Mesh->bPauseAnims = true;
	}
	if ((Watcher == nullptr) || Watcher->IsPendingKillPending() || Watcher->GetPawn())
	{
		Destroy();
	}
}

void AUTKillerTarget::InitFor(AUTCharacter* KillerPawn, AUTPlayerController* InWatcher)
{
	Watcher = InWatcher;
	if (Mesh == NULL)
	{
		Mesh = DuplicateObject<USkeletalMeshComponent>(KillerPawn->GetMesh(), this);
		Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // AttachParent gets copied but we don't want it to be

		{
			// TODO: scary that these get copied, need an engine solution and/or safe way to duplicate objects during gameplay
			//Mesh->PrimaryComponentTick = Mesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PrimaryComponentTick;
			//Mesh->PostPhysicsComponentTick = Mesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PostPhysicsComponentTick;
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // make sure because could be in ragdoll
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCastShadow(false);
		Mesh->SetMasterPoseComponent(KillerPawn->GetMesh());
	}
	if (Mesh && !Mesh->IsRegistered())
	{
		Mesh->RegisterComponent();
		Mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Mesh->LastRenderTime = KillerPawn->GetMesh()->LastRenderTime;
		Mesh->SetRelativeLocation(KillerPawn->GetMesh()->RelativeLocation);
		Mesh->SetRelativeRotation(KillerPawn->GetMesh()->RelativeRotation);
		Mesh->SetRelativeScale3D(KillerPawn->GetMesh()->RelativeScale3D);
		Mesh->SetAnimInstanceClass(KillerPawn->GetMesh()->AnimClass);
		Mesh->BoundsScale = 15000.f;
		Mesh->InvalidateCachedBounds();
		Mesh->UpdateBounds();
		Mesh->bVisible = true;
		Mesh->bHiddenInGame = false;
/*
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GhostMaterial, Mesh);
		MID->SetVectorParameterValue(NAME_ColorParam, FLinearColor::Red);
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, MID);
		}*/
	}

/*
	FOverlayEffect FirstOverlay = GS->GetFirstOverlay(CharOverlayFlags, false);
	// note: MID doesn't have any safe way to change Parent at runtime, so we need to make a new one every time...
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FirstOverlay.Material, OverlayMesh);
	// apply team color, if applicable
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->Team != NULL)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		MID->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
	}
	for (int32 i = 0; i < OverlayMesh->GetNumMaterials(); i++)
	{
		OverlayMesh->SetMaterial(i, MID);
	}*/
}