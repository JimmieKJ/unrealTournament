// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeap_RocketLauncher.h"
#include "UTProj_RocketSpiral.h"
#include "UTWeaponStateFiringChargedRocket.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealNetwork.h"

AUTWeap_RocketLauncher::AUTWeap_RocketLauncher(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringChargedRocket>(TEXT("FiringState1")))
{
	if (FiringState.Num() > 1)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[1] = UUTWeaponStateFiringChargedRocket::StaticClass();
#endif
	}

	NumLoadedRockets = 0;
	MaxLoadedRockets = 3;
	RocketLoadTime = 0.9f;
	FirstRocketLoadTime = 0.4f;
	CurrentRocketFireMode = 0;
	bDrawRocketModeString = false;

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
	CrosshairScale = 1.2f;

	CrosshairRotationTime = 0.3f;
	CurrentRotation = 0.0f;

	BarrelRadius = 9.0f;

	GracePeriod = 1.0f;
}

void AUTWeap_RocketLauncher::BeginLoadRocket()
{
	//Play the load animation. Speed of anim based on GetLoadTime()
	if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL)
	{
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (AnimInstance != NULL && LoadingAnimation.IsValidIndex(NumLoadedRockets) && LoadingAnimation[NumLoadedRockets] != NULL)
		{
			AnimInstance->Montage_Play(LoadingAnimation[NumLoadedRockets], LoadingAnimation[NumLoadedRockets]->SequenceLength / GetLoadTime());
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
	LastLoadTime = GetWorld()->TimeSeconds;

	UUTGameplayStatics::UTPlaySound(GetWorld(), RocketLoadedSound, UTOwner, SRT_AllButOwner);
}

void AUTWeap_RocketLauncher::ClearLoadedRockets()
{
	CurrentRocketFireMode = 0;
	NumLoadedRockets = 0;
	bDrawRocketModeString = false;
	LastLoadTime = 0.0f;
}

float AUTWeap_RocketLauncher::GetLoadTime()
{
	return ((NumLoadedRockets > 0) ? RocketLoadTime : FirstRocketLoadTime) * ((UTOwner != NULL) ? UTOwner->GetFireRateMultiplier() : 1.0f);
}


void AUTWeap_RocketLauncher::OnMultiPress_Implementation(uint8 OtherFireMode)
{
	if (CurrentFireMode == 1)
	{
		UUTWeaponStateFiringChargedRocket* AltState = Cast<UUTWeaponStateFiringChargedRocket>(FiringState[1]);
		if (AltState != NULL && AltState->bCharging)
		{
			CurrentRocketFireMode++;
			bDrawRocketModeString = true;

			if (CurrentRocketFireMode >= (uint32)RocketFireModes.Num())
			{
				CurrentRocketFireMode = 0;
			}

			UUTGameplayStatics::UTPlaySound(GetWorld(), AltFireModeChangeSound, UTOwner, SRT_AllButOwner);
		}
	}
}

TSubclassOf<AUTProjectile> AUTWeap_RocketLauncher::GetRocketProjectile()
{
	if (CurrentFireMode == 0 && ProjClass.IsValidIndex(0) && ProjClass[0] != NULL)
	{
		return HasLockedTarget() ? TSubclassOf<AUTProjectile>(SeekingProjClass) : ProjClass[0];
	}
	else if (CurrentFireMode == 1)
	{
		//If we are locked on and this rocket mode can shoot seeking rockets
		return (HasLockedTarget() && RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].bCanBeSeekingRocket) ? TSubclassOf<AUTProjectile>(SeekingProjClass) : RocketFireModes[CurrentRocketFireMode].ProjClass;
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("%s::GetRocketProjectile(): NULL Projectile returned"));
		return NULL;
	}
}

void AUTWeap_RocketLauncher::FireShot()
{
	UTOwner->DeactivateSpawnProtection();

	//Alternate fire already consumed ammo
	if (CurrentFireMode != 1)
	{
		ConsumeAmmo(CurrentFireMode);
	}

	if (!FireShotOverride())
	{
		FireProjectile();
		PlayFiringEffects();
		ClearLoadedRockets();
		SetLockTarget(NULL);
	}

	if (GetUTOwner() != NULL)
	{
		static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
		GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
	}
}

