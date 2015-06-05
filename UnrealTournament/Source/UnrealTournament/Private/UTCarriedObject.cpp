// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTProjectileMovementComponent.h"
#include "UTCTFScoring.h"
#include "Net/UnrealNetwork.h"

AUTCarriedObject::AUTCarriedObject(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Collision = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->InitCapsuleSize(72.0f, 30.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTCarriedObject::OnOverlapBegin);
	Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	Collision->SetAbsolute(false, false, true);
	RootComponent = Collision;

	MovementComponent = ObjectInitializer.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("MovementComp"));
	MovementComponent->HitZStopSimulatingThreshold = 0.7f;
	MovementComponent->MaxSpeed = 5000.0f; // needed for gravity
	MovementComponent->InitialSpeed = 360.0f;
	MovementComponent->SetIsReplicated(true);

	SetReplicates(true);
	bReplicateMovement = true;
	NetPriority=3.0;
	LastGameMessageTime = 0.f;
	AutoReturnTime = 30.0f;
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
	uint8 DesiredTeamNum = NewBase->GetTeamNum();
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
	HomeBase->ObjectStateWasChanged(ObjectState);
	MoveToHome();
	OnHolderChanged();
}

bool AUTCarriedObject::IsHome()
{
	return ObjectState == CarriedObjectState::Home;
}

void AUTCarriedObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCarriedObject, ObjectState);
	DOREPLIFETIME(AUTCarriedObject, Holder);
	DOREPLIFETIME(AUTCarriedObject, Team);
	DOREPLIFETIME_CONDITION(AUTCarriedObject, HomeBase, COND_InitialOnly);
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
	if (Character != NULL && !GetWorld()->LineTraceTestByChannel(OtherActor->GetActorLocation(), GetActorLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
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

	OnCarriedObjectStateChangedDelegate.Broadcast(this, ObjectState);
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

	OnCarriedObjectHolderChangedDelegate.Broadcast(this);
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
		HomeBase->ObjectStateWasChanged(ObjectState);
	}
}

bool AUTCarriedObject::CanBePickedUpBy(AUTCharacter* Character)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL && (!GS->IsMatchInProgress() || GS->IsMatchAtHalftime()))
	{
		return false;
	}
	else if (Character->IsRagdoll())
	{
		return false;
	}
	// If this is the NewHolder's objective and bTeamPickupSendsHome is set, then send this home.
	else if (GetTeamNum() == Character->GetTeamNum() && bTeamPickupSendsHome)
	{
		if (ObjectState == CarriedObjectState::Dropped)
		{
			SendGameMessage(0, Character->PlayerState, NULL);
			Score(FName("SentHome"), Character, Cast<AUTPlayerState>(Character->PlayerState));
		}
		return false;
	}
	else
	{
		return Team == NULL || bAnyoneCanPickup || Team->GetTeamNum() == GetTeamNum();
	}
}

void AUTCarriedObject::PickupDenied(AUTCharacter* Character)
{
	// By default do nothing.
}

void AUTCarriedObject::SendGameMessage(uint32 Switch, APlayerState* PS1, APlayerState* PS2, UObject* OptionalObject)
{
	LastGameMessageTime = GetWorld()->GetTimeSeconds();
	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GM != NULL && MessageClass != NULL)
	{
		GM->BroadcastLocalized(this, MessageClass, Switch, PS1, PS2, Team);
	}
}

void AUTCarriedObject::SetHolder(AUTCharacter* NewHolder)
{
	// Sanity Checks
	if (NewHolder == NULL || NewHolder->bPendingKillPending || Cast<AUTPlayerState>(NewHolder->PlayerState) == NULL)
	{
		return;
	}

	bool bWasHome = (ObjectState == CarriedObjectState::Home);
	ChangeState(CarriedObjectState::Held);

	// Tell the base it's been picked up
	HomeBase->ObjectWasPickedUp(NewHolder, bWasHome);

	HoldingPawn = NewHolder;
	AttachTo(HoldingPawn->GetMesh());
	Holder = Cast<AUTPlayerState>(HoldingPawn->PlayerState);
	PickedUpTime = GetWorld()->GetTimeSeconds();

	// If we have authority, immediately notify of the holder change, otherwise replication will handle it
	if (Role == ROLE_Authority)
	{
		OnHolderChanged();
		if (Holder && bWasHome)
		{
			Holder->ModifyStatsValue(NAME_FlagGrabs, 1);
			if (Holder->Team)
			{
				Holder->Team->ModifyStatsValue(NAME_TeamFlagGrabs, 1);
			}
		}
	}

	// Track the pawns that have held this flag - used for scoring
	int32 AssistIndex = FindAssist(Holder);
	if (AssistIndex < 0)
	{
		FAssistTracker NewAssist;
		NewAssist.Holder = Holder;
		NewAssist.TotalHeldTime = 0.0f;
		AssistIndex = AssistTracking.Add(NewAssist);
	}

	// Remove any spawn protection the pawn might have
	if (HoldingPawn->bSpawnProtectionEligible)
	{
		HoldingPawn->DeactivateSpawnProtection();
	}

	Holder->SetCarriedObject(this);
	UUTGameplayStatics::UTPlaySound(GetWorld(), PickupSound, HoldingPawn);

	SendGameMessage(4, Holder, NULL);

	if (Role == ROLE_Authority)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			for (AUTTeamInfo* TeamIter : GS->Teams)
			{
				TeamIter->NotifyObjectiveEvent(HomeBase, NewHolder->Controller, FName(TEXT("FlagStatusChange")));
			}
		}
	}
}

