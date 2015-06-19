// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_ImpactHammer.h"
#include "UTWeaponStateFiringCharged.h"
#include "UTCharacterMovement.h"
#include "UTLift.h"
#include "UTReachSpec_HighJump.h"
#include "StatNames.h"

AUTWeap_ImpactHammer::AUTWeap_ImpactHammer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringCharged>(TEXT("FiringState0")))
{
	if (FiringState.Num() > 0)
	{
		UUTWeaponStateFiringCharged* ChargedState = Cast<UUTWeaponStateFiringCharged>(FiringState[0]);
		if (ChargedState != NULL)
		{
			ChargedState->bChargeFlashCount = true;
		}
	}
	ClassicGroup = 1;
	WeaponBobScaling = 0.7f;
	FiringViewKickback = -50.f;
	FullChargeTime = 2.5f;
	FullImpactChargePct = 0.2f;
	MinAutoChargePct = 1.f;
	ImpactJumpTraceDist = 220.f;
	FOVOffset = FVector(0.1f, 1.f, 2.f);

	DroppedPickupClass = NULL; // doesn't drop

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bAffectedByStoppingPower = true;

	BaseAISelectRating = 0.35f;

	KillStatsName = NAME_ImpactHammerKills;
	DeathStatsName = NAME_ImpactHammerDeaths;
	DisplayName = NSLOCTEXT("UTWeap_ImpactHammer", "DisplayName", "Impact Hammer");
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
		float DamageMult = FMath::Min<float>(ChargedMode->ChargeTime / FullChargeTime, 1.0f);

		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
		const FVector FireDir = SpawnRotation.Vector();
		FCollisionQueryParams TraceParams(FName(TEXT("ImpactHammer")), true, UTOwner);
		
		FHitResult Hit;
		// if we auto-triggered on something, make sure it counts
		if (AutoHitTarget != NULL)
		{
			if (!AutoHitTarget->ActorLineTraceSingle(Hit, SpawnLocation, SpawnLocation + (AutoHitTarget->GetActorLocation() - SpawnLocation) * 2.0f, COLLISION_TRACE_WEAPON, TraceParams))
			{
				Hit = FHitResult(AutoHitTarget, Cast<UPrimitiveComponent>(AutoHitTarget->GetRootComponent()), AutoHitTarget->GetActorLocation(), -FireDir);
			}
		}
		else
		{
			float TraceDist = InstantHitInfo[CurrentFireMode].TraceRange;
			if (bDealDamage && ((FVector(0.f, 0.f, -1.f) | FireDir) > 0.f))
			{
				TraceDist += (ImpactJumpTraceDist - InstantHitInfo[CurrentFireMode].TraceRange) * (FVector(0.f, 0.f, -1.f) | FireDir);
			}

			const FVector EndTrace = SpawnLocation + FireDir * TraceDist;
			
			if (!GetWorld()->LineTraceSingleByChannel(Hit, SpawnLocation, EndTrace, COLLISION_TRACE_WEAPON, TraceParams))
			{
				Hit.Location = EndTrace;
			}
			else if (bDealDamage && Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && ((Hit.Location - SpawnLocation).Size() > InstantHitInfo[CurrentFireMode].TraceRange))
			{
				// only accept targets beyond trace range if they are world geometry
				bDealDamage = false;
			}
		}
		if (Role == ROLE_Authority && bDealDamage)
		{
			UTOwner->FlashCount = 0; // used by weapon attachment
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
						if (EnemyWeaponState != NULL && EnemyWeaponState->bCharging && ChargedMode->ChargeTime >= FullChargeTime * MinAutoChargePct)
						{
							float MyAim = FireDir | (C->GetActorLocation() - SpawnLocation).GetSafeNormal();
							const FVector EnemyFireStart = C->GetWeapon()->GetFireStartLoc();
							const FVector EnemyFireDir = C->GetWeapon()->GetAdjustedAim(EnemyFireStart).Vector();
							float EnemyAim = EnemyFireDir | (UTOwner->GetActorLocation() - EnemyFireStart).GetSafeNormal();
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
			if ((Hit.Component != NULL) && UTOwner != nullptr && UTOwner->UTCharacterMovement && (!Hit.Actor.IsValid() || !Hit.Actor->bCanBeDamaged || Cast<AUTLift>(Hit.Actor.Get())))
			{
				// if we hit something undamageable (world geometry, etc) then the damage is caused to ourselves instead
				// Special case of fixed damage and momentum
				bool bIsFullImpactImpulse = (ChargedMode->ChargeTime > FullChargeTime * FullImpactChargePct);
				float FinalDamage = bIsFullImpactImpulse ? UTOwner->UTCharacterMovement->FullImpactDamage : UTOwner->UTCharacterMovement->EasyImpactDamage;
				if (UTOwner->GetCharacterMovement()->Velocity.Z <= -1.f * UTOwner->MaxSafeFallSpeed)
				{
					// take falling damage, but give credit for it against impact damage
					float OldHealth = UTOwner->Health;
					UTOwner->TakeFallingDamage(Hit, UTOwner->GetCharacterMovement()->Velocity.Z);
					// damage might kill them
					if (UTOwner != NULL)
					{
						FinalDamage = FMath::Max(0.f, FinalDamage - (OldHealth - UTOwner->Health));
					}
				}
				// falling damage might have killed Owner
				if (UTOwner != NULL)
				{
					FVector JumpDir = -1.f*FireDir;
					if ((SpawnRotation.Pitch < 290.f) && (SpawnRotation.Pitch > 260.f))
					{
						// consider as straight down
						JumpDir = FVector(0.f, 0.f, 1.f);
					}

					UTOwner->UTCharacterMovement->ApplyImpactVelocity(JumpDir, bIsFullImpactImpulse);
					UUTGameplayStatics::UTPlaySound(GetWorld(), ImpactJumpSound, UTOwner, SRT_AllButOwner);
					UTOwner->TakeDamage(FinalDamage, FUTPointDamageEvent(FinalDamage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FVector(0.f)), UTOwner->Controller, this);
				}
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
	UUTWeaponStateFiringCharged* ChargedMode = Cast<UUTWeaponStateFiringCharged>(CurrentState);
	float OldChargeTime = (ChargedMode && ChargedMode->bCharging) ? ChargedMode->ChargeTime : 0.f;
	Super::Tick(DeltaTime);

	// check for auto target
	if (AutoHitTarget == NULL && UTOwner != NULL && Role == ROLE_Authority)
	{
		if (ChargedMode != NULL && ChargedMode->bCharging)
		{
			if (ChargedMode->ChargeTime >= FullChargeTime * FullImpactChargePct)
			{
				if (OldChargeTime < FullChargeTime * FullImpactChargePct)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), ChargeClickSound, UTOwner, SRT_AllButOwner);
				}
			}
			if (ChargedMode->ChargeTime >= FullChargeTime * MinAutoChargePct)
			{
				if (OldChargeTime < FullChargeTime * MinAutoChargePct)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), ChargeClickSound, UTOwner, SRT_AllButOwner);
				}

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
			// if AI controlled, see if bot wants to stop charging
			if (UTOwner != NULL) // above auto-fire might have killed Owner
			{
				AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
				if (B != NULL && !B->IsCharging() && GetAISelectRating() < 0.5f)
				{
					UTOwner->StopFiring();
				}
			}
		}
	}
}

