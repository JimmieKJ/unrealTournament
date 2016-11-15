// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "UTProjectileMovementComponent.h"
#include "UTImpactEffect.h"
#include "UTRemoteRedeemer.h"
#include "UTCTFRewardMessage.h"
#include "UTHUD.h"
#include "StatNames.h"
#include "UTWorldSettings.h"
#include "UTProj_WeaponScreen.h"
#include "UTRedeemerLaunchAnnounce.h"

AUTRemoteRedeemer::AUTRemoteRedeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Use a sphere as a simple collision representation
	CollisionComp = ObjectInitializer.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	if (CollisionComp != NULL)
	{
		CollisionComp->InitSphereRadius(0.0f);
		CollisionComp->BodyInstance.SetCollisionProfileName("ProjectileShootable");			// Collision profiles are defined in DefaultEngine.ini
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTRemoteRedeemer::OnOverlapBegin);
		CollisionComp->bTraceComplexOnMove = true;
		CollisionComp->bGenerateOverlapEvents = false;
		RootComponent = CollisionComp;
	}

	CapsuleComp = ObjectInitializer.CreateOptionalDefaultSubobject<UCapsuleComponent>(this, TEXT("CapsuleComp"));
	if (CapsuleComp != NULL)
	{
		CapsuleComp->BodyInstance.SetCollisionProfileName("ProjectileShootable");			// Collision profiles are defined in DefaultEngine.ini
		CapsuleComp->OnComponentBeginOverlap.AddDynamic(this, &AUTRemoteRedeemer::OnOverlapBegin);
		CapsuleComp->bTraceComplexOnMove = true;
		CapsuleComp->InitCapsuleSize(16.f, 70.0f);
		CapsuleComp->SetRelativeRotation(FRotator(90.f, 90.f, 90.f));
		CapsuleComp->SetupAttachment(RootComponent);
	}

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = ObjectInitializer.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 1700.f;
	ProjectileMovement->MaxSpeed = 1700.f;
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AUTRemoteRedeemer::OnStop);

	SetReplicates(true);
	bNetTemporary = false;

	AccelRate = 3400.f;
	RedeemerMouseSensitivity = 700.0f;
	AccelerationBlend = 5.0f;
	MaximumRoll = 25.0f;
	RollMultiplier = 0.01f;
	RollSmoothingMultiplier = 5.0f;
	MaxPitch = 75.0f;
	MinPitch = -55.0f;

	StatsHitCredit = 0.f;
	HitsStatsName = NAME_RedeemerHits;
	ProjHealth = 50;
	LockCount = 0;
	CachedTeamNum = 255;
	MaxFuelTime = 20.f;
}

FVector AUTRemoteRedeemer::GetVelocity() const
{
	return ProjectileMovement->Velocity;
}

void AUTRemoteRedeemer::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	ProjectileMovement->Velocity = NewVelocity;
	// make sure if client thought there was a collision and stopped the projectile that we undo that state
	if (!NewVelocity.IsZero() && ProjectileMovement->UpdatedComponent == NULL)
	{
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
	}
}

void AUTRemoteRedeemer::BeginPlay()
{
	if (IsPendingKillPending())
	{
		// engine bug that we need to do this
		return;
	}
	Super::BeginPlay();
	CurrentFuelTime = MaxFuelTime;
}

bool AUTRemoteRedeemer::TryToDrive(APawn* NewDriver)
{
	return DriverEnter(NewDriver);
}

bool AUTRemoteRedeemer::DriverEnter(APawn* NewDriver)
{
	if (Role != ROLE_Authority)
	{
		return false;
	}
	if (Driver != nullptr)
	{
		DriverLeave(true);
	}

	if (NewDriver != nullptr)
	{
		CurrentFuelTime = MaxFuelTime;
		Driver = NewDriver;
		AController* C = NewDriver->Controller;
		if (C)
		{
			C->UnPossess();
			NewDriver->SetOwner(this);
			C->Possess(this);
			AUTCharacter *UTChar = Cast<AUTCharacter>(NewDriver);
			if (UTChar)
			{
				UTChar->StartDriving(this);
				UTChar->PlayerState = PlayerState;
			}
			DamageInstigator = C;
		}
	}

	return true;
}

