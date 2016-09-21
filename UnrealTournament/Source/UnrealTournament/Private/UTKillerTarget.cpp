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
//		Mesh->bPauseAnims = true;
		Mesh->SetMasterPoseComponent(NULL);
	}
	if ((Watcher == nullptr) || Watcher->IsPendingKillPending() || Watcher->GetPawn() || Watcher->IsInState(NAME_Spectating))
	{
		Destroy();
		return;
	}
	AUTGameState* GS = Watcher->GetWorld()->GetGameState<AUTGameState>();
	if (GS)
	{
		if (GS->IsMatchIntermission() || GS->HasMatchEnded())
		{
			Destroy();
		}
	}
}

void AUTKillerTarget::InitFor(AUTCharacter* KillerPawn, AUTPlayerController* InWatcher)
{
	if (!KillerPawn || !KillerPawn->GetMesh())
	{
		return;
	}
	Watcher = InWatcher;
	if (Mesh == NULL)
	{
		Mesh = DuplicateObject<USkeletalMeshComponent>(KillerPawn->GetMesh(), this);
		Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // AttachParent gets copied but we don't want it to be
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // make sure because could be in ragdoll
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCastShadow(false);
		Mesh->SetSkeletalMesh(KillerPawn->GetMesh()->SkeletalMesh);
		Mesh->SetMasterPoseComponent(KillerPawn->GetMesh());
	}
	if (Mesh && !Mesh->IsRegistered())
	{
		Mesh->RegisterComponent();
		Mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Mesh->LastRenderTime = KillerPawn->GetMesh()->LastRenderTime;
		Mesh->bVisible = true;
		Mesh->bHiddenInGame = false;
		Mesh->SetRelativeLocation(KillerPawn->GetMesh()->RelativeLocation);
		Mesh->SetRelativeRotation(KillerPawn->GetMesh()->RelativeRotation);
		Mesh->SetRelativeScale3D(KillerPawn->GetMesh()->RelativeScale3D);

		if (KillerPawn->GhostMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(KillerPawn->GhostMaterial, Mesh);
			FLinearColor GhostColor = FLinearColor(0.5f, 0.15f, 0.15f, 0.2f);
			MID->SetVectorParameterValue(NAME_ColorParam, GhostColor);
			for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
			{
				Mesh->SetMaterial(i, MID);
			}
		}
	}
}

