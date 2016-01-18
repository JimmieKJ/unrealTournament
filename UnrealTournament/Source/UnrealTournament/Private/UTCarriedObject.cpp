// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTProjectileMovementComponent.h"
#include "UTCTFScoring.h"
#include "Net/UnrealNetwork.h"
#include "UTPainVolume.h"
#include "UTLift.h"

AUTCarriedObject::AUTCarriedObject(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Collision = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->InitCapsuleSize(72.0f, 30.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTCarriedObject::OnOverlapBegin);
	Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	RootComponent = Collision;

	MovementComponent = ObjectInitializer.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("MovementComp"));
	MovementComponent->HitZStopSimulatingThreshold = 0.7f;
	MovementComponent->MaxSpeed = 5000.0f; // needed for gravity
	MovementComponent->InitialSpeed = 360.0f;
	MovementComponent->SetIsReplicated(true);
	MovementComponent->OnProjectileStop.AddDynamic(this, &AUTCarriedObject::OnStop);

	SetReplicates(true);
	bReplicateMovement = true;
	NetPriority = 3.0;
	LastGameMessageTime = 0.f;
	AutoReturnTime = 30.0f;
	bMovementEnabled = true;
	LastTeleportedTime = -1000.f;
}

void AUTCarriedObject::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = (GetWorld()->GetTimeSeconds() < LastTeleportedTime + 5.f) ? LastTeleportedLoc : GetActorLocation();
	OutRotation = GetActorRotation();
}

void AUTCarriedObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// backwards compatibility; force values on existing instances
	Collision->SetAbsolute(false, false, true);
	if (Role == ROLE_Authority)
	{
		Collision->SetWorldRotation(FRotator(0.0f, 0.f, 0.f));
	}
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

void AUTCarriedObject::OnStop(const FHitResult& Hit)
{
	if (Hit.Actor.IsValid() && Hit.Component.IsValid() && Cast<AUTLift>(Hit.Actor.Get()))
	{
		AttachRootComponentTo(Hit.Component.Get(), NAME_None, EAttachLocation::KeepWorldPosition);
	}
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
	DOREPLIFETIME(AUTCarriedObject, bMovementEnabled);
	DOREPLIFETIME_CONDITION(AUTCarriedObject, HomeBase, COND_InitialOnly);
}

void AUTCarriedObject::AttachTo(USkeletalMeshComponent* AttachToMesh)
{
	if (AttachToMesh != NULL && Collision != NULL)
	{
		Collision->SetRelativeLocation(Holder3PTransform);
		Collision->SetRelativeRotation(Holder3PRotation);
		AttachRootComponentTo(AttachToMesh, Holder3PSocketName);
		ClientUpdateAttachment(true);
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
		ClientUpdateAttachment(false);
	}
}

void AUTCarriedObject::ClientUpdateAttachment(bool bNowAttached)
{}

void AUTCarriedObject::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsDropping)
	{
		AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
		if (Character != NULL && !GetWorld()->LineTraceTestByChannel(OtherActor->GetActorLocation(), GetActorLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
		{
			TryPickup(Character);
		}
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
	if (Role == ROLE_Authority)
	{
		// update movement component based on state; note that clients do this via OnRep_Moving() so as not to get physics out of sync if there is an inaccuracy in the location of blocking objects
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

	OnCarriedObjectStateChangedDelegate.Broadcast(this, ObjectState);
}

void AUTCarriedObject::OnRep_Moving()
{
	if (bMovementEnabled)
	{
		MovementComponent->SetUpdatedComponent(Collision);
	}
	else
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->SetUpdatedComponent(NULL);
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
	if (GS != NULL && (!GS->IsMatchInProgress() || GS->IsMatchIntermission()))
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
	if (NewHolder == NULL || NewHolder->IsPendingKillPending() || Cast<AUTPlayerState>(NewHolder->PlayerState) == NULL)
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
		if (bWasHome && MessageClass != NULL)
		{
			// the "X flag taken" announcement from home implies the new holder's current location, so update enemy bots
			for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
			{
				AUTBot* B = Cast<AUTBot>(It->Get());
				if (B != NULL && !B->IsTeammate(HoldingPawn))
				{
					B->UpdateEnemyInfo(HoldingPawn, EUT_HeardExact);
				}
			}
		}
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
		// prevent touches while dropping... we'll check them again when we're done
		TGuardValue<bool> DropGuard(bIsDropping, true);

		FVector Extra = FVector::ZeroVector;
		if (ObjectHolder->Health > 0)
		{
			Extra = ObjectHolder->GetActorRotation().Vector() * 900.0f;
		}
		FVector Loc = ObjectHolder->GetActorLocation();
		Loc.Z -= ObjectHolder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		Loc.Z += Collision->GetUnscaledCapsuleHalfHeight() * 1.1f;
		const FRotator DesiredRot = Extra.IsZero() ? FRotator::ZeroRotator : Extra.Rotation();
		bool bSuccess = true;
		if (!TeleportTo(Loc, DesiredRot))
		{
			const float AdjustDist = ObjectHolder->GetSimpleCollisionRadius() * 0.5f;
			FVector AdjustedLocs[] = { Loc + FVector(AdjustDist, 0.0f, 0.0f), Loc - FVector(AdjustDist, 0.0f, 0.0f), Loc + FVector(0.0f, AdjustDist, 0.0f), Loc - FVector(0.0f, AdjustDist, 0.0f) };
			bSuccess = false;
			for (int32 i = 0; i < ARRAY_COUNT(AdjustedLocs); i++)
			{
				if (TeleportTo(AdjustedLocs[i], DesiredRot))
				{
					bSuccess = true;
					break;
				}
			}
		}
		if (bSuccess)
		{
			MovementComponent->Velocity = (0.5f * ObjectHolder->GetMovementComponent()->Velocity) + Extra + (218.0f * (0.5f + FMath::FRand()) * FMath::VRand() + FVector(0.0f, 0.0f, 100.0f));
		}
		else
		{
			// put the flag exactly in the desired location without encroachment checks, then disable movement so it stays exactly there
			// since the prior holder is there, it should be still possible to touch it to acquire/return, but movement might cause the penetration resolution to put it on the wrong side of walls, etc
			TeleportTo(Loc, DesiredRot, false, true);
			MovementComponent->StopMovementImmediately();
			MovementComponent->SetUpdatedComponent(NULL);
		}
	}
	if (ObjectState == CarriedObjectState::Dropped)
	{
		// make sure no other players are touching the flag and should cause it to change state immediately
		Collision->UpdateOverlaps();
		TArray<AActor*> Touching;
		GetOverlappingActors(Touching, APawn::StaticClass());
		for (AActor* Touched : Touching)
		{
			if (Touched != LastHoldingPawn)
			{
				OnOverlapBegin(Touched, Cast<UPrimitiveComponent>(Touched->GetRootComponent()), INDEX_NONE, false, FHitResult(this, Collision, GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)));
				if (ObjectState != CarriedObjectState::Dropped)
				{
					break;
				}
			}
		}
	}
}

bool AUTCarriedObject::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	if (bNoCheck)
	{
		return Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	}

	// during teleportation, we need to change our collision to overlap potential telefrag targets instead of block
	// however, EncroachingBlockingGeometry() doesn't handle reflexivity correctly so we can't get anywhere changing our collision responses
	// instead, we must change our object type to adjust the query
	FVector TeleportStart = GetActorLocation();
	ECollisionChannel SavedObjectType = Collision->GetCollisionObjectType();
	Collision->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
	bool bResult = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	Collision->SetCollisionObjectType(SavedObjectType);
	Collision->UpdateOverlaps(); // make sure collision object type changes didn't mess with our overlaps
	return bResult;
}

