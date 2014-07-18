// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

AUTCarriedObject::AUTCarriedObject(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Collision = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->InitCapsuleSize(64.0f, 30.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTCarriedObject::OnOverlapBegin);
	Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	RootComponent = Collision;

	MovementComponent = PCIP.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("MovementComp"));
	MovementComponent->MaxSpeed = 420;
	MovementComponent->InitialSpeed = 360;
	MovementComponent->bShouldBounce = true;
	MovementComponent->SetIsReplicated(true);

	SetReplicates(true);
	bReplicateMovement = true;
	NetPriority=3.0;
}

void AUTCarriedObject::Init(AUTGameObjective* NewBase)
{
	// Look up the team for my CarriedObject
	if (DesiredTeamNum < 255)
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		if (GameState != NULL && DesiredTeamNum < GameState->Teams.Num())
		{
			Team = GameState->Teams[DesiredTeamNum];
		}
	}

	HomeBase = NewBase;
	ObjectState = CarriedObjectState::Home;
	MoveToHome();
	OnHolderChanged();
}

void AUTCarriedObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCarriedObject, ObjectState);
	DOREPLIFETIME(AUTCarriedObject, Holder);
	DOREPLIFETIME(AUTCarriedObject, Team);
}


void AUTCarriedObject::AttachTo(USkeletalMeshComponent* AttachToMesh)
{
	if (AttachToMesh != NULL && Collision != NULL)
	{
		Collision->SetRelativeLocation(Holder3PTransform);
		Collision->SetRelativeRotation(Holder3PRotation);
		AttachRootComponentTo(AttachToMesh, Holder3PSocketName);
	}
}

void AUTCarriedObject::DetachFrom(USkeletalMeshComponent* AttachToMesh)
{
	if (AttachToMesh != NULL && Collision != NULL)
	{
		Collision->SetRelativeLocation(FVector(0,0,0));
		Collision->SetRelativeRotation(FRotator(0,0,0));
		DetachRootComponentFromParent(true);
	}
}


void AUTCarriedObject::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
	if (Character != NULL)
	{
		TryPickup(Character);
	}
}

void AUTCarriedObject::TryPickup_Implementation(AUTCharacter* Character)
{
	if (Role == ROLE_Authority && Character->Controller != NULL && ObjectState != CarriedObjectState::Held)
	{
		// A valid character wants to pick this up.  First ask if they can.  

		if ( CanBePickedUpBy(Character) && Character->CanPickupObject(this) )
		{
			SetHolder(Character);
		}
	}
}


void AUTCarriedObject::OnObjectStateChanged()
{
	// By default we don't do anything.
}

void AUTCarriedObject::OnHolderChanged()
{
	if (Holder != NULL)
	{
		// Being held, so disable collision
		Collision->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	
	}
	else
	{
		// We don't have a holder, so turn on collision
		Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	}
}

uint8 AUTCarriedObject::GetTeamNum() const
{
	return (Team != NULL) ? Team->GetTeamNum() : 255;
}

void AUTCarriedObject::ChangeState(FName NewCarriedObjectState)
{
	if (Role == ROLE_Authority)
	{
		ObjectState = NewCarriedObjectState;
		OnObjectStateChanged();
	}
}

bool AUTCarriedObject::CanBePickedUpBy(AUTCharacter* Character)
{
	return Team == NULL || bAnyoneCanPickup || Team->GetTeamNum() == GetTeamNum();
}

void AUTCarriedObject::PickupDenied(AUTCharacter* Character)
{
	// By default do nothing.
}

void AUTCarriedObject::SetHolder(AUTCharacter* NewHolder)
{
	// Sanity Checks
	if (NewHolder == NULL || NewHolder->bPendingKillPending || NewHolder->PlayerState == NULL || Cast<AUTPlayerState>(NewHolder->PlayerState) == NULL) return; 

	// If this is the NewHolder's objective and bTeamPickupSendsHome is set, then send this home.
	if (GetTeamNum() == NewHolder->GetTeamNum() && bTeamPickupSendsHome)
	{
		SendHome();
		return;
	}
	
	// If this object is on it's base, tell the base it's been picked up

	if (ObjectState == CarriedObjectState::Home)
	{
		HomeBase->ObjectWasPickedUp(NewHolder);
	}

	HoldingPawn = NewHolder;
	AttachTo(HoldingPawn->Mesh);
	Holder = Cast<AUTPlayerState>(HoldingPawn->PlayerState);

	// If we have authority, immediately notify of the holder change, otherwise replication will handle it
	if (Role == ROLE_Authority)
	{
		OnHolderChanged();
	}

	// Track the pawns that have held this.  It's to be used for scoring
	Holders.AddUnique(Holder);

	// Remove any spawn protection the pawn might have
	if (HoldingPawn->bSpawnProtectionEligible)
	{
		HoldingPawn->DeactivateSpawnProtection();
	}


	Holder->SetCarriedObject(this);
	HoldingPawn->MakeNoise(2.0);
	ChangeState(CarriedObjectState::Held);
}

void AUTCarriedObject::NoLongerHeld()
{
	// Have the holding pawn drop the object
	if (HoldingPawn != NULL)
	{
		DetachFrom(HoldingPawn->Mesh);
	}

	if (Holder != NULL)
	{
		Holder->ClearCarriedObject(this);
	}

	LastHoldingPawn = HoldingPawn;

	HoldingPawn = NULL;
	Holder = NULL;

	// If we have authority, immediately notify of the holder change, otherwise replication will handle it
	if (Role == ROLE_Authority)
	{
		OnHolderChanged();
	}

	// Throw the object.
	if (LastHoldingPawn != NULL)
	{
		FRotator Dir = LastHoldingPawn->Health > 0 ? LastHoldingPawn->GetActorRotation() : FRotator(0, 0, 0);
		SetActorLocationAndRotation(LastHoldingPawn->GetActorLocation(), Dir);
		MovementComponent->Velocity = (0.5f * LastHoldingPawn->GetMovementComponent()->Velocity) + (654.0f * Dir.Vector()) + 218.0f * (0.5f + FMath::FRand()) + FMath::VRand();
		MovementComponent->SetUpdatedComponent(Collision);
	}

}

void AUTCarriedObject::Drop(AController* Killer)
{
	NoLongerHeld();
	ChangeState(CarriedObjectState::Dropped);
}

void AUTCarriedObject::Use()
{
	// Use should be managed in child classes.
}


void AUTCarriedObject::SendHome()
{
	if (ObjectState == CarriedObjectState::Home) return;	// Don't both if we are already home

	NoLongerHeld();
	ChangeState(CarriedObjectState::Home);
	HomeBase->ObjectReturnedHome(LastHoldingPawn);
	MoveToHome();
}

void AUTCarriedObject::MoveToHome()
{
	if (HomeBase != NULL)
	{
		FVector BaseLocation = HomeBase->GetActorLocation() + HomeBase->GetActorRotation().RotateVector(HomeBaseOffset) + (FVector(0,0,1) * Collision->GetScaledCapsuleHalfHeight());
		MovementComponent->StopMovementImmediately();
		MovementComponent->SetUpdatedComponent(NULL);
		SetActorLocationAndRotation(BaseLocation, HomeBase->GetActorRotation());
	}
}

void AUTCarriedObject::Score(FName Reason)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL)
	{
		Game->ScoreObject(this, HoldingPawn, Holder, Reason);
	}
}

void AUTCarriedObject::SetTeam(AUTTeamInfo* NewTeam)
{
	Team = NewTeam;
	ForceNetUpdate();
}
