// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_ImpactHammer.h"
#include "UTWeaponStateFiringCharged.h"
#include "UTCharacterMovement.h"

AUTWeap_ImpactHammer::AUTWeap_ImpactHammer(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringCharged>(TEXT("FiringState0")))
{
	if (FiringState.Num() > 0)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[0] = UUTWeaponStateFiringCharged::StaticClass();
#endif
	}
	WeaponBobScaling = 0.7f;
	FullChargeTime = 2.5f;
	MinChargePct = 0.4f;

	DroppedPickupClass = NULL; // doesn't drop

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	EasyImpactDamage = 30;
	ImpactMaxHorizontalVelocity = 1500.f;
	ImpactMaxVerticalVelocity = 1500.f;
}

void AUTWeap_ImpactHammer::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	UUTWeaponStateFiringCharged* ChargedMode = Cast<UUTWeaponStateFiringCharged>(CurrentState);
	if (ChargedMode == NULL)
	{
		Super::FireInstantHit(bDealDamage, OutHit);
	}
	else
	{
		float DamageMult = FMath::Clamp<float>(ChargedMode->ChargeTime / FullChargeTime, MinChargePct, 1.0f);

		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
		const FVector FireDir = SpawnRotation.Vector();
		FCollisionQueryParams TraceParams(FName(TEXT("ImpactHammer")), false, UTOwner);
		
		FHitResult Hit;
		// if we auto-triggered on something, make sure it counts
		if (AutoHitTarget != NULL)
		{
			if (!AutoHitTarget->ActorLineTraceSingle(Hit, SpawnLocation, SpawnLocation + (AutoHitTarget->GetActorLocation() - SpawnLocation) * 2.0f, COLLISION_TRACE_WEAPON, TraceParams))
			{
				Hit = FHitResult(AutoHitTarget, AutoHitTarget->GetRootPrimitiveComponent(), AutoHitTarget->GetActorLocation(), -FireDir);
			}
		}
		else
		{
			const FVector EndTrace = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;
			
			if (!GetWorld()->LineTraceSingle(Hit, SpawnLocation, EndTrace, COLLISION_TRACE_WEAPON, TraceParams))
			{
				Hit.Location = EndTrace;
			}
		}
		if (Role == ROLE_Authority)
		{
			UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
		}
		if (bDealDamage)
		{
			if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged)
			{
				// if we're against another hammer impacting, then player who is aiming most towards target center should win
				if (Role == ROLE_Authority)
				{
					AUTCharacter* C = Cast<AUTCharacter>(Hit.Actor.Get());
					if (C != NULL && C != UTOwner && C->GetWeaponClass() == GetClass() && C->GetWeapon()->GetCurrentFireMode() == CurrentFireMode)
					{
						UUTWeaponStateFiringCharged* EnemyWeaponState = Cast<UUTWeaponStateFiringCharged>(C->GetWeapon()->GetCurrentState());
						if (EnemyWeaponState != NULL && EnemyWeaponState->bCharging && ChargedMode->ChargeTime >= FullChargeTime * MinChargePct)
						{
							float MyAim = FireDir | (C->GetActorLocation() - SpawnLocation).SafeNormal();
							const FVector EnemyFireStart = C->GetWeapon()->GetFireStartLoc();
							const FVector EnemyFireDir = C->GetWeapon()->GetAdjustedAim(EnemyFireStart).Vector();
							float EnemyAim = EnemyFireDir | (UTOwner->GetActorLocation() - EnemyFireStart).SafeNormal();
							if (EnemyAim > MyAim)
							{
								// allow enemy to shoot first...
								EnemyWeaponState->EndFiringSequence(C->GetWeapon()->GetCurrentFireMode());
							}
						}
					}
					if (UTOwner != NULL && !UTOwner->IsDead())
					{
						Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage * DamageMult, FUTPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage * DamageMult, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FireDir * InstantHitInfo[CurrentFireMode].Momentum * DamageMult), UTOwner->Controller, this);
					}
				}
			}
			else if ((Hit.Component != NULL) && UTOwner->UTCharacterMovement)
			{
				// if we hit something undamageable (world geometry, etc) then the damage is caused to ourselves instead
				// Special case of fixed damage and momentum
				float FinalDamage = EasyImpactDamage;
				if (UTOwner->CharacterMovement->Velocity.Z >= -1.f * UTOwner->MaxSafeFallSpeed)
				{
					// take falling damage, but give credit for it against impact damage
					float OldHealth = UTOwner->Health;
					UTOwner->TakeFallingDamage(Hit);
					FinalDamage = FMath::Max(0.f, FinalDamage - (OldHealth - UTOwner->Health));
				}
				FVector JumpDir = -1.f*FireDir;
				if ((SpawnRotation.Pitch < 290.f) && (SpawnRotation.Pitch > 260.f))
				{
					// consider as straight down
					JumpDir = FVector(0.f, 0.f, 1.f);
				}

				// provide scaled boost in facing direction, clamped to ImpactMaxHorizontalVelocity and ImpactMaxVerticalVelocity
				// @TODO FIXMESTEVE should use AddDampedImpulse()?
				FVector NewVelocity = UTOwner->CharacterMovement->Velocity + JumpDir * UTOwner->UTCharacterMovement->EasyImpactImpulse;
				if (NewVelocity.Size2D() > ImpactMaxHorizontalVelocity)
				{
					float VelZ = NewVelocity.Z;
					NewVelocity = NewVelocity.SafeNormal2D() * ImpactMaxHorizontalVelocity;
					NewVelocity.Z = VelZ;
				}
				if (NewVelocity.Z > ImpactMaxVerticalVelocity)
				{
					NewVelocity.Z = ImpactMaxVerticalVelocity;
				}
				UTOwner->CharacterMovement->Velocity = NewVelocity;
				UTOwner->CharacterMovement->SetMovementMode(MOVE_Falling);
				UTOwner->CharacterMovement->bNotifyApex = true;
				UUTGameplayStatics::UTPlaySound(GetWorld(), ImpactJumpSound, UTOwner, SRT_AllButOwner);
				UTOwner->TakeDamage(FinalDamage, FUTPointDamageEvent(FinalDamage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FVector(0.f)), UTOwner->Controller, this);
			}
		}
		if (OutHit != NULL)
		{
			*OutHit = Hit;
		}
	}
}

