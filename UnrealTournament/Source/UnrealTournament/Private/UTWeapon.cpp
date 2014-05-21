// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateActive.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"
#include "UTWeaponStateInactive.h"
#include "UnrealNetwork.h"

AUTWeapon::AUTWeapon(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	AmmoCost.Add(1);
	AmmoCost.Add(1);

	Ammo = 20;
	MaxAmmo = 50;

	BringUpTime = 0.5f;
	PutDownTime = 0.5f;

	bFPFireFromCenter = true;
	FireOffset = FVector(75.0f, 0.0f, 0.0f);

	InactiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateInactive>(this, TEXT("StateInactive"));
	ActiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateActive>(this, TEXT("StateActive"));
	EquippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateEquipping>(this, TEXT("StateEquipping"));
	UnequippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateUnequipping>(this, TEXT("StateUnequipping"));

	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);
	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh1P"));
	Mesh->SetOnlyOwnerSee(true);
	Mesh->AttachParent = RootComponent;

	if (FiringState.Num() == 0)
	{
		for (int32 i = 0; i < 2; i++)
		{
			UUTWeaponStateFiring* NewState = PCIP.CreateDefaultSubobject<UUTWeaponStateFiring, UUTWeaponStateFiring>(this, FName(*FString::Printf(TEXT("FiringState%i"), i)), false, false, false);
			if (NewState != NULL)
			{
				FiringState.Add(NewState);
				FireInterval.Add(1.0f);
				UParticleSystemComponent* PSC = PCIP.CreateDefaultSubobject<UParticleSystemComponent, UParticleSystemComponent>(this, FName(*FString::Printf(TEXT("MuzzleFlash%i"), i)), false, false, false);
				MuzzleFlash.Add(PSC);
				if (PSC != NULL)
				{
					PSC->bAutoActivate = false;
					PSC->SetOnlyOwnerSee(true);
					PSC->AttachParent = Mesh;
				}
			}
		}
	}
}

void AUTWeapon::BeginPlay()
{
	Super::BeginPlay();

	// might have already been activated if at startup, see ClientGivenTo_Internal()
	if (CurrentState == NULL)
	{
		GotoState(InactiveState);
	}
	checkSlow(CurrentState != NULL);
}

void AUTWeapon::GotoState(UUTWeaponState* NewState)
{
	if (NewState == NULL || !NewState->IsIn(this))
	{
		UE_LOG(UT, Warning, TEXT("Attempt to send %s to invalid state %s"), *GetName(), (NewState == NULL) ? TEXT("None") : *NewState->GetFullName());
	}
	else if (CurrentState != NewState)
	{
		if (CurrentState != NULL)
		{
			CurrentState->EndState();
		}
		const UUTWeaponState* PrevState = CurrentState;
		CurrentState = NewState;
		CurrentState->BeginState(PrevState); // NOTE: may trigger another GotoState() call
	}
}

void AUTWeapon::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	// if character has ammo on it, transfer to weapon
	for (int32 i = 0; i < NewOwner->SavedAmmo.Num(); i++)
	{
		if (NewOwner->SavedAmmo[i].Type == GetClass())
		{
			AddAmmo(NewOwner->SavedAmmo[i].Amount);
			NewOwner->SavedAmmo.RemoveAt(i);
			break;
		}
	}
}

void AUTWeapon::ClientGivenTo_Internal(bool bAutoActivate)
{
	// make sure we initialized our state; this can be triggered if the weapon is spawned at game startup, since BeginPlay() will be deferred
	if (CurrentState == NULL)
	{
		GotoState(InactiveState);
	}

	Super::ClientGivenTo_Internal(bAutoActivate);

	if (bAutoActivate)
	{
		UTOwner->CheckAutoWeaponSwitch(this);
	}
}