bool AUTRemoteRedeemer::DriverLeave(bool bForceLeave)
{
	if (!bShotDown)
	{
		BlowUp();
	}

	AController* C = Controller;
	if (Driver && C)
	{
		if (C->PlayerState) 
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS && !GS->IsMatchIntermission() && !GS->HasMatchEnded())
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					AUTPlayerController* UTPC = Cast<AUTPlayerController>(It->PlayerController);
					if (UTPC && UTPC->LastSpectatedPlayerState == C->PlayerState)
					{
						UTPC->ViewPawn(Driver);
					}
				}
			}
		}
		C->UnPossess();
		Driver->SetOwner(C);
		AUTCharacter *UTChar = Cast<AUTCharacter>(Driver);
		if (UTChar)
		{
			UTChar->StopDriving(this);
			if (UTChar->GetWeapon())
			{
				UTChar->GetWeapon()->AddAmmo(0);
			}
		}
		C->Possess(Driver);
	}

	Driver = nullptr;
	return true;
}

void AUTRemoteRedeemer::OnStop(const FHitResult& Hit)
{
	if ((Role == ROLE_Authority) && !bShotDown)
	{
		BlowUp(Hit.ImpactNormal);
	}
}

void AUTRemoteRedeemer::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role == ROLE_Authority && Driver != OtherActor && !Cast<APhysicsVolume>(OtherActor))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(this, OtherActor))
		{
			AUTProjectile* Proj = Cast<AUTProjectile>(OtherActor);
			if (Proj != nullptr)
			{
				if (Cast<AUTProj_WeaponScreen>(OtherActor))
				{
					// rather than customtimedilation, just cutting velocity for now
					ProjectileMovement->Velocity = FVector::ZeroVector;
				}
				else
				{
					Proj->DamageImpactedActor(this, CollisionComp, Proj->GetActorLocation(), (Proj->GetActorLocation() - GetActorLocation()).GetSafeNormal());
				}
			}
			else if (!bShotDown)
			{
				BlowUp();
			}
		}
	}
}

void AUTRemoteRedeemer::Destroyed()
{
	if (Driver != nullptr)
	{
		DriverLeave(true);
	}
	TArray<UAudioComponent*> AudioComponents;
	GetComponents<UAudioComponent>(AudioComponents);
	for (int32 i = 0; i < AudioComponents.Num(); i++)
	{
		AudioComponents[i]->Stop();
	}
	Super::Destroyed();
}

void AUTRemoteRedeemer::BlowUp(FVector HitNormal)
{
	if (!bExploded)
	{
		TArray<UAudioComponent*> AudioComponents;
		GetComponents<UAudioComponent>(AudioComponents);
		for (int32 i = 0; i < AudioComponents.Num(); i++)
		{
			AudioComponents[i]->Stop();
		}
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (!GS || GS->HasMatchEnded() || GS->IsMatchIntermission())
		{
			return;
		}
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GM)
		{
			GM->BroadcastLocalized(this, UUTRedeemerLaunchAnnounce::StaticClass(), 3);
		}
		bExploded = true;
		bTearOff = true;

		if (OverlayMI != NULL)
		{
			AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
			if (WS != NULL)
			{
				WS->AddTimedMaterialParameter(OverlayMI, FName(TEXT("Static")), OverlayStaticCurve);
			}
		}

		ProjectileMovement->SetActive(false);
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			Components[i]->SetHiddenInGame(true);
		}

		PlayExplosionEffects();

		ExplosionCenter = GetActorLocation();
		if (!HitNormal.IsZero())
		{
			ECollisionChannel TraceChannel = COLLISION_TRACE_WEAPONNOCHARACTER;
			FCollisionQueryParams QueryParams(GetClass()->GetFName(), true, Instigator);
			FHitResult Hit;
			FVector EndTrace = ExplosionCenter + 100.f * HitNormal; // move up by maxstepheight
			bool bHitGeometry = GetWorld()->LineTraceSingleByChannel(Hit, ExplosionCenter, EndTrace, TraceChannel, QueryParams);
			ExplosionCenter = bHitGeometry ? 0.5f*(ExplosionCenter + Hit.Location) : EndTrace;
		}
		ExplodeStage1();
	}
}

