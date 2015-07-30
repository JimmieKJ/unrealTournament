// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeap_RocketLauncher.h"
#include "UTWeaponStateFiringChargedRocket.h"
#include "UTProj_Rocket.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealNetwork.h"
#include "StatNames.h"

AUTWeap_RocketLauncher::AUTWeap_RocketLauncher(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringChargedRocket>(TEXT("FiringState1")))
{
	ClassicGroup = 8;
	BringUpTime = 0.41f;

	NumLoadedRockets = 0;
	MaxLoadedRockets = 3;
	RocketLoadTime = 0.9f;
	FirstRocketLoadTime = 0.4f;
	CurrentRocketFireMode = 0;
	bDrawRocketModeString = false;
	FOVOffset = FVector(0.5f, 1.f, 1.f);

	bLockedOnTarget = false;
	LockCheckTime = 0.1f;
	LockRange = 16000.0f;
	LockAcquireTime = 1.1f;
	LockTolerance = 0.2f;
	LockedTarget = NULL;
	PendingLockedTarget = NULL;
	LastLockedOnTime = 0.0f;
	PendingLockedTargetTime = 0.0f;
	LastValidTargetTime = 0.0f;
	LockAim = 0.997f;
	bTargetLockingActive = true;
	LastTargetLockCheckTime = 0.0f;

	UnderReticlePadding = 50.0f;
	CrosshairScale = 0.5f;

	CrosshairRotationTime = 0.3f;
	CurrentRotation = 0.0f;

	BarrelRadius = 9.0f;

	GracePeriod = 0.75f;
	BurstInterval = 0.1f;
	FullLoadSpread = 9.f;

	BasePickupDesireability = 0.78f;
	BaseAISelectRating = 0.78f;
	FiringViewKickback = -50.f;

	KillStatsName = NAME_RocketKills;
	DeathStatsName = NAME_RocketDeaths;
}

void AUTWeap_RocketLauncher::Destroyed()
{
	Super::Destroyed();
	GetWorldTimerManager().ClearAllTimersForObject(this);
}

void AUTWeap_RocketLauncher::BeginLoadRocket()
{
	//Play the load animation. Speed of anim based on GetLoadTime()
	if (GetNetMode() != NM_DedicatedServer)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL && LoadingAnimation.IsValidIndex(NumLoadedRockets) && LoadingAnimation[NumLoadedRockets] != NULL)
		{
			AnimInstance->Montage_Play(LoadingAnimation[NumLoadedRockets], LoadingAnimation[NumLoadedRockets]->SequenceLength / GetLoadTime(NumLoadedRockets));
		}
	}

	//Replicate the loading sound to other players 
	//Local players will use the sounds synced to the animation
	AUTPlayerController* PC = Cast<AUTPlayerController>(UTOwner->Controller);
	if (PC != NULL && !PC->IsLocalPlayerController())
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), LoadingSounds[NumLoadedRockets], UTOwner, SRT_AllButOwner, false, FVector::ZeroVector, NULL);
	}
}

void AUTWeap_RocketLauncher::EndLoadRocket()
{
	ConsumeAmmo(CurrentFireMode);
	NumLoadedRockets++;
	SetRocketFlashExtra(CurrentFireMode, NumLoadedRockets + 1, CurrentRocketFireMode, bDrawRocketModeString);
	LastLoadTime = GetWorld()->TimeSeconds;

	UUTGameplayStatics::UTPlaySound(GetWorld(), RocketLoadedSound, UTOwner, SRT_AllButOwner);

	// bot maybe shoots rockets from here
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B != NULL && B->GetTarget() != NULL && B->LineOfSightTo(B->GetTarget()) && !B->NeedToTurn(B->GetFocalPoint()))
	{
		if (NumLoadedRockets == MaxLoadedRockets)
		{
			UTOwner->StopFiring();
		}
		else if (NumLoadedRockets > 1)
		{
			if (B->GetTarget() != B->GetEnemy())
			{
				if (FMath::FRand() < 0.5f)
				{
					UTOwner->StopFiring();
				}
			}
			else if (FMath::FRand() < 0.3f)
			{
				UTOwner->StopFiring();
			}
			else if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
			{
				// if rockets would do more than 2x target's health, worth trying for the kill now
				AUTCharacter* P = Cast<AUTCharacter>(B->GetEnemy());
				if (P != NULL && P->HealthMax * B->GetEnemyInfo(B->GetEnemy(), true)->EffectiveHealthPct * 2.0f < ProjClass[CurrentFireMode].GetDefaultObject()->DamageParams.BaseDamage * NumLoadedRockets)
				{
					UTOwner->StopFiring();
				}
			}
		}
	}
}