void AUTWeap_ImpactHammer::ClientAutoHit_Implementation(AActor* Target)
{
	if (UTOwner != NULL)
	{
		UUTWeaponStateFiringCharged* ChargedMode = Cast<UUTWeaponStateFiringCharged>(CurrentState);
		if (ChargedMode != NULL && ChargedMode->bCharging)
		{
			AutoHitTarget = Target;
			ChargedMode->EndFiringSequence(CurrentFireMode);
			AutoHitTarget = NULL;
		}
	}
}

bool AUTWeap_ImpactHammer::AllowAutoHit_Implementation(AActor* PotentialTarget)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	return (Cast<APawn>(PotentialTarget) != NULL && (GS == NULL || !GS->OnSameTeam(PotentialTarget, UTOwner)));
}

void AUTWeap_ImpactHammer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// check for auto target
	if (AutoHitTarget == NULL && UTOwner != NULL && Role == ROLE_Authority)
	{
		UUTWeaponStateFiringCharged* ChargedMode = Cast<UUTWeaponStateFiringCharged>(CurrentState);
		if (ChargedMode != NULL && ChargedMode->bCharging && ChargedMode->ChargeTime >= FullChargeTime * MinChargePct)
		{
			FHitResult Hit;
			FireInstantHit(false, &Hit);
			if (Hit.Actor.IsValid() && AllowAutoHit(Hit.Actor.Get()))
			{
				AutoHitTarget = Hit.Actor.Get();
				if (!UTOwner->IsLocallyControlled())
				{
					ClientAutoHit(AutoHitTarget);
				}
				ChargedMode->EndFiringSequence(CurrentFireMode);
				AutoHitTarget = NULL;
			}
		}
	}
}