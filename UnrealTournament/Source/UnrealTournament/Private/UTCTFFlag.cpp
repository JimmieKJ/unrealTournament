// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"

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

	if (Role == ROLE_Authority)
	{
		GetWorldTimerManager().SetTimer(this, &AUTCTFFlag::DefaultTimer, 1.0f, true);
	}
}

void AUTCTFFlag::DefaultTimer()
{
	if (Holder != NULL)
	{
		// Look to see if the other CTF Flag is out.
		
		AUTCTFGameState* CTFGameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (CTFGameState != NULL)
		{
			for (int i = 0; i < CTFGameState->FlagBases.Num(); i++)
			{
				if (CTFGameState->FlagBases[i] != NULL && CTFGameState->FlagBases[i]->GetCarriedObjectHolder() == NULL)
				{
					return;
				}
			}
		}

		AUTCTFGameMode* GM = Cast<AUTCTFGameMode>(GetWorld()->GetAuthGameMode());
		if (GM != NULL)
		{
			GM->ScoreHolder(Holder);
		}
	}
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
				SendGameMessage(2, CarriedFlag->Holder, NULL);
				CarriedFlag->Score( FName(TEXT("FlagCapture")), CarriedFlag->HoldingPawn, CarriedFlag->Holder);		
				return false;
			}
		}
	}

	return Super::CanBePickedUpBy(Character);
}

void AUTCTFFlag::Destroyed()
{
	Super::Destroyed();
}

void AUTCTFFlag::OnHolderChanged()
{
	Super::OnHolderChanged();

	APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
	Mesh->SetHiddenInGame(PC != NULL && Holder != NULL && PC->PlayerState == Holder);
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

void AUTCTFFlag::OnObjectStateChanged()
{
	Super::OnObjectStateChanged();

	if (Role == ROLE_Authority)
	{
		if (ObjectState == CarriedObjectState::Dropped)
		{
			GetWorldTimerManager().SetTimer(this, &AUTCTFFlag::AutoReturn, 30, false);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(this, &AUTCTFFlag::AutoReturn);
		}
	}

}

void AUTCTFFlag::AutoReturn()
{
	SendHome();
}