AUTProjectile* AUTWeap_RocketLauncher::FireProjectile()
{
	if (GetUTOwner() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"));
		return NULL;
	}
	else if (Role == ROLE_Authority)
	{
		//For the alternate fire, the number of flashes are replicated by the FireMode. 
		if (CurrentFireMode == 1)
		{
			//Only play Muzzle flashes if the Rocket mode permits ie: Grenades no flash
			if (RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].bCauseMuzzleFlash)
			{
				UTOwner->IncrementFlashCount(NumLoadedRockets);
			}
			
			return FireRocketProjectile();
		}
		else
		{
			checkSlow(ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL);
			UTOwner->IncrementFlashCount(CurrentFireMode);

			FVector SpawnLocation = GetFireStartLoc();
			FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);

			//Adjust from the center of the gun to the barrel
			SpawnLocation += FRotationMatrix(SpawnRotation).GetUnitAxis(EAxis::Z) * BarrelRadius; //Adjust rocket based on barrel size

			FActorSpawnParameters Params;
			Params.Instigator = UTOwner;
			Params.Owner = UTOwner;

			AUTProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AUTProjectile>(GetRocketProjectile(), SpawnLocation, SpawnRotation, Params);

			if (Cast<AUTProj_RocketSeeking>(SpawnedProjectile) != NULL)
			{
				Cast<AUTProj_RocketSeeking>(SpawnedProjectile)->TargetActor = LockedTarget;
			}
			return SpawnedProjectile;
		}
	}
	else
	{
		return NULL;
	}
}

void AUTWeap_RocketLauncher::PlayFiringEffects()
{
	if (CurrentFireMode == 1 && GetNetMode() != NM_DedicatedServer && UTOwner != NULL)
	{
		// try and play the sound if specified
		if (RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].FireSound != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), RocketFireModes[CurrentRocketFireMode].FireSound, UTOwner, SRT_AllButOwner);
		}

		// try and play a firing animation if specified
		if (FiringAnimation.IsValidIndex(NumLoadedRockets - 1) && FiringAnimation[NumLoadedRockets - 1] != NULL)
		{
			UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(FiringAnimation[NumLoadedRockets - 1], 1.f);
			}
		}

		//muzzle flash for each loaded rocket
		if (RocketFireModes.IsValidIndex(CurrentRocketFireMode) && RocketFireModes[CurrentRocketFireMode].bCauseMuzzleFlash)
		{
			for (uint32 i = 0; i < NumLoadedRockets; i++)
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
	else
	{
		Super::PlayFiringEffects();
	}
}

AUTProjectile* AUTWeap_RocketLauncher::FireRocketProjectile()
{
	checkSlow(RocketFireModes.IsValidIndex(CurrentRocketFireMode) && GetRocketProjectile() != NULL);

	//List of the rockets fired. If they are seeking rockets, set the target at the end
	TArray< AUTProjectile*, TInlineAllocator<3> > SeekerList;

	TSubclassOf<AUTProjectile> RocketProjClass = GetRocketProjectile();

	const FVector SpawnLocation = GetFireStartLoc();
	FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
	FRotationMatrix SpawnRotMat(SpawnRotation);

	FActorSpawnParameters Params;
	Params.Instigator = UTOwner;
	Params.Owner = UTOwner;

	AUTProjectile* ResultProj = NULL;

	switch (CurrentRocketFireMode)
	{
		case 0://spread
		{
			float StartSpread = (NumLoadedRockets - 1) * GetSpread() * -0.5;
			for (uint32 i = 0; i < NumLoadedRockets; i++)
			{
				FRotator SpreadRot = SpawnRotation + FRotator(0.0f, GetSpread() * i + StartSpread, 0.0f);
				SeekerList.Add(GetWorld()->SpawnActor<AUTProjectile>(RocketProjClass, SpawnLocation, SpreadRot, Params));
				if (i == 0)
				{
					ResultProj = SeekerList[0];
				}
			}
			break;
		}
		case 1://spiral
		{
			float RotDegree = 360.0f / NumLoadedRockets;
			for (uint32 i = 0; i < NumLoadedRockets; i++)
			{
				FVector SpreadLoc = SpawnLocation;
				SpawnRotation.Roll = RotDegree * i;

				//Spiral needs rockets centered but seeking needs to be at the BarrelRadius
				if (HasLockedTarget())
				{
					FVector Up = SpawnRotMat.GetUnitAxis(EAxis::Z);
					SpreadLoc = SpawnLocation + (Up * BarrelRadius);
				}
				else
				{
					SpreadLoc = SpawnLocation - 2.0f * ((FMath::Sin(i * 2.0f * PI / MaxLoadedRockets) * 8.0f - 7.0f) * SpawnRotMat.GetUnitAxis(EAxis::Y) - (FMath::Cos(i * 2.0f * PI / MaxLoadedRockets) * 8.0f - 7.0f) * SpawnRotMat.GetUnitAxis(EAxis::Z)) - SpawnRotMat.GetUnitAxis(EAxis::X) * 8.0f * FMath::FRand();
				}

				SeekerList.Add(GetWorld()->SpawnActor<AUTProjectile>(RocketProjClass, SpreadLoc, SpawnRotation, Params));
				if (i == 0)
				{
					ResultProj = SeekerList[0];
				}
			}
			// tell spiral rockets about each other
			for (int32 i = 0; i < SeekerList.Num(); i++)
			{
				AUTProj_RocketSpiral* Rocket = Cast<AUTProj_RocketSpiral>(SeekerList[i]);
				if (Rocket != NULL)
				{
					Rocket->bCurl = (i % 2 == 1);
					int32 FlockId = 0;
					for (int32 j = 0; j < SeekerList.Num() && FlockId < ARRAY_COUNT(Rocket->Flock); j++)
					{
						if (i != j)
						{
							Rocket->Flock[FlockId++] = Cast<AUTProj_RocketSpiral>(SeekerList[j]);
						}
					}
				}
			}
			break;
		}
		case 2://Grenade
		{
			float RotDegree = 360.0f / MaxLoadedRockets;
			for (uint32 i = 0; i < NumLoadedRockets; i++)
			{
				SpawnRotation.Roll = RotDegree * i;
				FVector Up = FRotationMatrix(SpawnRotation).GetUnitAxis(EAxis::Z);
				FVector SpreadLoc = SpawnLocation + (Up * BarrelRadius);
				AUTProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AUTProjectile>(RocketProjClass, SpreadLoc, SpawnRotation, Params);
				//Spread the TossZ
				SpawnedProjectile->ProjectileMovement->Velocity.Z += i * GetSpread();
				if (i == 0)
				{
					ResultProj = SpawnedProjectile;
				}
			}
			break;
		}
		default:
			UE_LOG(UT, Warning, TEXT("%s::FireRocketProjectile(): Invalid CurrentRocketFireMode"));
			break;
		}

	//Setup the seeking target
	if (HasLockedTarget())
	{
		for (AUTProjectile* Rocket : SeekerList)
		{
			if (Cast<AUTProj_RocketSeeking>(Rocket) != NULL)
			{
				((AUTProj_RocketSeeking*)Rocket)->TargetActor = LockedTarget;
			}
		}
	}
	return NULL;
}