void AUTWeap_RocketLauncher::ClearLoadedRockets()
{
	CurrentRocketFireMode = 0;
	NumLoadedRockets = 0;
	if (UTOwner != NULL)
	{
		UTOwner->SetFlashExtra(0, CurrentFireMode);
	}
	bDrawRocketModeString = false;
	LastLoadTime = 0.0f;
}

void AUTWeap_RocketLauncher::ClientAbortLoad_Implementation()
{
	UUTWeaponStateFiringChargedRocket* LoadState = Cast<UUTWeaponStateFiringChargedRocket>(CurrentState);
	if (LoadState != NULL)
	{
		// abort loading anim if it was playing
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL && LoadingAnimation.IsValidIndex(NumLoadedRockets) && LoadingAnimation[NumLoadedRockets] != NULL)
		{
			AnimInstance->Montage_SetPlayRate(LoadingAnimation[NumLoadedRockets], 0.0f);
		}
		// set grace timer
		float AdjustedGraceTime = GracePeriod;
		if (UTOwner != NULL && UTOwner->PlayerState != NULL)
		{
			AdjustedGraceTime = FMath::Max<float>(0.01f, AdjustedGraceTime - UTOwner->PlayerState->ExactPing * 0.0005f); // one way trip so half ping
		}
		GetWorldTimerManager().SetTimer(LoadState->GraceTimerHandle, LoadState, &UUTWeaponStateFiringChargedRocket::GraceTimer, AdjustedGraceTime, false);
	}
}

float AUTWeap_RocketLauncher::GetLoadTime(int32 InNumLoadedRockets)
{
	return ((InNumLoadedRockets > 0) ? RocketLoadTime : FirstRocketLoadTime) / ((UTOwner != NULL) ? UTOwner->GetFireRateMultiplier() : 1.0f);
}

void AUTWeap_RocketLauncher::OnMultiPress_Implementation(uint8 OtherFireMode)
{
	if (CurrentFireMode == 1)
	{
		UUTWeaponStateFiringChargedRocket* AltState = Cast<UUTWeaponStateFiringChargedRocket>(FiringState[1]);
		if (AltState != NULL && AltState->bCharging)
		{
			if (GetWorldTimerManager().IsTimerActive(AltState->GraceTimerHandle)
				&& (GetWorldTimerManager().GetTimerRemaining(AltState->GraceTimerHandle) < 0.05f))
			{
				// too close to timer ending, so don't allow mode change to avoid de-synchronizing client and server
				return;
			}
			CurrentRocketFireMode++;
			bDrawRocketModeString = true;

			if (CurrentRocketFireMode >= RocketFireModes.Num())
			{
				CurrentRocketFireMode = 0;
			}
			UUTGameplayStatics::UTPlaySound(GetWorld(), AltFireModeChangeSound, UTOwner, SRT_AllButOwner);

			//Update Extraflash so spectators can see the hud text
			if (Role == ROLE_Authority)
			{
				SetRocketFlashExtra(CurrentFireMode, NumLoadedRockets + 1, CurrentRocketFireMode, bDrawRocketModeString);
			}
		}
	}
}

bool AUTWeap_RocketLauncher::ShouldFireLoad()
{
	return !UTOwner || UTOwner->IsPendingKillPending() || (UTOwner->Health <= 0) || UTOwner->IsRagdoll();
}