void AUTRemoteRedeemer::ExplodeTimed()
{
	if (!bExploded && Role == ROLE_Authority)
	{
		BlowUp();
	}
}

void AUTRemoteRedeemer::OnShotDown()
{
	if (!bExploded)
	{
		bShotDown = true;

		if (Role == ROLE_Authority)
		{
			bTearOff = true;
			DriverLeave(true);
		}

		// fall to ground, explode after a delay
		ProjectileMovement->SetActive(true);
		ProjectileMovement->ProjectileGravityScale = 1.0f;
		ProjectileMovement->MaxSpeed += 2000.0f; // make room for gravity
		ProjectileMovement->bShouldBounce = true;
		ProjectileMovement->Bounciness = 0.25f;
		SetTimerUFunc(this, FName(TEXT("ExplodeTimed")), 2.f, false);

		if (GetNetMode() != NM_DedicatedServer)
		{
			PlayShotDownEffects();
		}
	}
}

void AUTRemoteRedeemer::PlayShotDownEffects()
{
	// stop any looping audio and particles
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	if (RedeemerProjectileClass && RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>()->ShotDownAmbient)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>()->ShotDownAmbient, this, SRT_IfSourceNotReplicated, false, GetActorLocation(), NULL, NULL, false, SAT_None);
	}
	for (int32 i = 0; i < Components.Num(); i++)
	{
		UAudioComponent* Audio = Cast<UAudioComponent>(Components[i]);
		if (Audio != NULL)
		{
			// only stop looping (ambient) sounds - note that the just played explosion sound may be encountered here
			if (Audio->Sound != NULL && Audio->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
			{
				Audio->Stop();
			}
		}
		else
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Components[i]);
			if (PSC != NULL && IsLoopingParticleSystem(PSC->Template))
			{
				PSC->DeactivateSystem();
			}
		}
	}
}

void AUTRemoteRedeemer::PlayExplosionEffects()
{
	// stop any looping audio
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		UAudioComponent* Audio = Cast<UAudioComponent>(Components[i]);
		if (Audio != NULL)
		{
			// only stop looping (ambient) sounds - note that the just played explosion sound may be encountered here
			if (Audio->Sound != NULL && Audio->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
			{
				Audio->Stop();
			}
		}
	}

	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != NULL)
	{
		if (DefaultRedeemer->ExplosionBP != NULL)
		{
			GetWorld()->SpawnActor<AActor>(DefaultRedeemer->ExplosionBP, FTransform(GetActorRotation(), GetActorLocation()));
			if (DefaultRedeemer->ExplosionEffects && DefaultRedeemer->ExplosionEffects.GetDefaultObject()->Audio)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), DefaultRedeemer->ExplosionEffects.GetDefaultObject()->Audio, this, SRT_IfSourceNotReplicated, false, GetActorLocation(), NULL, NULL, false, SAT_None);
			}
		}
		else if (DefaultRedeemer->ExplosionEffects != NULL)
		{
			DefaultRedeemer->ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(GetActorRotation(), GetActorLocation()), nullptr, this, DamageInstigator);
		}
	}
}

uint8 AUTRemoteRedeemer::GetTeamNum() const
{
	const IUTTeamInterface* TeamInterface = Cast<IUTTeamInterface>(PlayerState);
	if (TeamInterface != NULL)
	{
		return TeamInterface->GetTeamNum();
	}

	return CachedTeamNum;
}