bool AUTWeap_ImpactHammer::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	if (Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc))
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL)
		{
			// intentionally ignoring bPreferCurrentMode here as either fire mode can be pretty worthless based on the bot's current movement goals
			BestFireMode = 1;
			if (B->IsCharging())
			{
				BestFireMode = 0;
			}
			else
			{
				for (const FRouteCacheItem& Item : B->RouteCache)
				{
					if (Item.Actor.IsValid() && Item.Actor.Get() == B->GetEnemy())
					{
						BestFireMode = 0;
					}
				}
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

void AUTWeap_ImpactHammer::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	// tell bot that it has an impact hammer
	AUTBot* B = Cast<AUTBot>(NewOwner->Controller);
	if (B != NULL)
	{
		B->ImpactJumpZ = FMath::Max<float>(B->ImpactJumpZ, UTOwner->UTCharacterMovement->FullImpactImpulse - UTOwner->GetCharacterMovement()->JumpZVelocity);
	}
}

float AUTWeap_ImpactHammer::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL)
	{
		return BaseAISelectRating;
	}
	else
	{
		// super desireable for bot waiting to impact jump
		UUTReachSpec_HighJump* JumpSpec = Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get());
		if (JumpSpec != NULL && !B->AllowTranslocator() && JumpSpec->CalcAvailableSimpleJumpZ(UTOwner) < JumpSpec->CalcRequiredJumpZ(UTOwner))
		{
			return 9.f;
		}
		else if (B->GetEnemy() == NULL)
		{
			return BaseAISelectRating;
		}
		else
		{
			float EnemyDist = (B->GetEnemyLocation(B->GetEnemy(), true) - UTOwner->GetActorLocation()).Size();
			if (B->IsStopped() && EnemyDist > 225.0f)
			{
				return 0.1;
			}
			else if (EnemyDist < 1600.0f && B->Skill <= 2.0f && Cast<AUTCharacter>(B->GetEnemy()) != NULL && Cast<AUTWeap_ImpactHammer>(((AUTCharacter*)B->GetEnemy())->GetWeapon()) != NULL)
			{
				return FMath::Clamp<float>(650.0f / (EnemyDist + 1.0f), 0.6f, 0.75f);
			}
			else if (EnemyDist > 900.0f)
			{
				return 0.1;
			}
			else if (UTOwner->GetWeapon() != this && EnemyDist < 250.0f)
			{
				return 0.25;
			}
			else
			{
				return FMath::Min<float>(0.6f, 200.0f / (EnemyDist + 1.0f));
			}
		}
	}
}