void AUTWeap_RocketLauncher::FireShot()
{
	if (UTOwner)
	{
		UTOwner->DeactivateSpawnProtection();
	}
	//Alternate fire already consumed ammo
	if (CurrentFireMode != 1)
	{
		ConsumeAmmo(CurrentFireMode);
	}

	if (!FireShotOverride())
	{
		AUTProj_Rocket* NewRocket = Cast<AUTProj_Rocket>(FireProjectile());
		if (NewRocket && !NewRocket->IsPendingKillPending() && (Role == ROLE_Authority))
		{
			if (bIsFiringLeadRocket)
			{
				LeadRocket = NewRocket;
			}
			else if (LeadRocket && !LeadRocket->IsPendingKillPending())
			{
				LeadRocket->FollowerRockets.Add(NewRocket);
			}
		}
		bIsFiringLeadRocket = false;
		PlayFiringEffects();
		if (NumLoadedRockets <= 0)
		{
			ClearLoadedRockets();
			SetLockTarget(NULL);
		}
	}

	if (GetUTOwner() != NULL)
	{
		GetUTOwner()->InventoryEvent(InventoryEventName::FiredWeapon);
	}
}

AUTProjectile* AUTWeap_RocketLauncher::FireProjectile()
{
	if (GetUTOwner() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"), *GetName());
		return NULL;
	}

	UTOwner->SetFlashExtra(0, CurrentFireMode);

	//For the alternate fire, the number of flashes are replicated by the FireMode. 
	if (CurrentFireMode == 1)
	{
		// Bots choose mode now
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL)
		{
			if (B->GetTarget() == B->GetEnemy())
			{
				// when retreating, we want grenades
				if (B->GetEnemy() == NULL || B->LostContact(1.0f) /*|| B.IsRetreating() || B.IsInState('StakeOut')*/)
				{
					CurrentRocketFireMode = 1;
				}
				else
				{
					CurrentRocketFireMode = 0;
				}
			}
			else
			{
				CurrentRocketFireMode = 1;
			}
		}

		//Only play Muzzle flashes if the Rocket mode permits ie: Grenades no flash
		if ((Role == ROLE_Authority)/* && RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].bCauseMuzzleFlash*/)
		{
			UTOwner->IncrementFlashCount(NumLoadedRockets);
		}
			
		return FireRocketProjectile();
	}
	else
	{
		checkSlow(ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL);
		if (Role == ROLE_Authority)
		{
			UTOwner->IncrementFlashCount(CurrentFireMode);
		}

		FVector SpawnLocation = GetFireStartLoc();
		FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);

		//Adjust from the center of the gun to the barrel
		{
			FVector AdjustedSpawnLoc = SpawnLocation + FRotationMatrix(SpawnRotation).GetUnitAxis(EAxis::Z) * BarrelRadius; //Adjust rocket based on barrel size
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, SpawnLocation, AdjustedSpawnLoc, COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_None, true, UTOwner)))
			{
				SpawnLocation = Hit.Location - (AdjustedSpawnLoc - SpawnLocation).GetSafeNormal();
			}
			else
			{
				SpawnLocation = AdjustedSpawnLoc;
			}
		}

		AUTProjectile* SpawnedProjectile = SpawnNetPredictedProjectile(RocketFireModes[CurrentRocketFireMode].ProjClass, SpawnLocation, SpawnRotation);
		if (HasLockedTarget() &&  Cast<AUTProj_Rocket>(SpawnedProjectile))
		{
			Cast<AUTProj_Rocket>(SpawnedProjectile)->TargetActor = LockedTarget;
		}
		NumLoadedRockets = 0;
		return SpawnedProjectile;
	}
}

void AUTWeap_RocketLauncher::PlayFiringEffects()
{
	if (CurrentFireMode == 1 && UTOwner != NULL)
	{
		UTOwner->TargetEyeOffset.X = FiringViewKickback;

		// try and play the sound if specified
		if (RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].FireSound != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), RocketFireModes[CurrentRocketFireMode].FireSound, UTOwner, SRT_AllButOwner);
		}

		if (ShouldPlay1PVisuals())
		{
			// try and play a firing animation if specified
			if (FiringAnimation.IsValidIndex(NumLoadedRockets) && FiringAnimation[NumLoadedRockets] != NULL)
			{
				UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
				if (AnimInstance != NULL)
				{
					AnimInstance->Montage_Play(FiringAnimation[NumLoadedRockets], 1.f);
				}
			}

			//muzzle flash for each loaded rocket
			if (RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].bCauseMuzzleFlash)
			{
				for (int32 i = 0; i < NumLoadedRockets; i++)
				{
					if (MuzzleFlash.IsValidIndex(i) && MuzzleFlash[i] != NULL && MuzzleFlash[i]->Template != NULL)
					{
						if (!MuzzleFlash[i]->bIsActive || MuzzleFlash[i]->Template->Emitters[0] == NULL ||
							IsLoopingParticleSystem(MuzzleFlash[i]->Template))
						{
							MuzzleFlash[i]->ActivateSystem();
						}
					}
				}
			}
		}
	}
	else
	{
		Super::PlayFiringEffects();
	}
}

