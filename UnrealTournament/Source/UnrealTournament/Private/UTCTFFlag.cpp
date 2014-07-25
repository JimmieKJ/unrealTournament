// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTCTFGameMessage.h"

AUTCTFFlag::AUTCTFFlag(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FlagMesh (TEXT("SkeletalMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_CTF_Flag_IronGuard.S_CTF_Flag_IronGuard'"));

	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CTFFlag"));
	Mesh->SetSkeletalMesh(FlagMesh.Object);
	Mesh->AlwaysLoadOnClient = true;
	Mesh->AlwaysLoadOnServer = true;
	Mesh->AttachParent = RootComponent;
	Mesh->SetAbsolute(false, false, true);

	MovementComponent->ProjectileGravityScale=3.0;
	MessageClass = UUTCTFGameMessage::StaticClass();
	bAlwaysRelevant = true;

}

void AUTCTFFlag::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// backwards compatibility; force values on existing instances
	Mesh->SetAbsolute(false, false, true);
	Mesh->SetWorldRotation(FRotator(0.0f, 0.f, 0.f));
}



bool AUTCTFFlag::CanBePickedUpBy(AUTCharacter* Character)
{
	if (Character != NULL)
	{
		AUTCTFFlag* CarriedFlag = Cast<AUTCTFFlag>(Character->GetCarriedObject());
		if (CarriedFlag != NULL && CarriedFlag != this && ObjectState == CarriedObjectState::Home)
		{
			if (CarriedFlag->GetTeamNum() != GetTeamNum())
			{
				CarriedFlag->Score(FName(TEXT("FlagCapture")));		
				return false;
			}
		}
	}

	return Super::CanBePickedUpBy(Character);
}

void AUTCTFFlag::Score(FName Reason)
{
	Super::Score(Reason);
	SendHome();
}

void AUTCTFFlag::Destroyed()
{
	Super::Destroyed();
}

void AUTCTFFlag::DetachFrom(USkeletalMeshComponent* AttachToMesh)
{
	Super::DetachFrom(AttachToMesh);
	if (AttachToMesh != NULL && Mesh != NULL)
	{
		Mesh->SetAbsolute(false, false, true);
		Mesh->SetRelativeScale3D(FVector(1.0f,1.0f,1.0f));
		Mesh->SetWorldScale3D(FVector(1.0f,1.0f,1.0f));
	}
}