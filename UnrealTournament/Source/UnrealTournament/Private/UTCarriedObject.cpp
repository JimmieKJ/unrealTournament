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
	Collision->SetAbsolute(false, false, true);
	RootComponent = Collision;

	MovementComponent = PCIP.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("MovementComp"));
	MovementComponent->MaxSpeed = 420;
	MovementComponent->InitialSpeed = 360;
	MovementComponent->SetIsReplicated(true);

	SetReplicates(true);
	bReplicateMovement = true;
	NetPriority=3.0;
}

void AUTCarriedObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// backwards compatibility; force values on existing instances
	Collision->SetAbsolute(false, false, true);
	Collision->SetWorldRotation(FRotator(0.0f, 0.f, 0.f));
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
		Collision->SetRelativeScale3D(FVector(1.0f,1.0f,1.0f));
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
	// Look to see if the 

	if (ObjectState == CarriedObjectState::Held)
	{
		// Turn off movement on this actor

		MovementComponent->StopMovementImmediately();
		MovementComponent->SetUpdatedComponent(NULL);
	}
	else 
	{
		MovementComponent->SetUpdatedComponent(Collision);
	}
}

void AUTCarriedObject::OnHolderChanged()
{
	if (Holder != NULL)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	}
	else
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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
	// If this is the NewHolder's objective and bTeamPickupSendsHome is set, then send this home.
	if (GetTeamNum() == Character->GetTeamNum() && bTeamPickupSendsHome)
	{
		if (ObjectState == CarriedObjectState::Dropped)
		{
			SendGameMessage(0, Character->PlayerState, NULL);
			SendHome();
		}
		return false;
	}

	return Team == NULL || bAnyoneCanPickup || Team->GetTeamNum() == GetTeamNum();
}

void AUTCarriedObject::PickupDenied(AUTCharacter* Character)
{
	// By default do nothing.
}

void AUTCarriedObject::SendGameMessage(uint32 Switch, APlayerState* PS1, APlayerState* PS2, UObject* OptionalObject)
{
	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GM != NULL && MessageClass != NULL)
	{
		GM->BroadcastLocalized(this, MessageClass, Switch, PS1, PS2, Team);
	}
}

void AUTCarriedObject::SetHolder(AUTCharacter* NewHolder)
{
	// Sanity Checks
	if (NewHolder == NULL || NewHolder->bPendingKillPending || NewHolder->PlayerState == NULL || Cast<AUTPlayerState>(NewHolder->PlayerState) == NULL) return; 
	
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

	SendGameMessage(4, Holder, NULL);
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
}

void AUTCarriedObject::TossObject(AUTCharacter* ObjectHolder)
{
	// Throw the object.
	if (ObjectHolder != NULL)
	{
		FRotator Dir = ObjectHolder->Health > 0 ? ObjectHolder->GetActorRotation() : FRotator(0, 0, 0);
		SetActorLocationAndRotation(ObjectHolder->GetActorLocation(), Dir);
		MovementComponent->Velocity = (0.5f * ObjectHolder->GetMovementComponent()->Velocity) + (900.0f * Dir.Vector()) + 218.0f * (0.5f + FMath::FRand()) + FMath::VRand();
	}
	
}

void AUTCarriedObject::Drop(AController* Killer)
{
	SendGameMessage(3, Holder, NULL);
	NoLongerHeld();
	// Toss is out
	TossObject(LastHoldingPawn);
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

		MovementComponent->Velocity = FVector(0.0f,0.0f,0.0f);
		SetActorLocationAndRotation(BaseLocation, HomeBase->GetActorRotation());
		ForceNetUpdate();
	}
}

void AUTCarriedObject::Score(FName Reason)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL)
	{
		SendGameMessage(2, Holder, NULL);
		Game->ScoreObject(this, HoldingPawn, Holder, Reason);
	}
}

void AUTCarriedObject::SetTeam(AUTTeamInfo* NewTeam)
{
	Team = NewTeam;
	ForceNetUpdate();
}

void AUTCarriedObject::FellOutOfWorld(const UDamageType& dmgType)
{
	if (Role == ROLE_Authority)
	{
		SendHome();
	}
	else
	{
		// Force it back to the last replicated location
		SetActorLocation(ReplicatedMovement.Location);
	}
}

// workaround for bug in AActor implementation
void AUTCarriedObject::OnRep_AttachmentReplication()
{
	if (AttachmentReplication.AttachParent)
	{
		if (RootComponent)
		{
			USceneComponent* ParentComponent = AttachmentReplication.AttachParent->GetRootComponent();

			if (AttachmentReplication.AttachComponent != NULL)
			{
				ParentComponent = AttachmentReplication.AttachComponent;
			}

			if (ParentComponent)
			{
				// Calculate scale before attachment as ComponentToWorld will be modified after AttachTo()
				FVector NewRelativeScale3D = RootComponent->RelativeScale3D;
				if (!RootComponent->bAbsoluteScale)
				{
					FTransform ParentToWorld = ParentComponent->GetSocketTransform(AttachmentReplication.AttachSocket);
					FTransform RelativeTM = RootComponent->ComponentToWorld.GetRelativeTransform(ParentToWorld);
					NewRelativeScale3D = RelativeTM.GetScale3D();
				}

				RootComponent->AttachTo(ParentComponent, AttachmentReplication.AttachSocket);
				RootComponent->RelativeLocation = AttachmentReplication.LocationOffset;
				RootComponent->RelativeRotation = AttachmentReplication.RotationOffset;
				RootComponent->RelativeScale3D = NewRelativeScale3D;

				RootComponent->UpdateComponentToWorld();
			}
		}
	}
	else
	{
		DetachRootComponentFromParent();
	}
}

// HACK: workaround for engine bug with transform replication when attaching/detaching things
void AUTCarriedObject::OnRep_ReplicatedMovement()
{
	// ignore the redundant ReplicatedMovement we replicated below
	if (AttachmentReplication.AttachParent == NULL)
	{
		Super::OnRep_ReplicatedMovement();
	}
}
void AUTCarriedObject::GatherCurrentMovement()
{
	Super::GatherCurrentMovement();
	// force ReplicatedMovement to be replicated even when attached, which is a hack to force the last replicated data to be different when the flag is eventually returned
	// otherwise an untouched flag capture results in replication fail because this value has never changed in between times it was replicated (only attachment was)
	// leaving the flag floating at the enemy flag base on clients
	if (RootComponent != NULL && RootComponent->AttachParent != NULL)
	{
		ReplicatedMovement.Location = RootComponent->GetComponentLocation();
		ReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
		ReplicatedMovement.bRepPhysics = false;
	}
}