void AUTRemoteRedeemer::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && (GS->IsMatchIntermission() || GS->HasMatchEnded()))
	{
		ProjectileMovement->Acceleration = FVector::ZeroVector;
		ProjectileMovement->Velocity = FVector::ZeroVector;
	}
	else if (Controller && Controller->IsLocalPlayerController())
	{
		APlayerController *PC = Cast<APlayerController>(Controller);

		FRotator Rotation = GetActorRotation();
		FVector X, Y, Z;
		FRotationMatrix R(Rotation);
		R.GetScaledAxes(X, Y, Z);

		// Bleed off Yaw and Pitch acceleration while addition additional from input
		YawAccel = (1.0f - 2.0f * DeltaTime) * YawAccel + DeltaTime * PC->RotationInput.Yaw * RedeemerMouseSensitivity;
		PitchAccel = (1.0f - 2.0f * DeltaTime) * PitchAccel + DeltaTime * PC->RotationInput.Pitch * RedeemerMouseSensitivity;
		
		if (Rotation.Pitch > MaxPitch)
		{
			PitchAccel = FMath::Min(0.0f, PitchAccel);
		}
		else if (Rotation.Pitch < MinPitch)
		{
			PitchAccel = FMath::Max(0.0f, PitchAccel);
		}

		ProjectileMovement->Acceleration = ProjectileMovement->Velocity + AccelerationBlend * (YawAccel * Y + PitchAccel * Z);
		if (ProjectileMovement->Acceleration.IsNearlyZero())
		{
			ProjectileMovement->Acceleration = ProjectileMovement->Velocity;
		}
		
		ProjectileMovement->Acceleration = ProjectileMovement->Acceleration.GetSafeNormal() * AccelRate;
/*
		if (Controller->GetPawn() == this)
		{
			NewControlRotation.Roll = Rotation.Roll;
			Controller->SetControlRotation(NewControlRotation);
		}
*/	}
}

void AUTRemoteRedeemer::GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	out_Location = GetActorLocation();
	out_Rotation = GetActorRotation();
}

void AUTRemoteRedeemer::PawnStartFire(uint8 FireModeNum)
{
	if (FireModeNum == 0)
	{
		ServerBlowUp();
		ProjectileMovement->SetActive(false);
	}
}

bool AUTRemoteRedeemer::ServerBlowUp_Validate()
{
	return true;
}

void AUTRemoteRedeemer::ServerBlowUp_Implementation()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && !GS->HasMatchEnded() && !GS->IsMatchIntermission() && !bShotDown)
	{
		BlowUp();
	}
}

void AUTRemoteRedeemer::OnRep_PlayerState()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != nullptr)
	{
		CachedTeamNum = PS->GetTeamNum();
	}
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && !GS->IsMatchIntermission() && !GS->HasMatchEnded())
	{
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(It->PlayerController);
			if (UTPC && UTPC->LastSpectatedPlayerState == PlayerState)
			{
				UTPC->ViewPawn(this);
			}
		}
	}
}

void AUTRemoteRedeemer::ExplodeStage(float RangeMultiplier)
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != NULL)
	{
		FRadialDamageParams AdjustedDamageParams = DefaultRedeemer->DamageParams;
		if (AdjustedDamageParams.OuterRadius > 0.0f)
		{
			TArray<AActor*> IgnoreActors;
			StatsHitCredit = 0.f;
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			AUTPlayerState* StatusPS = ((Role == ROLE_Authority) && GetController() && GS && GS->bPlayStatusAnnouncements) ? Cast<AUTPlayerState>(GetController()->PlayerState) : nullptr;
			if (StatusPS)
			{
				int32 LiveEnemyCount = 0;
				for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
				{
					AController* C = *Iterator;
					AUTPlayerState* TeamPS = C ? Cast<AUTPlayerState>(C->PlayerState) : nullptr;
					if (TeamPS && C->GetPawn() && !GS->OnSameTeam(GetController(), C))
					{
						LiveEnemyCount++;
					}
				}
				KillCount += LiveEnemyCount;
			}

			//DrawDebugSphere(GetWorld(), ExplosionCenter, RangeMultiplier*AdjustedDamageParams.OuterRadius, 12, FColor::Green, true, -1.f);
			UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, DefaultRedeemer->Momentum, ExplosionCenter, RangeMultiplier * AdjustedDamageParams.InnerRadius, RangeMultiplier * AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff,
				DefaultRedeemer->MyDamageType, IgnoreActors, this, DamageInstigator, nullptr, nullptr, DefaultRedeemer->CollisionFreeRadius);
			if (StatusPS)
			{
				int32 LiveEnemyCount = 0;
				for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
				{
					AController* C = *Iterator;
					AUTPlayerState* TeamPS = C ? Cast<AUTPlayerState>(C->PlayerState) : nullptr;
					if (TeamPS && C->GetPawn() && !GS->OnSameTeam(GetController(), C))
					{
						LiveEnemyCount++;
					}
				}
				KillCount -= LiveEnemyCount;
			}
			if ((Role == ROLE_Authority) && (HitsStatsName != NAME_None))
			{
				AUTPlayerState* PS = DamageInstigator ? Cast<AUTPlayerState>(DamageInstigator->PlayerState) : NULL;
				if (PS)
				{
					PS->ModifyStatsValue(HitsStatsName, StatsHitCredit / AdjustedDamageParams.BaseDamage);
				}
			}
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("UTRemoteRedeemer does not have a proper reference to UTProj_Redeemer"));
	}
}