void AUTWeapon::StartFire(uint8 FireModeNum)
{
	BeginFiringSequence(FireModeNum);
	if (Role < ROLE_Authority)
	{
		ServerStartFire(FireModeNum);
	}
}
void AUTWeapon::ServerStartFire_Implementation(uint8 FireModeNum)
{
	BeginFiringSequence(FireModeNum);
}
bool AUTWeapon::ServerStartFire_Validate(uint8 FireModeNum)
{
	return true;
}
void AUTWeapon::BeginFiringSequence(uint8 FireModeNum)
{
	UTOwner->SetPendingFire(FireModeNum, true);
	CurrentState->BeginFiringSequence(FireModeNum);
}

void AUTWeapon::StopFire(uint8 FireModeNum)
{
	EndFiringSequence(FireModeNum);
	if (Role < ROLE_Authority)
	{
		ServerStopFire(FireModeNum);
	}
}
void AUTWeapon::ServerStopFire_Implementation(uint8 FireModeNum)
{
	EndFiringSequence(FireModeNum);
}
bool AUTWeapon::ServerStopFire_Validate(uint8 FireModeNum)
{
	return true;
}
void AUTWeapon::EndFiringSequence(uint8 FireModeNum)
{
	UTOwner->SetPendingFire(FireModeNum, false);
	CurrentState->EndFiringSequence(FireModeNum);
}

void AUTWeapon::BringUp()
{
	AttachToOwner();

	CurrentState->BringUp();
}
bool AUTWeapon::PutDown()
{
	if (eventPreventPutDown())
	{
		return false;
	}
	else
	{
		CurrentState->PutDown();
		return true;
	}
}

void AUTWeapon::AttachToOwner_Implementation()
{
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->AttachTo(UTOwner->FirstPersonMesh);
	}
}

void AUTWeapon::DetachFromOwner_Implementation()
{
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->DetachFromParent();
	}
}

void AUTWeapon::PlayFiringEffects()
{
	if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL)
	{
		// try and play the sound if specified
		if (FireSound.IsValidIndex(CurrentFireMode) && FireSound[CurrentFireMode] != NULL)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FireSound[CurrentFireMode], UTOwner->GetActorLocation());
		}

		// try and play a firing animation if specified
		if (FireAnimation.IsValidIndex(CurrentFireMode) && FireAnimation[CurrentFireMode] != NULL)
		{
			// Get the animation object for the arms mesh
			UAnimInstance* AnimInstance = UTOwner->FirstPersonMesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(FireAnimation[CurrentFireMode], 1.f);
			}
		}

		// muzzle flash
		if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL)
		{
			MuzzleFlash[CurrentFireMode]->ActivateSystem();
		}
	}
}
void AUTWeapon::StopFiringEffects()
{
	// TODO
}
void AUTWeapon::PlayImpactEffects(const FVector& TargetLoc)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// fire effects
		if (FireEffect.IsValidIndex(CurrentFireMode) && FireEffect[CurrentFireMode] != NULL)
		{
			const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CurrentFireMode], SpawnLocation, (TargetLoc - SpawnLocation).Rotation(), true);
			static FName NAME_HitLocation(TEXT("HitLocation"));
			PSC->SetVectorParameter(NAME_HitLocation, TargetLoc);
		}
		// FIXME: temp debug line until we have effects
		else
		{
			const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
			DrawDebugLine(GetWorld(), SpawnLocation, TargetLoc, FColor(0, 0, 255), false, 0.5f);
		}
	}
}

void AUTWeapon::FireShot()
{
	ConsumeAmmo(CurrentFireMode);

	if (!FireShotOverride())
	{
		if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
		{
			FireProjectile();
		}
		else if (InstantHitInfo.IsValidIndex(CurrentFireMode) && InstantHitInfo[CurrentFireMode].DamageType != NULL)
		{
			FireInstantHit();
		}
		PlayFiringEffects();
	}
}

void AUTWeapon::AddAmmo(int32 Amount)
{
	Ammo = FMath::Clamp<int32>(Ammo + Amount, 0, MaxAmmo);
	
	// trigger weapon switch if necessary
	if (UTOwner->IsLocallyControlled())
	{
		OnRep_Ammo();
	}
}