void AUTWeap_RocketLauncher::SetLeadRocket()
{
	bIsFiringLeadRocket = true;
}

AUTProjectile* AUTWeap_RocketLauncher::FireRocketProjectile()
{
	checkSlow(RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].ProjClass != NULL);

	//List of the rockets fired. If they are seeking rockets, set the target at the end
	TArray< AUTProjectile*, TInlineAllocator<3> > SeekerList;

	TSubclassOf<AUTProjectile> RocketProjClass = RocketFireModes[CurrentRocketFireMode].ProjClass;

	const FVector SpawnLocation = GetFireStartLoc();
	FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
	FRotationMatrix SpawnRotMat(SpawnRotation);

	FActorSpawnParameters Params;
	Params.Instigator = UTOwner;
	Params.Owner = UTOwner;

	AUTProjectile* ResultProj = NULL;

	switch (CurrentRocketFireMode)
	{
		case 0://rockets
		{
			float RotDegree = 360.0f / FMath::Max(1,NumLoadedRockets);
			FVector SpreadLoc = SpawnLocation;
			SpawnRotation.Roll = RotDegree * NumLoadedRockets;
			if (ShouldFireLoad())
			{
				SpawnRotation.Yaw += FullLoadSpread*float(NumLoadedRockets-2.f);
			}
			NetSynchRandomSeed(); 
			SeekerList.Add(SpawnNetPredictedProjectile(RocketProjClass, SpawnLocation, SpawnRotation));
			ResultProj = SeekerList[0];

			//Setup the seeking target
			if (HasLockedTarget() && Cast<AUTProj_Rocket>(ResultProj))
			{
				Cast<AUTProj_Rocket>(ResultProj)->TargetActor = LockedTarget;
			}

			break;
		}
		case 1://Grenades
		{
			float GrenadeSpread = GetSpread(0);
			float RotDegree = 360.0f / MaxLoadedRockets;
			SpawnRotation.Roll = RotDegree * MaxLoadedRockets;
			FRotator SpreadRot = SpawnRotation;
			SpreadRot.Yaw += GrenadeSpread*float(MaxLoadedRockets) - GrenadeSpread;
				
			AUTProjectile* SpawnedProjectile = SpawnNetPredictedProjectile(RocketFireModes[CurrentRocketFireMode].ProjClass, SpawnLocation, SpreadRot);
				
			if (SpawnedProjectile != nullptr)
			{
				//Spread the TossZ
				SpawnedProjectile->ProjectileMovement->Velocity.Z += (MaxLoadedRockets % 2) * GetSpread(2);
			}

			ResultProj = SpawnedProjectile;
			break;
		}
		default:
			UE_LOG(UT, Warning, TEXT("%s::FireRocketProjectile(): Invalid CurrentRocketFireMode"), *GetName());
			break;
		}

	NumLoadedRockets--;
	return ResultProj;
}


float AUTWeap_RocketLauncher::GetSpread(int32 ModeIndex)
{
	if (RocketFireModes.IsValidIndex(ModeIndex))
	{
		return RocketFireModes[ModeIndex].Spread;
	}
	return 0.0f;
}