// Target Locking Code


void AUTWeap_RocketLauncher::StateChanged()
{
	if (Role == ROLE_Authority && CurrentState != InactiveState && CurrentState != EquippingState && CurrentState != UnequippingState)
	{
		GetWorldTimerManager().SetTimer(this, &AUTWeap_RocketLauncher::UpdateLock, LockCheckTime, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(this, &AUTWeap_RocketLauncher::UpdateLock);
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
		const FVector TargetDir = (Target->GetActorLocation() - UTOwner->GetActorLocation()).SafeNormal();
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
			if (GetNetMode() != NM_DedicatedServer)
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
			if (GetNetMode() != NM_DedicatedServer)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), LockLostSound, UTOwner, SRT_None);
			}
		}
	}
}

void AUTWeap_RocketLauncher::UpdateLock()
{
	if (UTOwner == NULL)
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
	if (LoadCrosshairTextures.IsValidIndex(NumLoadedRockets) && LoadCrosshairTextures[NumLoadedRockets] != NULL)
	{
		UTexture2D* Tex = LoadCrosshairTextures[NumLoadedRockets];
		float W = Tex->GetSurfaceWidth();
		float H = Tex->GetSurfaceHeight();

		float Scale = WeaponHudWidget->GetRenderScale() * CrosshairScale;

		float DegreesPerRocket = 360.0f / MaxLoadedRockets;
		float CrosshairRot = DegreesPerRocket * ((NumLoadedRockets > 1) ? NumLoadedRockets : 0);

		if (NumLoadedRockets < MaxLoadedRockets)
		{
			float NextCrosshairRot = CrosshairRot + DegreesPerRocket;
			float DeltaTime = LastLoadTime + CrosshairRotationTime - GetWorld()->TimeSeconds;
			float Alpha = FMath::Clamp(DeltaTime / CrosshairRotationTime, 0.0f, 1.0f);
			CrosshairRot = FMath::Lerp(NextCrosshairRot, CrosshairRot, Alpha);
		}
		WeaponHudWidget->DrawTexture(Tex, 0, 0, W * Scale, H * Scale, 0.0, 0.0, W, H, 1.0, GetCrosshairColor(WeaponHudWidget), FVector2D(0.5f, 0.5f), CrosshairRot);
	}

	//Draw the locked on crosshair
	if (HasLockedTarget())
	{
		UTexture2D* Tex = LockCrosshairTexture;
		float W = Tex->GetSurfaceWidth();
		float H = Tex->GetSurfaceHeight();
		float Scale = WeaponHudWidget->GetRenderScale() * CrosshairScale;

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