void AUTRemoteRedeemer::ExplodeStage1()
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[0]);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTRemoteRedeemer::ExplodeStage2, DefaultRedeemer->ExplosionTimings[0]);
	}
}
void AUTRemoteRedeemer::ExplodeStage2()
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[1]);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTRemoteRedeemer::ExplodeStage3, DefaultRedeemer->ExplosionTimings[1]);
	}
}
void AUTRemoteRedeemer::ExplodeStage3()
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[2]);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTRemoteRedeemer::ExplodeStage4, DefaultRedeemer->ExplosionTimings[2]);
	}
}
void AUTRemoteRedeemer::ExplodeStage4()
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[3]);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTRemoteRedeemer::ExplodeStage5, DefaultRedeemer->ExplosionTimings[3]);
	}
}
void AUTRemoteRedeemer::ExplodeStage5()
{
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[4]);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTRemoteRedeemer::ExplodeStage6, DefaultRedeemer->ExplosionTimings[4]);
	}
}
void AUTRemoteRedeemer::ExplodeStage6()
{
	AUTPlayerState* StatusPS = ((Role == ROLE_Authority) && GetController()) ? Cast<AUTPlayerState>(GetController()->PlayerState) : nullptr;
	if (Role == ROLE_Authority)
	{
		DriverLeave(true);
	}
	AUTProj_Redeemer* DefaultRedeemer = (RedeemerProjectileClass != NULL) ? RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>() : NULL;
	if (DefaultRedeemer != nullptr)
	{
		ExplodeStage(DefaultRedeemer->ExplosionRadii[5]);
	}

	if (StatusPS && (KillCount > 0))
	{
		StatusPS->AnnounceStatus(StatusMessage::RedeemerKills, KillCount - 1);
	}
	ShutDown();
}

void AUTRemoteRedeemer::ShutDown()
{
	// Post explosion clean up here
	ProjectileMovement->SetActive(false);

	SetLifeSpan(2.0f);
}

void AUTRemoteRedeemer::ForceReplication_Implementation()
{
}

bool AUTRemoteRedeemer::IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const
{
	if (ReplicatedActor == ActorOwner)
	{
		return true;
	}

	if (Driver == ReplicatedActor->GetOwner())
	{
		return true;
	}

	return Super::IsRelevancyOwnerFor(ReplicatedActor, ActorOwner, ConnectionActor);
}

void AUTRemoteRedeemer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTRemoteRedeemer, bShotDown, COND_None);
}

void AUTRemoteRedeemer::TornOff()
{
	if (bShotDown)
	{
		OnShotDown();
	}
	else
	{
		BlowUp();
	}
}