bool AUTWeap_ImpactHammer::DoAssistedJump()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL)
	{
		return false;
	}
	// if AI no longer has enough health (got hit on the way?) then abort
	else if (UTOwner->HealthMax * UTOwner->GetEffectiveHealthPct(false) <= UTOwner->UTCharacterMovement->FullImpactDamage)
	{
		B->MoveTimer = -1.0f;
		return false;
	}
	else
	{
		// look at ground
		FVector LookPoint = UTOwner->GetActorLocation() + (B->GetMovePoint() - UTOwner->GetActorLocation()).GetSafeNormal2D() - FVector(0.0f, 0.0f, 1000.0f);
		if (B->NeedToTurn(LookPoint))
		{
			B->SetFocalPoint(LookPoint, SCRIPTEDMOVE_FOCUS_PRIORITY);
			return false; // not ready yet
		}
		else if (!UTOwner->CanJump())
		{
			return false; // not ready yet
		}
		else if (CurrentFireMode != 0 && IsFiring())
		{
			// stop other firemode
			UTOwner->StopFiring();
			return false;
		}
		else if (!IsFiring())
		{
			// start charging
			UTOwner->StartFire(0);
			return false;
		}
		else
		{
			UUTWeaponStateFiringCharged* ChargedState = Cast<UUTWeaponStateFiringCharged>(CurrentState);
			const bool bFullImpactJump = (ChargedState == NULL || ChargedState->ChargeTime >= FullChargeTime * FullImpactChargePct);
			UUTReachSpec_HighJump* JumpSpec = Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get());
			if (bFullImpactJump || (JumpSpec != NULL && JumpSpec->CalcRequiredJumpZ(UTOwner) * 1.05f < UTOwner->UTCharacterMovement->EasyImpactImpulse))
			{
				// do it!
				const float ZSpeed = bFullImpactJump ? UTOwner->UTCharacterMovement->FullImpactImpulse : UTOwner->UTCharacterMovement->EasyImpactImpulse;
				FVector DesiredVel2D;
				if (AUTBot::FindBestJumpVelocityXY(DesiredVel2D, UTOwner->GetActorLocation(), B->GetMovePoint(), ZSpeed, UTOwner->UTCharacterMovement->GetGravityZ(), UTOwner->GetSimpleCollisionHalfHeight()))
				{
					float DesiredSpeed = DesiredVel2D.Size2D();
					// if low speed and not going to bump head on ceiling, do normal jump
					if (DesiredSpeed < UTOwner->UTCharacterMovement->GetMaxSpeed() && !GetWorld()->LineTraceTestByChannel(UTOwner->GetActorLocation(), UTOwner->GetActorLocation() + FVector(0.0f, 0.0f, ZSpeed * 0.5f), ECC_Pawn, FCollisionQueryParams(FName(TEXT("ImpactJump")), false, UTOwner)))
					{
						UTOwner->UTCharacterMovement->Velocity = DesiredVel2D;
					}
					else
					{
						// forward dodge for more XY speed
						FRotationMatrix YawMat(FRotator(0.f, (B->GetMovePoint() - UTOwner->GetActorLocation()).Rotation().Yaw, 0.f));
						FVector X = YawMat.GetScaledAxis(EAxis::X).GetSafeNormal();
						FVector Y = YawMat.GetScaledAxis(EAxis::Y).GetSafeNormal();
						UTOwner->Dodge(X, Y);
					}
				}
				else
				{
					UTOwner->UTCharacterMovement->Velocity = (B->GetMovePoint() - UTOwner->GetActorLocation()).GetSafeNormal2D() * UTOwner->UTCharacterMovement->GetMaxSpeed();
				}
				UTOwner->StopFire(0);
				return true;
			}
			else
			{
				// waiting for charge
				return false;
			}
		}
	}
}