// Target Locking Code
void AUTWeap_RocketLauncher::StateChanged()
{
	if (Role == ROLE_Authority && CurrentState != InactiveState && CurrentState != EquippingState && CurrentState != UnequippingState)
	{
		GetWorldTimerManager().SetTimer(UpdateLockHandle, this, &AUTWeap_RocketLauncher::UpdateLock, LockCheckTime, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(UpdateLockHandle);
	}

	//Clear loaded rockets and hide the HUD text when inactive
	if (CurrentState == InactiveState)
	{
		ClearLoadedRockets();
	}
}

bool AUTWeap_RocketLauncher::CanLockTarget(AActor *Target)
{
	//Make sure its not dead
	if (Target != NULL && !Target->bTearOff && !bPendingKillPending)
	{
		AUTCharacter* UTP = Cast<AUTCharacter>(Target);

		//not same team
		return (UTP != NULL && (UTP->GetTeamNum() == 255 || UTP->GetTeamNum() != UTOwner->GetTeamNum()));
	}
	else
	{
		return false;
	}
}

bool AUTWeap_RocketLauncher::WithinLockAim(AActor *Target)
{
	if (CanLockTarget(Target))
	{
		const FVector Dir = GetAdjustedAim(GetFireStartLoc()).Vector();
		const FVector TargetDir = (Target->GetActorLocation() - UTOwner->GetActorLocation()).GetSafeNormal();
		// note that we're not tracing to retain existing target; allows locking through walls to a limited extent
		return (FVector::DotProduct(Dir, TargetDir) > LockAim || UUTGameplayStatics::PickBestAimTarget(UTOwner->Controller, GetFireStartLoc(), Dir, LockAim, LockRange, AUTCharacter::StaticClass()) == Target);
	}
	else
	{
		return false;
	}
}

void AUTWeap_RocketLauncher::OnRep_LockedTarget()
{
	SetLockTarget(LockedTarget);
}

void AUTWeap_RocketLauncher::SetLockTarget(AActor* NewTarget)
{
	LockedTarget = NewTarget;
	if (LockedTarget != NULL)
	{
		if (!bLockedOnTarget)
		{
			bLockedOnTarget = true;
			LastLockedOnTime = GetWorld()->TimeSeconds;
			if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL && UTOwner->IsLocallyControlled())
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), LockAcquiredSound, UTOwner, SRT_None);
			}
		}
	}
	else
	{
		if (bLockedOnTarget)
		{
			bLockedOnTarget = false;
			if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL && UTOwner->IsLocallyControlled())
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), LockLostSound, UTOwner, SRT_None);
			}
		}
	}
}

void AUTWeap_RocketLauncher::UpdateLock()
{
	if (UTOwner == NULL || UTOwner->Controller == NULL || UTOwner->IsFiringDisabled())
	{
		SetLockTarget(NULL);
		return;
	}

	//Have a target. Update the target lock
	if (LockedTarget != NULL)
	{
		//Still Valid LockTarget
		if (WithinLockAim(LockedTarget))
		{
			LastLockedOnTime = GetWorld()->TimeSeconds;
		}
		//Lost the Target
		else if ((LockTolerance + LastLockedOnTime) < GetWorld()->TimeSeconds)
		{
			SetLockTarget(NULL);
		}
	}
	//Have a pending target
	else if (PendingLockedTarget != NULL)
	{
		bool bAimingAt = WithinLockAim(PendingLockedTarget);
		//If we are looking at the target and its time to lock on
		if (bAimingAt && (PendingLockedTargetTime + LockAcquireTime) < GetWorld()->TimeSeconds)
		{
			SetLockTarget(PendingLockedTarget);
			PendingLockedTarget = NULL;
		}
		//Lost the pending target
		else if (!bAimingAt)
		{
			PendingLockedTarget = NULL;
		}
	}
	//Trace to see if we are looking at a new target
	else
	{
		AActor* NewTarget = UUTGameplayStatics::PickBestAimTarget(UTOwner->Controller, GetFireStartLoc(), GetAdjustedAim(GetFireStartLoc()).Vector(), LockAim, LockRange, AUTCharacter::StaticClass());
		if (NewTarget != NULL)
		{
			PendingLockedTarget = NewTarget;
			PendingLockedTargetTime = GetWorld()->TimeSeconds;
		}
	}
}

