// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTLastSecondMessage.h"

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

	MovementComponent->ProjectileGravityScale=1.3f;
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
				CarriedFlag->Score(FName(TEXT("FlagCapture")), CarriedFlag->HoldingPawn, CarriedFlag->Holder);
				CarriedFlag->Mesh->SetRelativeScale3D(FVector(1.5f,1.5f,1.5f));
				CarriedFlag->Mesh->SetWorldScale3D(FVector(1.5f,1.5f,1.5f));
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
			GetWorldTimerManager().SetTimer(this, &AUTCTFFlag::SendHomeWithNotify, 30, false);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(this, &AUTCTFFlag::SendHomeWithNotify);
		}
	}
}

void AUTCTFFlag::SendHomeWithNotify()
{
	SendGameMessage(1, NULL, NULL);
	SendHome();
}

void AUTCTFFlag::SendHome()
{
	Mesh->SetRelativeScale3D(FVector(1.5f,1.5f,1.5f));
	Mesh->SetWorldScale3D(FVector(1.5f,1.5f,1.5f));
	Super::SendHome();
}

void AUTCTFFlag::Drop(AController* Killer)
{
	bool bDelayDroppedMessage = false;
	AUTPlayerState* KillerState = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	if (KillerState && KillerState->Team && (KillerState != Holder))
	{
		// see if this is a last second save
		AUTCTFGameState* GameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (GameState)
		{
			AUTCTFFlagBase* KillerBase = GameState->FlagBases[1-GetTeamNum()];
			AActor* Considered = LastHoldingPawn;
			if (!Considered)
			{
				Considered = this;
			}

			if (KillerBase && (KillerBase->GetFlagState() == CarriedObjectState::Home) && KillerBase->ActorIsNearMe(Considered))
			{
				AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
				if (GM)
				{
					bDelayDroppedMessage = true;
					GM->BroadcastLocalized(this, UUTLastSecondMessage::StaticClass(), 0, Killer->PlayerState, Holder, NULL);
				}
			}
		}
	}

	FlagDropTime = GetWorld()->GetTimeSeconds();
	if (bDelayDroppedMessage)
	{
		GetWorldTimerManager().SetTimer(this, &AUTCTFFlag::DelayedDropMessage, 0.8f, false);
	}
	else
	{
		SendGameMessage(3, Holder, NULL);
	}
	NoLongerHeld();

	// Toss is out
	TossObject(LastHoldingPawn);

	if (HomeBase != NULL)
	{
		HomeBase->ObjectWasDropped(LastHoldingPawn);
	}
	ChangeState(CarriedObjectState::Dropped);
}

void AUTCTFFlag::DelayedDropMessage()
{
	if ((LastGameMessageTime < FlagDropTime) && (ObjectState == CarriedObjectState::Dropped))
	{
		SendGameMessage(3, Holder, NULL);
	}
}