void AUTCarriedObject::NoLongerHeld(AController* InstigatedBy)
{
	// Have the holding pawn drop the object
	if (HoldingPawn != NULL)
	{
		DetachFrom(HoldingPawn->GetMesh());
	}
	LastHolder = Holder;
	if (Holder != NULL)
	{
		Holder->ClearCarriedObject(this);
	}

	LastHoldingPawn = HoldingPawn;
	int32 AssistIndex = FindAssist(Holder);
	float LastHeldTime = GetWorld()->GetTimeSeconds() - PickedUpTime;
	if (AssistIndex >= 0 && AssistIndex < AssistTracking.Num())
	{
		AssistTracking[AssistIndex].TotalHeldTime += LastHeldTime;
	}

	HoldingPawn = NULL;
	Holder = NULL;

	// If we have authority, immediately notify of the holder change, otherwise replication will handle it
	if (Role == ROLE_Authority)
	{
		OnHolderChanged();

		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			for (AUTTeamInfo* TeamIter : GS->Teams)
			{
				TeamIter->NotifyObjectiveEvent(HomeBase, InstigatedBy, FName(TEXT("FlagStatusChange")));
			}
		}
	}
}

void AUTCarriedObject::TossObject(AUTCharacter* ObjectHolder)
{
	// Throw the object.
	if (ObjectHolder != NULL)
	{
		FVector Extra = FVector::ZeroVector;
		if (ObjectHolder->Health > 0)
		{
			Extra = ObjectHolder->GetActorRotation().Vector() * 900.0f;
		}
		FVector Loc = ObjectHolder->GetActorLocation();
		Loc.Z -= ObjectHolder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		Loc.Z += Collision->GetUnscaledCapsuleHalfHeight() * 1.1f;
		SetActorLocationAndRotation(Loc, Extra.IsZero() ? FRotator::ZeroRotator : Extra.Rotation());
		MovementComponent->Velocity = (0.5f * ObjectHolder->GetMovementComponent()->Velocity) + Extra + (218.0f * (0.5f + FMath::FRand()) * FMath::VRand() + FVector(0.0f, 0.0f, 100.0f));
	}
	
}

void AUTCarriedObject::Drop(AController* Killer)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), DropSound, (HoldingPawn != NULL) ? (AActor*)HoldingPawn : (AActor*)this);

	SendGameMessage(3, Holder, NULL);
	NoLongerHeld(Killer);

	// Toss is out
	TossObject(LastHoldingPawn);
	
	if (HomeBase != NULL)
	{
		HomeBase->ObjectWasDropped(LastHoldingPawn);
	}
	ChangeState(CarriedObjectState::Dropped);
}

void AUTCarriedObject::Use()
{
	// Use should be managed in child classes.
}

void AUTCarriedObject::SendHomeWithNotify()
{
	SendHome();
}

void AUTCarriedObject::SendHome()
{
	DetachRootComponentFromParent(true);
	if (ObjectState == CarriedObjectState::Home) return;	// Don't both if we are already home

	NoLongerHeld();
	ChangeState(CarriedObjectState::Home);
	HomeBase->ObjectReturnedHome(LastHoldingPawn);
	MoveToHome();
}

void AUTCarriedObject::MoveToHome()
{
	AssistTracking.Empty();
	if (HomeBase != NULL)
	{
		FVector BaseLocation = HomeBase->GetActorLocation() + HomeBase->GetActorRotation().RotateVector(HomeBaseOffset) + FVector(0.f, 0.f, Collision->GetScaledCapsuleHalfHeight());
		MovementComponent->Velocity = FVector(0.0f,0.0f,0.0f);
		SetActorLocationAndRotation(BaseLocation, HomeBase->GetActorRotation());
		ForceNetUpdate();
	}
}

void AUTCarriedObject::Score_Implementation(FName Reason, AUTCharacter* ScoringPawn, AUTPlayerState* ScoringPS)
{
	LastGameMessageTime = GetWorld()->GetTimeSeconds();
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL)
	{

		Game->ScoreObject(this, ScoringPawn, ScoringPS, Reason);
	}
	SendHome();
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
		SendHomeWithNotify();
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

float AUTCarriedObject::GetHeldTime(AUTPlayerState* TestHolder)
{
	float CurrentHeldTime = (Holder && (TestHolder == Holder)) ? GetWorld()->GetTimeSeconds() - PickedUpTime : 0.f;
	int32 AssistIndex = FindAssist(TestHolder);
	if (AssistIndex >= 0 && AssistIndex < AssistTracking.Num())
	{
		return CurrentHeldTime + AssistTracking[AssistIndex].TotalHeldTime;
	}
	return CurrentHeldTime;
}