void AUTWeap_RocketLauncher::DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	//Draw the Rocket Firemode Text
	if (bDrawRocketModeString && RocketModeFont != NULL)
	{
		FText RocketModeText = RocketFireModes[CurrentRocketFireMode].DisplayString;
		float PosY = WeaponHudWidget->GetRenderScale() * UnderReticlePadding;

		WeaponHudWidget->DrawText(RocketModeText, 0.0f, PosY, RocketModeFont, FLinearColor::Black, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Top);
	}
	
	//Draw the crosshair
	//TODO TIM: Move this over to a custom UTCrosshair
	Super::DrawWeaponCrosshair_Implementation(WeaponHudWidget, RenderDelta);
	/*if (LoadCrosshairTextures.IsValidIndex(NumLoadedRockets) && LoadCrosshairTextures[NumLoadedRockets] != NULL)
	{
		UTexture2D* Tex = LoadCrosshairTextures[NumLoadedRockets];
		float W = Tex->GetSurfaceWidth();
		float H = Tex->GetSurfaceHeight();
		float Scale = WeaponHudWidget->GetRenderScale() * CrosshairScale * GetCrosshairScale(WeaponHudWidget->UTHUDOwner);

		float DegreesPerRocket = 360.0f / MaxLoadedRockets;
		float CrosshairRot = 0;

		if (NumLoadedRockets < MaxLoadedRockets)
		{
			float DeltaTime = GetWorld()->TimeSeconds - LastLoadTime;
			float Alpha = FMath::Clamp(DeltaTime / CrosshairRotationTime, 0.0f, 1.0f);
			CrosshairRot = FMath::Lerp(0.f, DegreesPerRocket, Alpha);
		}

		WeaponHudWidget->DrawTexture(Tex, 0, 0, W * Scale, H * Scale, 0.0, 0.0, W, H, 1.0, GetCrosshairColor(WeaponHudWidget), FVector2D(0.5f, 0.5f), CrosshairRot);
		AUTPlayerState* PS;
		if (ShouldDrawFFIndicator(WeaponHudWidget->UTHUDOwner->PlayerOwner, PS))
		{
			WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->DefaultCrosshairTex, 0, 0, W * Scale * 0.75f, H * Scale * 0.75f, 0.0, 0.0, 16, 16, 1.0, FLinearColor::Green, FVector2D(0.5f, 0.5f), 45.0f);
		}
		else
		{
			UpdateCrosshairTarget(PS, WeaponHudWidget, RenderDelta);
		}
	}*/

	//Draw the locked on crosshair
	if (HasLockedTarget())
	{
		UTexture2D* Tex = LockCrosshairTexture;
		float W = Tex->GetSurfaceWidth();
		float H = Tex->GetSurfaceHeight();
		float Scale = WeaponHudWidget->GetRenderScale() * GetCrosshairScale(WeaponHudWidget->UTHUDOwner);

		FVector ScreenTarget = WeaponHudWidget->GetCanvas()->Project(LockedTarget->GetActorLocation());
		ScreenTarget.X -= WeaponHudWidget->GetCanvas()->SizeX*0.5f;
		ScreenTarget.Y -= WeaponHudWidget->GetCanvas()->SizeY*0.5f;

		float CrosshairRot = GetWorld()->TimeSeconds * 90.0f;

		WeaponHudWidget->DrawTexture(Tex, ScreenTarget.X, ScreenTarget.Y, W * Scale, H * Scale, 0.0, 0.0, W, H, 1.0, FLinearColor::Red, FVector2D(0.5f, 0.5f), CrosshairRot);
	}
}

void AUTWeap_RocketLauncher::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeap_RocketLauncher, LockedTarget, COND_None);
}

