// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTRemoteRedeemer.h"

AUTRemoteRedeemer::AUTRemoteRedeemer(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// Use a sphere as a simple collision representation
	CollisionComp = PCIP.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	if (CollisionComp != NULL)
	{
		CollisionComp->InitSphereRadius(0.0f);
		CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");			// Collision profiles are defined in DefaultEngine.ini
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTRemoteRedeemer::OnOverlapBegin);
		CollisionComp->bTraceComplexOnMove = true;
		CollisionComp->bGenerateOverlapEvents = false;
		RootComponent = CollisionComp;
	}

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = PCIP.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 2200.f;
	ProjectileMovement->MaxSpeed = 2200.f;
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AUTRemoteRedeemer::OnStop);

	SetReplicates(true);
	bNetTemporary = false;
	bReplicateInstigator = true;

	AccelRate = 4000.0;
	RedeemerMouseSensitivity = 200.0f;
	AccelerationBlend = 5.0f;
	MaximumRoll = 20.0f;
	RollMultiplier = 0.01f;
	RollSmoothingMultiplier = 5.0f;
	MaxPitch = 75.0f;
	MinPitch = -55.0f;

	ExplosionTimings[0] = 0.5;
	ExplosionTimings[1] = 0.2;
	ExplosionTimings[2] = 0.2;
	ExplosionTimings[3] = 0.2;
	ExplosionTimings[4] = 0.2;

	ExplosionRadii[0] = 0.125f;
	ExplosionRadii[1] = 0.3f;
	ExplosionRadii[2] = 0.475f;
	ExplosionRadii[3] = 0.65f;
	ExplosionRadii[4] = 0.825f;
	ExplosionRadii[5] = 1.0f;
}

void AUTRemoteRedeemer::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::BlowUp, 20.0, false);
}

FVector AUTRemoteRedeemer::GetVelocity() const
{
	return ProjectileMovement->Velocity;
}

void AUTRemoteRedeemer::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	ProjectileMovement->Velocity = NewVelocity;
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
		Driver = NewDriver;
		AController* C = NewDriver->Controller;
		if (C)
		{
			C->UnPossess();
			NewDriver->SetOwner(this);
			C->Possess(this);
		}
	}

	return true;
}

bool AUTRemoteRedeemer::DriverLeave(bool bForceLeave)
{
	AController* C = Controller;
	if (Driver && C)
	{
		C->UnPossess();
		Driver->SetOwner(C);
		C->Possess(Driver);
	}

	Driver = nullptr;

	return true;
}

void AUTRemoteRedeemer::OnStop(const FHitResult& Hit)
{
	BlowUp();
}

void AUTRemoteRedeemer::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role != ROLE_Authority)
	{
		return;
	}

	if (Driver == OtherActor)
	{
		return;
	}

	const IUTTeamInterface* TeamInterface = InterfaceCast<IUTTeamInterface>(OtherActor);
	if (TeamInterface)
	{
		if (TeamInterface->GetTeamNum() == GetTeamNum())
		{
			return;
		}
	}

	BlowUp();
}

void AUTRemoteRedeemer::Destroyed()
{
	if (Driver != nullptr)
	{
		DriverLeave(true);
	}
}

void AUTRemoteRedeemer::BlowUp()
{
	GetWorldTimerManager().ClearTimer(this, &AUTRemoteRedeemer::BlowUp);
	DriverLeave(true);

	ProjectileMovement->SetActive(false);

	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		Components[i]->SetHiddenInGame(true);
	}

	if (!bExploded)
	{
		bExploded = true;
		ExplodeStage1();
	}
}

uint8 AUTRemoteRedeemer::GetTeamNum() const
{
	const IUTTeamInterface* TeamInterface = InterfaceCast<IUTTeamInterface>(Controller);
	if (TeamInterface != NULL)
	{
		return TeamInterface->GetTeamNum();
	}

	return 255;
}

void AUTRemoteRedeemer::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (Controller && Controller->IsLocalPlayerController())
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
		
		ProjectileMovement->Acceleration = ProjectileMovement->Acceleration.SafeNormal() * AccelRate;

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
		
		float SmoothRoll = FMath::Min(1.0f, RollSmoothingMultiplier * DeltaTime);
		RolledRotation.Roll = RolledRotation.Roll * SmoothRoll + Rotation.Roll * (1.0f - SmoothRoll);
		SetActorRotation(RolledRotation);
	}
}

void AUTRemoteRedeemer::GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	out_Location = GetActorLocation();
	out_Rotation = GetActorRotation();
}

void AUTRemoteRedeemer::PawnStartFire(uint8 FireModeNum)
{
	ServerBlowUp();
	ProjectileMovement->SetActive(false);
}

bool AUTRemoteRedeemer::ServerBlowUp_Validate()
{
	return true;
}

void AUTRemoteRedeemer::ServerBlowUp_Implementation()
{
	BlowUp();
}

void AUTRemoteRedeemer::ExplodeStage(float RangeMultiplier)
{
	AUTProj_Redeemer *DefaultRedeemer = RedeemerProjectileClass->GetDefaultObject<AUTProj_Redeemer>();
	if (DefaultRedeemer)
	{
		FRadialDamageParams AdjustedDamageParams = DefaultRedeemer->DamageParams;
		if (AdjustedDamageParams.OuterRadius > 0.0f)
		{
			TArray<AActor*> IgnoreActors;
			FVector ExplosionCenter = GetActorLocation() + FVector(0, 0, 400);

			UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, DefaultRedeemer->Momentum, ExplosionCenter, RangeMultiplier * AdjustedDamageParams.InnerRadius, RangeMultiplier * AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff,
				DefaultRedeemer->MyDamageType, IgnoreActors, this, Controller, Controller, DefaultRedeemer->MyDamageType);
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("UTRemoteRedeemer does not have a proper reference to UTProj_Redeemer"));
	}
}

void AUTRemoteRedeemer::ExplodeStage1()
{
	ExplodeStage(ExplosionRadii[0]);
	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::ExplodeStage2, ExplosionTimings[0]);
}
void AUTRemoteRedeemer::ExplodeStage2()
{
	ExplodeStage(ExplosionRadii[1]);
	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::ExplodeStage3, ExplosionTimings[1]);
}
void AUTRemoteRedeemer::ExplodeStage3()
{
	ExplodeStage(ExplosionRadii[2]);
	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::ExplodeStage4, ExplosionTimings[2]);
}
void AUTRemoteRedeemer::ExplodeStage4()
{
	ExplodeStage(ExplosionRadii[3]);
	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::ExplodeStage5, ExplosionTimings[3]);
}
void AUTRemoteRedeemer::ExplodeStage5()
{
	ExplodeStage(ExplosionRadii[4]);
	GetWorldTimerManager().SetTimer(this, &AUTRemoteRedeemer::ExplodeStage6, ExplosionTimings[4]);
}
void AUTRemoteRedeemer::ExplodeStage6()
{
	ExplodeStage(ExplosionRadii[5]);
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