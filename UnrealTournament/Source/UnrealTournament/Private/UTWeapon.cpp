// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateActive.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"
#include "UTWeaponStateInactive.h"

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
	FireOffset = FVector(100.0f, 30.0f, 10.0f);

	InactiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateInactive>(this, TEXT("StateInactive"));
	ActiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateActive>(this, TEXT("StateActive"));
	EquippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateEquipping>(this, TEXT("StateEquipping"));
	UnequippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateUnequipping>(this, TEXT("StateUnequipping"));

	if (FiringState.Num() == 0)
	{
		for (int32 i = 0; i < 2; i++)
		{
			UUTWeaponStateFiring* NewState = PCIP.CreateDefaultSubobject<UUTWeaponStateFiring, UUTWeaponStateFiring>(this, FName(*FString::Printf(TEXT("FiringState%i"), i)), false, false, false);
			if (NewState != NULL)
			{
				FiringState.Add(NewState);
				FireInterval.Add(1.0f);
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
	CurrentState->EndFiringSequence(FireModeNum);
}

void AUTWeapon::BringUp()
{
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

		// TODO: muzzle flash

		// fire effects
		if (FireEffect.IsValidIndex(CurrentFireMode) && FireEffect[CurrentFireMode] != NULL)
		{
			const FVector SpawnLocation = UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CurrentFireMode], SpawnLocation, (UTOwner->FlashLocation - SpawnLocation).Rotation(), true);
			static FName NAME_HitLocation(TEXT("HitLocation"));
			PSC->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
		}
		// FIXME: temp debug line until we have effects
		else
		{
			DrawDebugLine(GetWorld(), UTOwner->GetActorLocation() + UTOwner->GetViewRotation().RotateVector(FireOffset), UTOwner->FlashLocation, FColor(0, 0, 255), false, 0.5f);
		}
	}
}
void AUTWeapon::StopFiringEffects()
{
	// TODO
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
	}
}

void AUTWeapon::ConsumeAmmo(uint8 FireModeNum)
{
	if (AmmoCost.IsValidIndex(FireModeNum))
	{
		Ammo -= AmmoCost[FireModeNum];
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
		return UTOwner->GetPawnViewLocation();
	}
	else
	{
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		return UTOwner->GetActorLocation() + UTOwner->GetViewRotation().RotateVector(FireOffset);
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

	FHitResult Hit;
	GetWorld()->LineTraceSingle(Hit, SpawnLocation, SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange, ECC_GameTraceChannel1, FCollisionQueryParams(GetClass()->GetFName(), true, UTOwner));
	if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged)
	{
		// TODO: replicated momentum handling
		if (Hit.Component != NULL)
		{
			Hit.Component->AddImpulseAtLocation(FireDir * InstantHitInfo[CurrentFireMode].Momentum, Hit.Location);
		}

		Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage, FPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType), UTOwner->Controller, this);
	}
	UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
}

AUTProjectile* AUTWeapon::FireProjectile()
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