float AUTWeap_RocketLauncher::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL || B->GetEnemy() == NULL)
	{
		return BaseAISelectRating;
	}
	// if standing on a lift, make sure not about to go around a corner and lose sight of target
	// (don't want to blow up a rocket in bot's face)
	else if (UTOwner->GetMovementBase() != NULL && !UTOwner->GetMovementBase()->GetComponentVelocity().IsZero() && !B->CheckFutureSight(0.1f))
	{
		return 0.1f;
	}
	else
	{
		const FVector EnemyDir = B->GetEnemyLocation(B->GetEnemy(), false) - UTOwner->GetActorLocation();
		float EnemyDist = EnemyDir.Size();
		float Rating = BaseAISelectRating;

		// don't pick rocket launcher if enemy is too close
		if (EnemyDist < 800.0f)
		{
			// don't switch away from rocket launcher unless really bad tactical situation
			// TODO: also don't if OK with mutual death (high aggressiveness, high target priority, or grudge against target?)
			if (UTOwner->GetWeapon() == this && (EnemyDist > 550.0f || (UTOwner->Health < 50 && UTOwner->GetEffectiveHealthPct(false) < B->GetEnemyInfo(B->GetEnemy(), true)->EffectiveHealthPct)))
			{
				return Rating;
			}
			else
			{
				return 0.05f + EnemyDist * 0.0005;
			}
		}

		// rockets are good if higher than target, bad if lower than target
		float ZDiff = EnemyDir.Z;
		if (ZDiff < -250.0f)
		{
			Rating += 0.25;
		}
		else if (ZDiff > 350.0f)
		{
			Rating -= 0.35;
		}
		else if (ZDiff > 175.0f)
		{
			Rating -= 0.05;
		}

		// slightly higher chance to use against melee because high rocket momentum will keep enemy away
		AUTCharacter* EnemyChar = Cast<AUTCharacter>(B->GetEnemy());
		if (EnemyChar != NULL && EnemyChar->GetWeapon() != NULL && EnemyChar->GetWeapon()->bMeleeWeapon && EnemyDist < 5500.0f)
		{
			Rating += 0.1f;
		}

		return Rating;
	}
}

float AUTWeap_RocketLauncher::SuggestAttackStyle_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B != NULL && B->GetEnemy() != NULL)
	{
		// recommend backing off if target is too close
		float EnemyDist = (B->GetEnemyLocation(B->GetEnemy(), false) - UTOwner->GetActorLocation()).Size();
		if (EnemyDist < 1600.0f)
		{
			return (EnemyDist < 1100.0f) ? -1.5f : -0.7f;
		}
		else if (EnemyDist > 3500.0f)
		{
			return 0.5f;
		}
		else
		{
			return -0.1f;
		}
	}
	else
	{
		return -0.1f;
	}
}

bool AUTWeap_RocketLauncher::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc))
	{
		if (!bPreferCurrentMode && B != NULL)
		{
			// prefer single rocket for visible enemy unless enemy is much stronger and want to try bursting
			BestFireMode = (FMath::FRand() < 0.3f || B->GetTarget() != B->GetEnemy() || (!B->IsStopped() && B->RelativeStrength(B->GetEnemy()) <= 0.5f)) ? 0 : 1;
		}
		return true;
	}
	else if (B == NULL)
	{
		return false;
	}
	else
	{
		if (GetWorld()->TimeSeconds - LastAttackSkillCheckTime < 1.0f)
		{
			LastAttackSkillCheckTime = GetWorld()->TimeSeconds;
			bAttackSkillCheckResult = B->WeaponProficiencyCheck() && FMath::FRand() > 0.5f;
		}
		if (!bAttackSkillCheckResult || B->GetTarget() != B->GetEnemy() || B->IsCharging() || (!IsPreparingAttack() && B->LostContact(3.0f))/* || !B->IsHunting()*/)
		{
			if (IsPreparingAttack() && bAttackSkillCheckResult)
			{
				BestFireMode = 1;
				if (NumLoadedRockets < MaxLoadedRockets - 1)
				{
					// don't modify aim yet, not close enough to firing
					return true;
				}
				else
				{
					// TODO: if high skill, look around for someplace to shoot rockets that won't blow self up
					return true;
				}
			}
			return false;
		}
		else
		{
			// consider firing standard rocket in case enemy is coming around corner soon
			// note: repeat of skill check is intentional
			if (!bPreferCurrentMode)
			{
				BestFireMode = (!B->LostContact(1.5f) && B->WeaponProficiencyCheck() && FMath::FRand() < 0.5f) ? 0 : 1;
			}

			if (!PredicitiveTargetLoc.IsZero() && !GetWorld()->LineTraceTestByChannel(UTOwner->GetActorLocation(), PredicitiveTargetLoc, ECC_Visibility, FCollisionQueryParams(FName(TEXT("PredictiveRocket")), false, UTOwner), WorldResponseParams))
			{
				OptimalTargetLoc = PredicitiveTargetLoc;
				return true;
			}
			else
			{
				TArray<FVector> FoundPoints;
				B->GuessAppearancePoints(Target, TargetLoc, true, FoundPoints);
				if (FoundPoints.Num() == 0)
				{
					return false;
				}
				else
				{
					PredicitiveTargetLoc = FoundPoints[FMath::RandHelper(FoundPoints.Num())];
					OptimalTargetLoc = PredicitiveTargetLoc;
					return true;
				}
			}
		}
	}
}