void AUTCarriedObject::Drop(AController* Killer)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), DropSound, (HoldingPawn != NULL) ? (AActor*)HoldingPawn : (AActor*)this);

	SendGameMessage(3, Holder, NULL);
	{
		// NoLongerHeld() results in collision being enabled, but we will check for new touches in TossObject()
		TGuardValue<bool> DropGuard(bIsDropping, true);
		NoLongerHeld(Killer);
	}

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
	LastTeleportedTime = GetWorld()->GetTimeSeconds();
	LastTeleportedLoc = GetActorLocation();

	DetachRootComponentFromParent(true);
	if (ObjectState == CarriedObjectState::Home) return;	// Don't both if we are already home

	NoLongerHeld();
	ChangeState(CarriedObjectState::Home);
	HomeBase->ObjectReturnedHome(LastHoldingPawn);
	MoveToHome();
}

FVector AUTCarriedObject::GetHomeLocation() const
{
	if (HomeBase == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Carried object querying home location with no home"), *GetName());
		return GetActorLocation();
	}
	else
	{
		return HomeBase->GetActorLocation() + HomeBase->GetActorRotation().RotateVector(HomeBaseOffset) + FVector(0.f, 0.f, Collision->GetScaledCapsuleHalfHeight());
	}
}
FRotator AUTCarriedObject::GetHomeRotation() const
{
	if (HomeBase == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Carried object querying home rotation with no home"), *GetName());
		return GetActorRotation();
	}
	else
	{
		return HomeBase->GetActorRotation() + HomeBaseRotOffset;
	}
}

void AUTCarriedObject::MoveToHome()
{
	DetachRootComponentFromParent(true);
	AssistTracking.Empty();
	HolderRescuers.Empty();
	if (HomeBase != NULL)
	{
		MovementComponent->Velocity = FVector(0.0f,0.0f,0.0f);
		SetActorLocationAndRotation(GetHomeLocation(), GetHomeRotation());
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

void AUTCarriedObject::EnteredPainVolume(AUTPainVolume* PainVolume)
{
	if (Holder)
	{
		return;
	}
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
				ClientUpdateAttachment(Cast<APawn>(AttachmentReplication.AttachParent) != nullptr);
			}
		}
	}
	else
	{
		DetachRootComponentFromParent();
		ClientUpdateAttachment(false);
	}
}

// HACK: workaround for engine bug with transform replication when attaching/detaching things
void AUTCarriedObject::OnRep_ReplicatedMovement()
{
	// ignore the redundant ReplicatedMovement we replicated below
	if (AttachmentReplication.AttachParent == NULL)
	{
		Super::OnRep_ReplicatedMovement();
		if ((ObjectState == CarriedObjectState::Home) && (HomeBase != NULL))
		{
			const FVector BaseLocation = HomeBase->GetActorLocation() + HomeBase->GetActorRotation().RotateVector(HomeBaseOffset) + FVector(0.f, 0.f, Collision->GetScaledCapsuleHalfHeight());
			MovementComponent->Velocity = FVector(0.0f, 0.0f, 0.0f);
			if ((BaseLocation - GetActorLocation()).SizeSquared() > 1.f)
			{
				SetActorLocationAndRotation(BaseLocation, HomeBase->GetActorRotation());
			}
		}
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