void AUTRemoteRedeemer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CurrentFuelTime -= DeltaSeconds;
	if ((Role == ROLE_Authority) && (CurrentFuelTime < 0.f) && !bShotDown)
	{
		BlowUp();
		return;
	}
	else if ((CurrentFuelTime < 3.f) && (CurrentFuelTime + DeltaSeconds >= 3.f) && FuelWarningSound && Cast<AUTPlayerController>(GetController()))
	{
		Cast<AUTPlayerController>(GetController())->UTClientPlaySound(FuelWarningSound);
	}
	if (Role != ROLE_SimulatedProxy)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && (GS->IsMatchIntermission() || GS->HasMatchEnded()))
		{
			ProjectileMovement->Acceleration = FVector::ZeroVector;
			ProjectileMovement->Velocity = FVector::ZeroVector;
		}
		else
		{
			FRotator Rotation = GetActorRotation();
			FVector X, Y, Z;
			FRotationMatrix R(Rotation);
			R.GetScaledAxes(X, Y, Z);
			// Roll the camera when there's yaw acceleration
			FRotator RolledRotation = ProjectileMovement->Velocity.Rotation();
			float YawMag = ProjectileMovement->Acceleration | Y;
			if (YawMag > 0)
			{
				RolledRotation.Roll = FMath::Min(MaximumRoll, RollMultiplier * YawMag);
			}
			else
			{
				RolledRotation.Roll = FMath::Max(-MaximumRoll, RollMultiplier * YawMag);
			}

			float SmoothRoll = FMath::Min(1.0f, RollSmoothingMultiplier * DeltaSeconds);
			RolledRotation.Roll = RolledRotation.Roll * SmoothRoll + Rotation.Roll * (1.0f - SmoothRoll);
			SetActorRotation(RolledRotation);
		}
	}

	// check outline
	// this is done in Tick() so that it handles edge cases like viewer changing teams
	if (GetNetMode() != NM_DedicatedServer)
	{
		AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
		bool bShowOutline = false;
		if (GS != nullptr && !bExploded)
		{
			if (bShotDown)
			{
				bShowOutline = true;
			}
			else
			{
				TInlineComponentArray<UMeshComponent*> Meshes(this);
				UMeshComponent* Mesh = (Meshes.Num() > 0) ? Meshes[0] : nullptr;
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != nullptr && (GS->OnSameTeam(It->PlayerController, this) || (Mesh && (GetWorld()->GetTimeSeconds() - Mesh->LastRenderTime < 0.05f))))
					{
						// note: does not handle splitscreen
						bShowOutline = true;
						break;
					}
				}
			}
		}
		if (bShowOutline)
		{
			if (CustomDepthMesh == nullptr)
			{
				TInlineComponentArray<UMeshComponent*> Meshes(this);
				if (Meshes.Num() > 0)
				{
					CustomDepthMesh = CreateCustomDepthOutlineMesh(Meshes[0], this);
					CustomDepthMesh->CustomDepthStencilValue = (GetTeamNum() == 255) ? 255 : GetTeamNum() + 1;
					CustomDepthMesh->CustomDepthStencilValue |= 128;
					CustomDepthMesh->RegisterComponent();
				}
			}
		}
		else if (CustomDepthMesh != nullptr)
		{
			CustomDepthMesh->DestroyComponent();
			CustomDepthMesh = nullptr;
		}
	}
}

void AUTRemoteRedeemer::RedeemerDenied(AController* InstigatedBy)
{
	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GM)
	{
		APlayerState* InstigatorPS = GetController() ? GetController()->PlayerState : NULL;;
		APlayerState* InstigatedbyPS = InstigatedBy ? InstigatedBy->PlayerState : NULL;
		GM->BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
}

float AUTRemoteRedeemer::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
	{
		return 0.f;
	}
	else
	{
		int32 ResultDamage = Damage;
		FVector ResultMomentum(0.f);
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != NULL)
		{
			// we need to pull the hit info out of FDamageEvent because ModifyDamage() goes through blueprints and that doesn't correctly handle polymorphic structs
			FHitResult HitInfo;
			FVector UnusedDir;
			DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, UnusedDir);

			Game->ModifyDamage(ResultDamage, ResultMomentum, this, EventInstigator, HitInfo, DamageCauser, DamageEvent.DamageTypeClass);
		}

		if (ResultDamage > 0)
		{
			if (EventInstigator != NULL && EventInstigator != Controller)
			{
				LastHitBy = EventInstigator;
			}

			if (ResultDamage > 0)
			{
				// this is partially copied from AActor::TakeDamage() (just the calls to the various delegates and K2 notifications)
				const UDamageType* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();

				float ActualDamage = float(ResultDamage); // engine hooks want float
				// generic damage notifications sent for any damage
				ReceiveAnyDamage(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
				OnTakeAnyDamage.Broadcast(this, ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
				if (EventInstigator != NULL)
				{
					EventInstigator->InstigatedAnyDamage(ActualDamage, DamageTypeCDO, this, DamageCauser);
				}
				ProjHealth -= ActualDamage;
				UUTGameplayStatics::UTPlaySound(GetWorld(), HitSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(EventInstigator), NULL, false, SAT_PainSound);
				if (ProjHealth <= 0)
				{
					if (!bShotDown)
					{
						OnShotDown();
						RedeemerDenied(EventInstigator);
					}
				}
			}
		}
		return float(ResultDamage);
	}
}