bool AUTWeap_RocketLauncher::IsPreparingAttack_Implementation()
{
	if (GracePeriod <= 0.0f)
	{
		return false;
	}
	else
	{
		// rocket launcher charge doesn't hold forever, so AI should make sure to fire before doing something else
		UUTWeaponStateFiringCharged* ChargeState = Cast<UUTWeaponStateFiringCharged>(CurrentState);
		return (ChargeState != NULL && ChargeState->bCharging);
	}
}

void AUTWeap_RocketLauncher::FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode)
{
	if (InFireMode == 1)
	{
		int32 NewNumLoadedRockets;
		GetRocketFlashExtra(NewFlashExtra, InFireMode, NewNumLoadedRockets, CurrentRocketFireMode, bDrawRocketModeString);
		
		//Play the load animation
		if (NewNumLoadedRockets != NumLoadedRockets && GetNetMode() != NM_DedicatedServer)
		{
			NumLoadedRockets = NewNumLoadedRockets;

			if (NumLoadedRockets > 0)
			{
				UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
				if (AnimInstance != NULL && LoadingAnimation.IsValidIndex(NumLoadedRockets - 1) && LoadingAnimation[NumLoadedRockets - 1] != NULL)
				{
					AnimInstance->Montage_Play(LoadingAnimation[NumLoadedRockets - 1], LoadingAnimation[NumLoadedRockets - 1]->SequenceLength / GetLoadTime(NumLoadedRockets - 1));
				}
			}
		}
	}
}

void AUTWeap_RocketLauncher::SetRocketFlashExtra(uint8 InFireMode, int32 InNumLoadedRockets, int32 InCurrentRocketFireMode, bool bInDrawRocketModeString)
{
	if (UTOwner != nullptr && Role == ROLE_Authority)
	{
		if (InFireMode == 0)
		{
			GetUTOwner()->SetFlashExtra(0, InFireMode);
		}
		else
		{
			uint8 NewFlashExtra = InNumLoadedRockets;
			if (bInDrawRocketModeString)
			{
				NewFlashExtra |= 1 << 7;
				NewFlashExtra |= InCurrentRocketFireMode << 4;
			}
			GetUTOwner()->SetFlashExtra(NewFlashExtra, InFireMode);
		}
	}
}

void AUTWeap_RocketLauncher::GetRocketFlashExtra(uint8 InFlashExtra, uint8 InFireMode, int32& OutNumLoadedRockets, int32& OutCurrentRocketFireMode, bool& bOutDrawRocketModeString)
{
	if (InFireMode == 1)
	{
		//High bit is whether or not we should display the the Rocket mode string
		if (InFlashExtra >> 7 > 0) 
		{
			bOutDrawRocketModeString = true;
			OutCurrentRocketFireMode = (InFlashExtra >> 4) & 0x07; //The next 3 bits is the rocket mode
		}
		//Low 4 bits is the number of rockets
		OutNumLoadedRockets = FMath::Min(InFlashExtra & 0x0F, MaxLoadedRockets);
	}
}

void AUTWeap_RocketLauncher::FiringInfoUpdated_Implementation(uint8 InFireMode, uint8 FlashCount, FVector InFlashLocation)
{
	Super::FiringInfoUpdated_Implementation(InFireMode, FlashCount, InFlashLocation);
	CurrentRocketFireMode = 0;
	bDrawRocketModeString = false;
	NumLoadedRockets = 0;
}