void AUTWeapon::OnRep_Ammo()
{
	for (int32 i = GetNumFireModes() - 1; i >= 0; i--)
	{
		if (HasAmmo(i))
		{
			return;
		}
	}
	// TODO: UTOwner->Controller->SwitchToBestWeapon();
}

void AUTWeapon::ConsumeAmmo(uint8 FireModeNum)
{
	// FIXME: temporarily disabled until we have enough systems going
	return;

	if (AmmoCost.IsValidIndex(FireModeNum))
	{
		AddAmmo(-AmmoCost[FireModeNum]);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Invalid fire mode %i in %s::ConsumeAmmo()"), int32(FireModeNum), *GetName());
	}
}

bool AUTWeapon::HasAmmo(uint8 FireModeNum)
{
	return (AmmoCost.IsValidIndex(FireModeNum) && Ammo > AmmoCost[FireModeNum]);
}

FVector AUTWeapon::GetFireStartLoc()
{
	if (bFPFireFromCenter && Cast<APlayerController>(UTOwner->Controller) != NULL) // TODO: first person view check
	{
		return UTOwner->GetPawnViewLocation() + GetFireRotation().RotateVector(FireOffset);
	}
	else
	{
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		return UTOwner->GetActorLocation() + GetFireRotation().RotateVector(FireOffset);
	}
}

FRotator AUTWeapon::GetFireRotation()
{
	return UTOwner->GetViewRotation();
}

void AUTWeapon::FireInstantHit()
{
	checkSlow(InstantHitInfo.IsValidIndex(CurrentFireMode));

	const FRotator SpawnRotation = GetFireRotation();
	const FVector SpawnLocation = GetFireStartLoc();
	const FVector FireDir = SpawnRotation.Vector();
	const FVector EndTrace = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingle(Hit, SpawnLocation, EndTrace, COLLISION_TRACE_WEAPON, FCollisionQueryParams(GetClass()->GetFName(), false, UTOwner)))
	{
		Hit.Location = EndTrace;
	}
	else if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged)
	{
		// TODO: replicated momentum handling
		if (Hit.Component != NULL)
		{
			Hit.Component->AddImpulseAtLocation(FireDir * InstantHitInfo[CurrentFireMode].Momentum, Hit.Location);
		}

		Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage, FPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType), UTOwner->Controller, this);
	}
	if (Role == ROLE_Authority)
	{
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
	}
}

AUTProjectile* AUTWeapon::FireProjectile()
{
	if (Role == ROLE_Authority)
	{
		checkSlow(ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL);

		// try and fire a projectile
		const FRotator SpawnRotation = GetFireRotation();
		const FVector SpawnLocation = GetFireStartLoc();

		UTOwner->IncrementFlashCount(CurrentFireMode);

		// spawn the projectile at the muzzle
		FActorSpawnParameters Params;
		Params.Instigator = UTOwner;
		Params.Owner = UTOwner;
		return GetWorld()->SpawnActor<AUTProjectile>(ProjClass[CurrentFireMode], SpawnLocation, SpawnRotation, Params);
	}
	else
	{
		return NULL;
	}
}

float AUTWeapon::GetRefireTime(uint8 FireModeNum)
{
	if (FireInterval.IsValidIndex(FireModeNum))
	{
		return FMath::Max<float>(0.01f, FireInterval[FireModeNum]);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Invalid firing mode %i in %s::GetRefireTime()"), int32(FireModeNum), *GetName());
		return 0.1f;
	}
}

void AUTWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState != InactiveState)
	{
		CurrentState->Tick(DeltaTime);
	}
}

void AUTWeapon::Destroyed()
{
	Super::Destroyed();

	// this makes sure timers, etc go away
	GotoState(InactiveState);
}

void AUTWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeapon, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTWeapon, MaxAmmo, COND_OwnerOnly);
}