void AUTRemoteRedeemer::PostRender(AUTHUD* HUD, UCanvas* C)
{
	if (RedeemerDisplayOne != NULL)
	{
		FCanvasTileItem XHairItem(0.5f*FVector2D(C->ClipX - 0.8f*C->ClipY, 0.f), RedeemerDisplayOne->Resource, 0.8f*FVector2D(C->ClipY, C->ClipY), FLinearColor::Red);
		XHairItem.UV0 = FVector2D(0.0f, 0.0f);
		XHairItem.UV1 = FVector2D(1.0f, 1.0f);
		XHairItem.BlendMode = SE_BLEND_Translucent;
		XHairItem.Rotation.Yaw = -1.f*GetActorRotation().Roll;
		XHairItem.Position.Y = XHairItem.Position.Y + 0.1f * C->ClipY;
		XHairItem.PivotPoint = FVector2D(0.5f, 0.5f);
		C->DrawItem(XHairItem);
	}
	int32 NewLockCount = 0;
	if (TargetIndicator != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* EnemyChar = Cast<AUTCharacter>(*It);
			if (EnemyChar != NULL && !EnemyChar->IsDead() && !EnemyChar->IsInvisible() && (EnemyChar->DrivenVehicle != this) && !EnemyChar->IsFeigningDeath() && (EnemyChar->GetMesh()->LastRenderTime > GetWorld()->TimeSeconds - 0.15f) && (GS == NULL || !GS->OnSameTeam(EnemyChar, this)))
			{
				FVector CircleLoc = EnemyChar->GetActorLocation();
				static FName NAME_RedeemerHUD(TEXT("RedeemerHUD"));
				if (!GetWorld()->LineTraceTestByChannel(GetActorLocation(), CircleLoc, COLLISION_TRACE_WEAPONNOCHARACTER, FCollisionQueryParams(NAME_RedeemerHUD, true, this)))
				{
					float CircleRadius = 1.2f * EnemyChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					FVector Perpendicular = (CircleLoc - GetActorLocation()).GetSafeNormal() ^ FVector(0.0f, 0.0f, 1.0f);
					FVector PointA = C->Project(CircleLoc + Perpendicular * CircleRadius);
					FVector PointB = C->Project(CircleLoc - Perpendicular * CircleRadius);
					FVector2D UpperLeft(FMath::Min<float>(PointA.X, PointB.X), FMath::Min<float>(PointA.Y, PointB.Y));
					FVector2D BottomRight(FMath::Max<float>(PointA.X, PointB.X), FMath::Max<float>(PointA.Y, PointB.Y));
					float MidY = (UpperLeft.Y + BottomRight.Y) * 0.5f;

					// skip drawing if too off-center
					if ((FMath::Abs(MidY - 0.5f*C->SizeY) < 0.5f*C->SizeY) && (FMath::Abs(0.5f*(UpperLeft.X + BottomRight.X) - 0.5f*C->SizeX) < 0.5f*C->SizeX))
					{
						NewLockCount++;

						// square-ify
						float SizeY = FMath::Max<float>(MidY - UpperLeft.Y, (BottomRight.X - UpperLeft.X) * 0.5f);
						UpperLeft.Y = MidY - SizeY;
						BottomRight.Y = MidY + SizeY;
						FLinearColor TargetColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
						FCanvasTileItem HeadCircleItem(UpperLeft, TargetIndicator->Resource, BottomRight - UpperLeft, TargetColor);
						HeadCircleItem.BlendMode = SE_BLEND_Translucent;
						C->DrawItem(HeadCircleItem);

						FFormatNamedArguments Args;
						static const FNumberFormattingOptions RespawnTimeFormat = FNumberFormattingOptions()
							.SetMinimumFractionalDigits(2)
							.SetMaximumFractionalDigits(2);
						Args.Add("Dist", FText::AsNumber( 0.01f * (GetActorLocation() - EnemyChar->GetActorLocation()).Size(), &RespawnTimeFormat));
						FText DistanceMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "TargetDist", "{Dist}m"), Args);
						FFontRenderInfo TextRenderInfo;
						TextRenderInfo.bEnableShadow = true;
						C->SetDrawColor(FLinearColor::Red.ToFColor(false));
						C->DrawText(HUD->TinyFont, DistanceMessage, BottomRight.X, UpperLeft.Y, 1.f, 1.f, TextRenderInfo);
					}
				}
			}
		}
	}
	if ((NewLockCount > LockCount) && HUD && HUD->UTPlayerOwner)
	{
		HUD->UTPlayerOwner->UTClientPlaySound(LockAcquiredSound);
	}

	LockCount = NewLockCount;

	HUD->DrawKillSkulls();

	bool bFuelCritical = (CurrentFuelTime < 3.f);
	float Width = 150.f;
	float Height = 21.f;
	float WidthScale = C->ClipX / 1920.f;
	float HeightScale = 1.5f * WidthScale;
	WidthScale *= 1.5f;
	FLinearColor ChargeColor = FLinearColor::White;
	float ChargePct = FMath::Clamp(CurrentFuelTime / MaxFuelTime, 0.f, 1.f);
	float XPos = 0.5f*C->ClipX;
	float YPos = 0.95f * C->ClipY;
	DrawTexture(C, HUD->HUDAtlas, XPos - 0.5f*WidthScale*Width, YPos, WidthScale*Width*ChargePct, HeightScale*Height, 127, 641, Width, Height, bFuelCritical ? 1.f : 0.7f, FLinearColor::White, FVector2D(0.f, 0.5f));

	float XL, YL;
	FText Fuel = NSLOCTEXT("Redeemer", "Fuel", "FUEL");
	C->TextSize(HUD->SmallFont, Fuel.ToString(), XL, YL);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	C->SetDrawColor(FLinearColor::White.ToFColor(false));
	C->DrawText(HUD->TinyFont, Fuel, XPos - 0.5f*WidthScale*Width - 1.1f*XL, YPos - 0.5f*YL, 1.f, 1.f, TextRenderInfo);
	if (bFuelCritical)
	{
		FText FuelWarning = NSLOCTEXT("Redeemer", "FuelWarning", "WARNING");
		C->TextSize(HUD->SmallFont, FuelWarning.ToString(), XL, YL);
		FLinearColor WarningColor = (FMath::Cos(2.f*PI*CurrentFuelTime) > 0.f) ? FLinearColor::Yellow : FLinearColor::Red;
		C->SetDrawColor(WarningColor.ToFColor(false));
		C->DrawText(HUD->TinyFont, FuelWarning, XPos - 0.5f*XL, YPos - 0.5f*YL, 1.f, 1.f, TextRenderInfo);
	}
	DrawTexture(C, HUD->HUDAtlas, XPos - 0.5f*WidthScale*Width, YPos, WidthScale*Width, HeightScale*Height, 127, 612, Width, Height, 1.f, FLinearColor::White, FVector2D(0.f, 0.5f));
}

void AUTRemoteRedeemer::PawnClientRestart()
{
	Super::PawnClientRestart();

	RedeemerRestarted(Controller);
}

void AUTRemoteRedeemer::DrawTexture(UCanvas* Canvas, UTexture* Texture, float X, float Y, float Width, float Height, float U, float V, float UL, float VL, float DrawOpacity, FLinearColor DrawColor, FVector2D RenderOffset, float Rotation, FVector2D RotPivot)
{
	if (Texture && Texture->Resource)
	{
		FVector2D RenderPos = FVector2D(X - (Width * RenderOffset.X), Y - (Height * RenderOffset.Y));

		U = U / Texture->Resource->GetSizeX();
		V = V / Texture->Resource->GetSizeY();
		UL = U + (UL / Texture->Resource->GetSizeX());
		VL = V + (VL / Texture->Resource->GetSizeY());
		DrawColor.A = DrawOpacity;

		FCanvasTileItem ImageItem(RenderPos, Texture->Resource, FVector2D(Width, Height), FVector2D(U, V), FVector2D(UL, VL), DrawColor);
		ImageItem.Rotation = FRotator(0, Rotation, 0);
		ImageItem.PivotPoint = RotPivot;
		ImageItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
		Canvas->DrawItem(ImageItem);
	}
}