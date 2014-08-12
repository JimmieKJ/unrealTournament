// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTCharacterMovement.h"
#include "UTProjectile.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTDmgType_Suicide.h"
#include "UTDmgType_Fell.h"
#include "UTDmgType_FallingCrush.h"
#include "UTJumpBoots.h"
#include "UTCTFFlag.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTTeamGameMode.h"
#include "UTDmgType_Telefragged.h"

//////////////////////////////////////////////////////////////////////////
// AUTCharacter

DEFINE_LOG_CATEGORY_STATIC(LogUTCharacter, Log, All);

AUTCharacter::AUTCharacter(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP.SetDefaultSubobjectClass<UUTCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	CapsuleComponent->InitCapsuleSize(42.f, 92.0f);

	// Create a CameraComponent	
	CharacterCameraComponent = PCIP.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
	CharacterCameraComponent->AttachParent = CapsuleComponent;
	DefaultBaseEyeHeight = 71.f;
	CharacterCameraComponent->RelativeLocation = FVector(0, 0, DefaultBaseEyeHeight); // Position the camera

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	FirstPersonMesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CharacterMesh1P"));
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->AttachParent = CharacterCameraComponent;
	FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;

	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->bEnablePhysicsOnDedicatedServer = true; // needed for feign death; death ragdoll shouldn't be invoked on server

	UTCharacterMovement = Cast<UUTCharacterMovement>(CharacterMovement);

	HealthMax = 100;
	SuperHealthMax = 199;
	DamageScaling = 1.0f;
	FireRateMultiplier = 1.0f;
	bSpawnProtectionEligible = true;
	MaxSafeFallSpeed = 2400.0f;
	FallingDamageFactor = 100.0f;
	CrushingDamageFactor = 2.0f;
	HeadScale = 1.0f;
	HeadRadius = 18.0f;
	HeadHeight = 8.0f;
	HeadBone = FName(TEXT("b_Head"));

	BobTime = 0.f;
	WeaponBobMagnitude = FVector(0.f, 0.8f, 0.4f);
	WeaponJumpBob = FVector(0.f, 0.f, -3.6f);
	WeaponDodgeBob = FVector(0.f, 6.f, -2.5f);
	WeaponLandBob = FVector(0.f, 0.f, 10.5f);
	WeaponBreathingBobRate = 0.2f;
	WeaponRunningBobRate = 0.8f;
	WeaponJumpBobInterpRate = 6.5f;
	WeaponLandBobDecayRate = 5.f;
	EyeOffset = FVector(0.f, 0.f, 0.f);
	CrouchEyeOffset = EyeOffset;
	TargetEyeOffset = EyeOffset;
	EyeOffsetInterpRate = 9.f;
	CrouchEyeOffsetInterpRate = 6.f;
	EyeOffsetDecayRate = 7.f;
	EyeOffsetJumpBob = 20.f;
	EyeOffsetLandBob = -110.f;
	EyeOffsetLandBobThreshold = 300.f;
	WeaponLandBobThreshold = 100.f;
	FullWeaponLandBobVelZ = 900.f;
	FullEyeOffsetLandBobVelZ = 750.f;
	WeaponDirChangeDeflection = 4.f;
	RagdollBlendOutTime = 0.75f;

	MinPainSoundInterval = 0.35f;
	LastPainSoundTime = -100.0f;
	bCanPlayWallHitSound = true;

	SprintAmbientStartSpeed = 1000.f;
	SprintAmbientStopSpeed = 970.f;
	FallingAmbientStartSpeed = -1600.f;

	PrimaryActorTick.bStartWithTickEnabled = true;

	// TODO: write real relevancy checking
	NetCullDistanceSquared = 500000000.0f;

	OnActorBeginOverlap.AddDynamic(this, &AUTCharacter::OnOverlapBegin);

	PlayerIndicatorMaxDistance = 2700.0f;
}

void AUTCharacter::SetMeshVisibility(bool bThirdPersonView)
{
	FirstPersonMesh->SetOwnerNoSee(bThirdPersonView);

	Mesh->SetVisibility(bThirdPersonView);
	Mesh->SetOwnerNoSee(!bThirdPersonView);
	if (OverlayMesh != NULL)
	{
		OverlayMesh->SetVisibility(bThirdPersonView);
		OverlayMesh->SetOwnerNoSee(!bThirdPersonView);
	}
	if (WeaponAttachment != NULL && WeaponAttachment->Mesh != NULL)
	{
		WeaponAttachment->Mesh->SetOwnerNoSee(!bThirdPersonView);
	}
	if (Weapon != NULL && Weapon->Mesh != NULL)
	{
		Weapon->Mesh->SetOwnerNoSee(bThirdPersonView);
	}
}

void AUTCharacter::BeginPlay()
{
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->AddPostRenderedActor(this);
		}
	}
	UE_LOG(UT,Log,TEXT("UTCHARACTER.BeginPlay()"));
	if (Health == 0 && Role == ROLE_Authority)
	{
		Health = HealthMax;
	}
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, DefaultBaseEyeHeight), false);
	if (CharacterCameraComponent->RelativeLocation.Size2D() > 0.0f)
	{
		UE_LOG(UT, Warning, TEXT("%s: CameraComponent shouldn't have X/Y translation!"), *GetName());
	}
	Super::BeginPlay();
}

void AUTCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetNetMode() != NM_DedicatedServer)
	{
		BodyMI = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void AUTCharacter::RecalculateBaseEyeHeight()
{
	BaseEyeHeight = bIsCrouched ? CrouchedEyeHeight : DefaultBaseEyeHeight;
}

void AUTCharacter::OnEndCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	Super::OnEndCrouch(HeightAdjust, ScaledHeightAdjust);
	CrouchEyeOffset.Z += CrouchedEyeHeight - DefaultBaseEyeHeight - HeightAdjust;
	UTCharacterMovement->OldZ = GetActorLocation().Z;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, DefaultBaseEyeHeight), false);
}

void AUTCharacter::OnStartCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	Super::OnStartCrouch(HeightAdjust, ScaledHeightAdjust);
	CrouchEyeOffset.Z += DefaultBaseEyeHeight - CrouchedEyeHeight + HeightAdjust;
	UTCharacterMovement->OldZ = GetActorLocation().Z;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CrouchedEyeHeight),false);

	// Kill any montages that might be overriding the crouch anim
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (AnimInstance != NULL)
	{
		AnimInstance->Montage_Stop(0.2f);
	}
}

void AUTCharacter::Restart()
{
	Super::Restart();
	ClearJumpInput();

	// make sure equipped weapon state is synchronized
	if (IsLocallyControlled())
	{
		if (Weapon != NULL)
		{
			SwitchWeapon(Weapon);
		}
		else
		{
			SwitchToBestWeapon();
		}
	}
}

void AUTCharacter::DeactivateSpawnProtection()
{
	bSpawnProtectionEligible = false;
	// TODO: visual effect
}

void AUTCharacter::SetHeadScale(float NewHeadScale)
{
	HeadScale = NewHeadScale;
	if (GetNetMode() != NM_DedicatedServer)
	{
		HeadScaleUpdated();
	}
}

void AUTCharacter::HeadScaleUpdated()
{
	// TODO
}

static TAutoConsoleVariable<int32> CVarDebugHeadshots(
	TEXT("p.DebugHeadshots"),
	0,
	TEXT("Debug headshot traces"),
	ECVF_Default);

bool AUTCharacter::IsHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor)
{
	if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
	{
		// no headshots while dodge rolling
		return false;
	}

	// force mesh update if necessary
	if (!Mesh->ShouldTickPose())
	{
		Mesh->TickAnimation(0.0f);
		Mesh->RefreshBoneTransforms();
		Mesh->UpdateComponentToWorld();
	}
	FVector HeadLocation = Mesh->GetSocketLocation(HeadBone) + FVector(0.0f, 0.0f, HeadHeight);
	bool bHeadShot = FMath::PointDistToLine(HeadLocation, ShotDirection, HitLocation) < HeadRadius * HeadScale * WeaponHeadScaling;

	if (CVarDebugHeadshots.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(GetWorld(), HitLocation + (ShotDirection * 1000.f), HitLocation - (ShotDirection * 1000.f), FColor::White, true);
		if (bHeadShot)
		{
			DrawDebugSphere(GetWorld(), HeadLocation, HeadRadius * HeadScale * WeaponHeadScaling, 10, FColor::Green, true);
		}
		else
		{
			DrawDebugSphere(GetWorld(), HeadLocation, HeadRadius * HeadScale * WeaponHeadScaling, 10, FColor::Red, true);
		}
	}

	if (bHeadShot)
	{
		// check for inventory items that prevent headshots
		for (AUTInventory* Inv = GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			if (Inv->bCallDamageEvents && Inv->PreventHeadShot(HitLocation, ShotDirection, WeaponHeadScaling, bConsumeArmor))
			{
				bHeadShot = false;
				break;
			}
		}
	}
	return bHeadShot;
}

FVector AUTCharacter::GetWeaponBobOffset(float DeltaTime, AUTWeapon* MyWeapon)
{
	FRotationMatrix RotMatrix = FRotationMatrix(GetViewRotation());
	FVector X = RotMatrix.GetScaledAxis(EAxis::X);
	FVector Y = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector Z = RotMatrix.GetScaledAxis(EAxis::Z);

	float InterpTime = FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime);
	if (!CharacterMovement || CharacterMovement->IsFalling() || !MyWeapon )
	{
		// interp out weapon bob if falling
		BobTime = 0.f;
		CurrentWeaponBob.Y *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		CurrentWeaponBob.Z *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
	}
	else
	{
		float Speed = CharacterMovement->Velocity.Size();
		float LastBobTime = BobTime;
		float BobFactor = (WeaponBreathingBobRate + WeaponRunningBobRate*Speed / CharacterMovement->MaxWalkSpeed);
		BobTime += DeltaTime * BobFactor;
		DesiredJumpBob *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		FVector AccelDir = CharacterMovement->GetCurrentAcceleration().SafeNormal();
		if ((AccelDir | CharacterMovement->Velocity) < 0.5f*CharacterMovement->MaxWalkSpeed)
		{
			if ((AccelDir | Y) > 0.65f)
			{
				DesiredJumpBob.Y = -1.f*WeaponDirChangeDeflection;
			}
			else if ((AccelDir | Y) < -0.65f)
			{
				DesiredJumpBob.Y = WeaponDirChangeDeflection;
			}
		}
		CurrentWeaponBob.X = 0.f;
		if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
		{
			// interp out weapon bob when dodge rolliing
			BobTime = 0.f;
			CurrentWeaponBob.Y *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
			CurrentWeaponBob.Z *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		}
		else
		{
			CurrentWeaponBob.Y = WeaponBobMagnitude.Y*BobFactor * FMath::Sin(8.f*BobTime);
			CurrentWeaponBob.Z = WeaponBobMagnitude.Z*BobFactor * FMath::Sin(16.f*BobTime);
		}

		// play footstep sounds when weapon changes bob direction if walking
		if (CharacterMovement->MovementMode == MOVE_Walking && Speed > 10.0f && !bIsCrouched && (FMath::FloorToInt(0.5f + 8.f*BobTime / PI) != FMath::FloorToInt(0.5f + 8.f*LastBobTime / PI)))
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}
	CurrentJumpBob = (1.f - InterpTime)*CurrentJumpBob + InterpTime*DesiredJumpBob;
	float WeaponBobGlobalScaling = MyWeapon->WeaponBobScaling * (Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->WeaponBobGlobalScaling : 1.f);
	float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
	return WeaponBobGlobalScaling*(CurrentWeaponBob.Y + CurrentJumpBob.Y)*Y + WeaponBobGlobalScaling*(CurrentWeaponBob.Z + CurrentJumpBob.Z)*Z + CrouchEyeOffset + EyeOffsetGlobalScaling*EyeOffset;
}

void AUTCharacter::NotifyJumpApex()
{
	DesiredJumpBob = FVector(0.f);
}

float AUTCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
	{
		return 0.f;
	}
	else if (Damage < 0.0f)
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("TakeDamage() called with damage %i of type %s... use HealDamage() to add health"), int32(Damage), *GetNameSafe(DamageEvent.DamageTypeClass));
		return 0.0f;
	}
	else
	{
		UE_LOG(LogUTCharacter, Verbose, TEXT("%s::TakeDamage() %d Class:%s Causer:%s"), *GetName(), int32(Damage), *GetNameSafe(DamageEvent.DamageTypeClass), *GetNameSafe(DamageCauser));

		const UDamageType* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		const UUTDamageType* const UTDamageTypeCDO = Cast<UUTDamageType>(DamageTypeCDO); // warning: may be NULL

		int32 ResultDamage = FMath::TruncToInt(Damage);
		FVector ResultMomentum = UTGetDamageMomentum(DamageEvent, this, EventInstigator);
		if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			bool bScaleMomentum = !DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID) || ((const FUTRadialDamageEvent&)DamageEvent).bScaleMomentum;
			if (Damage == 0.0f)
			{
				if (bScaleMomentum)
				{
					// use fake 1.0 damage so we can use the damage scaling code to scale momentum
					ResultMomentum *= InternalTakeRadialDamage(1.0f, (const FRadialDamageEvent&)DamageEvent, EventInstigator, DamageCauser);
				}
			}
			else
			{
				float AdjustedDamage = InternalTakeRadialDamage(Damage, (const FRadialDamageEvent&)DamageEvent, EventInstigator, DamageCauser);
				if (bScaleMomentum)
				{
					ResultMomentum *= AdjustedDamage / Damage;
				}
				ResultDamage = FMath::TruncToInt(AdjustedDamage);
			}
		}
		if (!IsDead())
		{
			// note that we split the gametype query out so that it's always in a consistent place
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != NULL)
			{
				// we need to pull the hit info out of FDamageEvent because ModifyDamage() goes through blueprints and that doesn't correctly handle polymorphic structs
				FHitResult HitInfo;
				FVector UnusedDir;
				DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, UnusedDir);

				Game->ModifyDamage(ResultDamage, ResultMomentum, this, EventInstigator, HitInfo, DamageCauser);
			}
			ModifyDamageTaken(ResultDamage, ResultMomentum, DamageEvent, EventInstigator, DamageCauser);

			if (ResultDamage > 0 || !ResultMomentum.IsZero())
			{
				if (EventInstigator != NULL && EventInstigator != Controller)
				{
					LastHitBy = EventInstigator;
				}

				if (ResultDamage > 0)
				{
					// this is partially copied from AActor::TakeDamage() (just the calls to the various delegates and K2 notifications)

					float ActualDamage = float(ResultDamage); // engine hooks want float
					// generic damage notifications sent for any damage
					ReceiveAnyDamage(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
					OnTakeAnyDamage.Broadcast(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
					if (EventInstigator != NULL)
					{
						EventInstigator->InstigatedAnyDamage(ActualDamage, DamageTypeCDO, this, DamageCauser);
					}
					if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
					{
						// point damage event, pass off to helper function
						FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

						// K2 notification for this actor
						if (ActualDamage != 0.f)
						{
							ReceivePointDamage(ActualDamage, DamageTypeCDO, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.ImpactNormal, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, EventInstigator, DamageCauser);
							OnTakePointDamage.Broadcast(ActualDamage, EventInstigator, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, DamageTypeCDO, DamageCauser);
						}
					}
					else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
					{
						// radial damage event, pass off to helper function
						FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;

						// K2 notification for this actor
						if (ActualDamage != 0.f)
						{
							FHitResult const& Hit = (RadialDamageEvent->ComponentHits.Num() > 0) ? RadialDamageEvent->ComponentHits[0] : FHitResult();
							ReceiveRadialDamage(ActualDamage, DamageTypeCDO, RadialDamageEvent->Origin, Hit, EventInstigator, DamageCauser);
						}
					}
				}
			}

			Health -= ResultDamage;
			UE_LOG(LogUTCharacter, Verbose, TEXT("%s took %d damage, %d health remaining"), *GetName(), ResultDamage, Health);

			// Let the game Score damage if it wants to
			Game->ScoreDamage(ResultDamage, Controller, EventInstigator);

			if (UTDamageTypeCDO != NULL)
			{
				ResultMomentum.Z = UTDamageTypeCDO->bForceZMomentum ? FMath::Max<float>(ResultMomentum.Z, 0.4f * ResultMomentum.Size()) : ResultMomentum.Z;
				if (Controller && (EventInstigator == Controller))
				{
					ResultMomentum *= UTDamageTypeCDO->SelfMomentumBoost;
				}
			}
			// if Z impulse is low enough and currently walking, remove Z impulse to prevent switch to falling physics, preventing lockdown effects
			else if (CharacterMovement->MovementMode == MOVE_Walking && ResultMomentum.Z < ResultMomentum.Size() * 0.1f)
			{
				ResultMomentum.Z = 0.0f;
			}
			CharacterMovement->AddImpulse(ResultMomentum, false);
			NotifyTakeHit(EventInstigator, ResultDamage, ResultMomentum, DamageEvent);
			SetLastTakeHitInfo(ResultDamage, ResultMomentum, DamageEvent);
			if (Health <= 0)
			{
				Died(EventInstigator, DamageEvent);
			}
		}
		else
		{
			FVector HitLocation = Mesh->GetComponentLocation();
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				HitLocation = ((const FPointDamageEvent&)DamageEvent).HitInfo.Location;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent& RadialEvent = (const FRadialDamageEvent&)DamageEvent;
				if (RadialEvent.ComponentHits.Num() > 0)
				{
					HitLocation = RadialEvent.ComponentHits[0].Location;
				}
			}
			Mesh->AddImpulseAtLocation(ResultMomentum, HitLocation);
		}
	
		return float(ResultDamage);
	}
}

void AUTCharacter::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// check for caused modifiers on instigator
	AUTCharacter* InstigatorChar = NULL;
	if (DamageCauser != NULL)
	{
		InstigatorChar = Cast<AUTCharacter>(DamageCauser->Instigator);
	}
	if (InstigatorChar == NULL && EventInstigator != NULL)
	{
		InstigatorChar = Cast<AUTCharacter>(EventInstigator->GetPawn());
	}
	if (InstigatorChar != NULL && !InstigatorChar->IsDead())
	{
		InstigatorChar->ModifyDamageCaused(Damage, Momentum, DamageEvent, this, EventInstigator, DamageCauser);
	}
	// check inventory
	if (InventoryList != NULL)
	{
		AUTInventory* Inv = InventoryList;
		while (Inv != NULL)
		{
			AUTInventory* NextInv = Inv->GetNext(); // cache here in case Inv is destroyed by damage absorption (e.g. armor)
			if (Inv->bCallDamageEvents)
			{
				Inv->ModifyDamageTaken(Damage, Momentum, DamageEvent, EventInstigator, DamageCauser);
			}
			Inv = NextInv;
		}
	}
}
void AUTCharacter::ModifyDamageCaused_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AActor* Victim, AController* EventInstigator, AActor* DamageCauser)
{
	Damage *= DamageScaling;
}

void AUTCharacter::SetLastTakeHitInfo(int32 Damage, const FVector& Momentum, const FDamageEvent& DamageEvent)
{
	LastTakeHitInfo.Damage = Damage;
	LastTakeHitInfo.DamageType = DamageEvent.DamageTypeClass;
	LastTakeHitInfo.Momentum = Momentum;

	FVector NewRelHitLocation;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		NewRelHitLocation = ((FPointDamageEvent*)&DamageEvent)->HitInfo.Location - GetActorLocation();
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		NewRelHitLocation = ((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].Location - GetActorLocation();
	}
	// make sure there's a difference from the last time so replication happens
	if ((NewRelHitLocation - LastTakeHitInfo.RelHitLocation).IsNearlyZero(1.0f))
	{
		NewRelHitLocation.Z += 1.0f;
	}
	LastTakeHitInfo.RelHitLocation = NewRelHitLocation;

	LastTakeHitTime = GetWorld()->TimeSeconds;
			
	PlayTakeHitEffects();
}

void AUTCharacter::PlayTakeHitEffects_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer && LastTakeHitInfo.Damage > 0 && BloodEffect != NULL)
	{
		// we want ourselves as the Outer for OwnerNoSee checks yet not attached to us, so the GameplayStatics functions don't get the job done
		UParticleSystemComponent* PSC = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass(), this);
		PSC->bAutoDestroy = true;
		PSC->SecondsBeforeInactive = 0.0f;
		PSC->bAutoActivate = false;
		PSC->SetTemplate(BloodEffect);
		PSC->bOverrideLODMethod = false;
		PSC->RegisterComponentWithWorld(GetWorld());
		PSC->SetAbsolute(true, true, true);
		PSC->SetOwnerNoSee(true); // FIXME: !IsFirstPerson()
		PSC->SetWorldLocationAndRotation(LastTakeHitInfo.RelHitLocation + GetActorLocation(), LastTakeHitInfo.RelHitLocation.Rotation());
		PSC->SetRelativeScale3D(FVector(1.f));
		PSC->ActivateSystem(true);
	}
}

void AUTCharacter::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	if (Role == ROLE_Authority)
	{
		AUTPlayerController* InstigatedByPC = Cast<AUTPlayerController>(InstigatedBy);
		if (InstigatedByPC != NULL)
		{
			InstigatedByPC->ClientNotifyCausedHit(this, Damage);
		}

		// we do the sound here instead of via PlayTakeHitEffects() so it uses RPCs instead of variable replication which is higher priority
		// (at small bandwidth cost)
		if (GetWorld()->TimeSeconds - LastPainSoundTime >= MinPainSoundInterval)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), PainSound, this, SRT_All, false, FVector::ZeroVector, InstigatedByPC);
			LastPainSoundTime = GetWorld()->TimeSeconds;
		}

		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			PC->NotifyTakeHit(InstigatedBy, Damage, Momentum, DamageEvent);
		}
		else
		{
			// TODO: bots
		}
	}
}

void AUTCharacter::OnRepDodgeRolling()
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->bIsDodgeRolling = bRepDodgeRolling;
	}
}

bool AUTCharacter::Died(AController* EventInstigator, const FDamageEvent& DamageEvent)
{
	if (Role < ROLE_Authority || IsDead())
	{
		// can't kill pawns on client
		// can't kill pawns that are already dead :)
		return false;
	}
	else
	{
		// if this is an environmental death then refer to the previous killer so that they receive credit (knocked into lava pits, etc)
		if (DamageEvent.DamageTypeClass != NULL && DamageEvent.DamageTypeClass.GetDefaultObject()->bCausedByWorld && (EventInstigator == NULL || EventInstigator == Controller) && LastHitBy != NULL)
		{
			EventInstigator = LastHitBy;
		}

		// TODO: GameInfo::PreventDeath()

		bTearOff = true; // important to set this as early as possible so IsDead() returns true

		GetWorld()->GetAuthGameMode<AUTGameMode>()->Killed(EventInstigator, (Controller != NULL) ? Controller : Cast<AController>(GetOwner()), this, DamageEvent.DamageTypeClass);

		Health = FMath::Min<int32>(Health, 0);

		// Drop any carried objects when you die.
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS != NULL && PS->CarriedObject != NULL)
		{
			PS->CarriedObject->Drop(EventInstigator);
		}

		if (Controller != NULL)
		{
			Controller->PawnPendingDestroy(this);
		}

		OnDied.Broadcast(EventInstigator, DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass.GetDefaultObject() : NULL);

		PlayDying();

		return true;
	}
}

void AUTCharacter::StartRagdoll()
{
	StopFiring();
	bDisallowWeaponFiring = true;
	bInRagdollRecovery = false;
	
	CharacterMovement->ApplyAccumulatedForces(0.0f);
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->UpdateKinematicBonesToPhysics(true, true);
	Mesh->SetSimulatePhysics(true);
	Mesh->RefreshBoneTransforms();
	Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
	Mesh->DetachFromParent(true);
	RootComponent = Mesh;
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComponent->DetachFromParent(false);
	CapsuleComponent->AttachTo(Mesh, NAME_None, EAttachLocation::KeepWorldPosition);

	if (bDeferredReplicatedMovement)
	{
		OnRep_ReplicatedMovement();
		// OnRep_ReplicatedMovement() will only apply to the root body but in this case we want to apply to all bodies
		Mesh->SetAllPhysicsLinearVelocity(Mesh->GetBodyInstance(NAME_None)->GetUnrealWorldVelocity());
		bDeferredReplicatedMovement = false;
	}
	else
	{
		Mesh->SetAllPhysicsLinearVelocity(GetMovementComponent()->Velocity * FVector(1.0f, 1.0f, 0.5f), false); // gravity doesn't seem to be the same magnitude for ragdolls...
	}
}

void AUTCharacter::StopRagdoll()
{
	CapsuleComponent->DetachFromParent(true);
	FRotator FixedRotation = CapsuleComponent->RelativeRotation;
	FixedRotation.Pitch = FixedRotation.Roll = 0.0f;
	if (Controller != NULL)
	{
		// always recover in the direction the controller is facing since turning is instant
		FixedRotation.Yaw = Controller->GetControlRotation().Yaw;
	}
	CapsuleComponent->SetRelativeRotation(FixedRotation);
	CapsuleComponent->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->CapsuleComponent->RelativeScale3D);
	RootComponent = CapsuleComponent;

	Mesh->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->MeshComponentUpdateFlag;
	Mesh->bBlendPhysics = false; // for some reason bBlendPhysics == false is the value that actually blends instead of using only physics

	// TODO: make sure cylinder is in valid position (navmesh?)
	FVector AdjustedLoc = GetActorLocation() + FVector(0.0f, 0.0f, CapsuleComponent->GetUnscaledCapsuleHalfHeight());
	GetWorld()->FindTeleportSpot(this, AdjustedLoc, GetActorRotation());
	CapsuleComponent->SetWorldLocation(AdjustedLoc);

	// terminate constraints on the root bone so we can move it without interference
	for (int32 i = 0; i < Mesh->Constraints.Num(); i++)
	{
		if (Mesh->Constraints[i] != NULL && (Mesh->GetBoneIndex(Mesh->Constraints[i]->ConstraintBone1) == 0 || Mesh->GetBoneIndex(Mesh->Constraints[i]->ConstraintBone2) == 0))
		{
			Mesh->Constraints[i]->TermConstraint();
		}
	}
	// move the root bone to where we put the capsule, then disable further physics
	if (Mesh->Bodies.Num() > 0 && Mesh->Bodies[0] != NULL)
	{
		FTransform NewTransform(GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->RelativeRotation, GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->RelativeLocation);
		NewTransform *= CapsuleComponent->GetComponentTransform();
		Mesh->Bodies[0]->SetBodyTransform(NewTransform, true);
		Mesh->Bodies[0]->SetInstanceSimulatePhysics(false, true);
		Mesh->Bodies[0]->PhysicsBlendWeight = 1.0f; // second parameter of SetInstanceSimulatePhysics() doesn't actually work at the moment...
		Mesh->SyncComponentToRBPhysics();
	}

	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	bInRagdollRecovery = true;
}

void AUTCharacter::PlayDying()
{
	SetAmbientSound(NULL);
	SetLocalAmbientSound(NULL);

	// TODO: damagetype effects, etc

	if (GetNetMode() != NM_DedicatedServer)
	{
		StartRagdoll();
		SetLifeSpan(10.0f); // TODO: destroy early if hidden, et al
	}
	else
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetLifeSpan(0.25f);
	}
}

void AUTCharacter::FeignDeath()
{
	ServerFeignDeath();
}
bool AUTCharacter::ServerFeignDeath_Validate()
{
	return true;
}
void AUTCharacter::ServerFeignDeath_Implementation()
{
	if (Role == ROLE_Authority && !IsDead())
	{
		if (bFeigningDeath)
		{
			if (GetWorld()->TimeSeconds >= FeignDeathRecoverStartTime)
			{
				bFeigningDeath = false;
				PlayFeignDeath();
			}
		}
		else if (Mesh->Bodies.Num() == 0 || Mesh->Bodies[0]->PhysicsBlendWeight <= 0.0f)
		{
			bFeigningDeath = true;
			FeignDeathRecoverStartTime = GetWorld()->TimeSeconds + 1.0f;
			PlayFeignDeath();
		}
	}
}
void AUTCharacter::PlayFeignDeath()
{
	if (bFeigningDeath)
	{
		DropFlag();

		// force behind view
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
			if (PC != NULL && PC->GetViewTarget() == this)
			{
				PC->BehindView(true);
			}
		}

		StartRagdoll();
	}
	else
	{
		StopRagdoll();
	}
}
void AUTCharacter::ForceFeignDeath(float MinRecoveryTime)
{
	if (!bFeigningDeath && Role == ROLE_Authority && !IsDead())
	{
		bFeigningDeath = true;
		FeignDeathRecoverStartTime = GetWorld()->TimeSeconds + MinRecoveryTime;
		PlayFeignDeath();
	}
}

void AUTCharacter::Destroyed()
{
	Super::Destroyed();

	DiscardAllInventory();
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->Destroy();
		WeaponAttachment = NULL;
	}

	if (GetWorld()->GetNetMode() != NM_DedicatedServer && GEngine->GetWorldContextFromWorld(GetWorld()) != NULL) // might not be able to get world context when exiting PIE
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}
}

void AUTCharacter::SetAmbientSound(USoundBase* NewAmbientSound, bool bClear)
{
	if (bClear)
	{
		if (NewAmbientSound == AmbientSound)
		{
			AmbientSound = NULL;
		}
	}
	else
	{
		AmbientSound = NewAmbientSound;
	}
	AmbientSoundUpdated();
}

void AUTCharacter::AmbientSoundUpdated()
{
	if (AmbientSound == NULL)
	{
		if (AmbientSoundComp != NULL)
		{
			AmbientSoundComp->Stop();
		}
	}
	else
	{
		if (AmbientSoundComp == NULL)
		{
			AmbientSoundComp = NewObject<UAudioComponent>(this);
			AmbientSoundComp->bAutoDestroy = false;
			AmbientSoundComp->bAutoActivate = false;
			AmbientSoundComp->AttachTo(RootComponent);
			AmbientSoundComp->RegisterComponent();
		}
		if (AmbientSoundComp->Sound != AmbientSound)
		{
			// don't attenuate/spatialize sounds made by a local viewtarget
			AmbientSoundComp->bAllowSpatialization = true;
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
				{
					AmbientSoundComp->bAllowSpatialization = false;
					break;
				}
			}
			AmbientSoundComp->SetSound(AmbientSound);
		}
		if (!AmbientSoundComp->IsPlaying())
		{
			AmbientSoundComp->Play();
		}
	}
}


void AUTCharacter::SetLocalAmbientSound(USoundBase* NewAmbientSound, bool bClear)
{
	if (bClear)
	{
		if (NewAmbientSound == LocalAmbientSound)
		{
			LocalAmbientSound = NULL;
		}
	}
	else
	{
		LocalAmbientSound = NewAmbientSound;
	}
	LocalAmbientSoundUpdated();
}

void AUTCharacter::LocalAmbientSoundUpdated()
{
	if (LocalAmbientSound == NULL)
	{
		if (LocalAmbientSoundComp != NULL)
		{
			LocalAmbientSoundComp->Stop();
		}
	}
	else
	{
		if (LocalAmbientSoundComp == NULL)
		{
			LocalAmbientSoundComp = NewObject<UAudioComponent>(this);
			LocalAmbientSoundComp->bAutoDestroy = false;
			LocalAmbientSoundComp->bAutoActivate = false;
			LocalAmbientSoundComp->AttachTo(RootComponent);
			LocalAmbientSoundComp->RegisterComponent();
		}
		if (LocalAmbientSoundComp->Sound != LocalAmbientSound)
		{
			// don't attenuate/spatialize sounds made by a local viewtarget
			LocalAmbientSoundComp->bAllowSpatialization = true;
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
				{
					LocalAmbientSoundComp->bAllowSpatialization = false;
					break;
				}
			}
			LocalAmbientSoundComp->SetSound(AmbientSound);
		}
		if (!LocalAmbientSoundComp->IsPlaying())
		{
			LocalAmbientSoundComp->Play();
		}
	}
}
void AUTCharacter::StartFire(uint8 FireModeNum)
{
	UE_LOG(LogUTCharacter, Verbose, TEXT("StartFire %d"), FireModeNum);

	if (!IsLocallyControlled())
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("StartFire() can only be called on the owning client"));
	}
	// when feigning death, attempting to fire gets us out of it
	else if (bFeigningDeath)
	{
		FeignDeath();
	}
	else if (Weapon != NULL)
	{
		Weapon->StartFire(FireModeNum);
	}
}

void AUTCharacter::StopFire(uint8 FireModeNum)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		Weapon->StopFire(FireModeNum);
	}
}

void AUTCharacter::StopFiring()
{
	for (int32 i = 0; i < PendingFire.Num(); i++)
	{
		if (PendingFire[i])
		{
			StopFire(i);
		}
	}
}

bool AUTCharacter::IsTriggerDown(uint8 FireMode)
{
	return IsPendingFire(FireMode);
}

void AUTCharacter::SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode)
{
	// make sure two consecutive shots don't set the same FlashLocation as that will prevent replication and thus clients won't see the shot
	FlashLocation = ((FlashLocation - InFlashLoc).SizeSquared() >= 1.0f) ? InFlashLoc : (InFlashLoc + FVector(0.0f, 0.0f, 1.0f));
	// we reserve the zero vector to stop firing, so make sure we aren't setting a value that would replicate that way
	if (FlashLocation.IsNearlyZero(1.0f))
	{
		FlashLocation.Z += 1.1f;
	}
	FireMode = InFireMode;
	FiringInfoUpdated();
}
void AUTCharacter::IncrementFlashCount(uint8 InFireMode)
{
	FlashCount++;
	// we reserve zero for not firing; make sure we don't set that
	if (FlashCount == 0)
	{
		FlashCount++;
	}
	FireMode = InFireMode;
	FiringInfoUpdated();
}
void AUTCharacter::ClearFiringInfo()
{
	FlashLocation = FVector::ZeroVector;
	FlashCount = 0;
	FiringInfoUpdated();
}
void AUTCharacter::FiringInfoUpdated()
{
	// Kill any montages that might be overriding the crouch anim
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (AnimInstance != NULL)
	{
		AnimInstance->Montage_Stop(0.2f);
	}

	if (Weapon != NULL && IsLocallyControlled() && Cast<APlayerController>(Controller) != NULL)  // and in first person?
	{
		if (!FlashLocation.IsZero())
		{
			Weapon->PlayImpactEffects(FlashLocation);
		}
	}
	else if (WeaponAttachment != NULL)
	{
		if (FlashCount != 0 || !FlashLocation.IsZero())
		{
			WeaponAttachment->PlayFiringEffects();
		}
		else
		{
			WeaponAttachment->StopFiringEffects();
		}
	}
}

void AUTCharacter::AddAmmo(const FStoredAmmo& AmmoToAdd)
{
	AUTWeapon* ExistingWeapon = FindInventoryType<AUTWeapon>(AmmoToAdd.Type, true);
	if (ExistingWeapon != NULL)
	{
		ExistingWeapon->AddAmmo(AmmoToAdd.Amount);
	}
	else
	{
		for (int32 i = 0; i < SavedAmmo.Num(); i++)
		{
			if (SavedAmmo[i].Type == AmmoToAdd.Type)
			{
				// note that we're not clamping to max here, the weapon does so when the ammo is applied to it
				SavedAmmo[i].Amount += AmmoToAdd.Amount;
				if (SavedAmmo[i].Amount <= 0)
				{
					SavedAmmo.RemoveAt(i);
				}
				return;
			}
		}
		if (AmmoToAdd.Amount > 0)
		{
			new(SavedAmmo)FStoredAmmo(AmmoToAdd);
		}
	}
}

bool AUTCharacter::HasMaxAmmo(TSubclassOf<AUTWeapon> Type)
{
	if (Type != NULL)
	{
		int32 Amount = 0;
		for (int32 i = 0; i < SavedAmmo.Num(); i++)
		{
			if (SavedAmmo[i].Type == Type)
			{
				Amount += SavedAmmo[i].Amount;
			}
		}
		AUTWeapon* Weapon = FindInventoryType<AUTWeapon>(Type, true);
		if (Weapon != NULL)
		{
			Amount += Weapon->Ammo;
			return Amount >= Weapon->MaxAmmo;
		}
		else
		{
			return Amount >= Type.GetDefaultObject()->MaxAmmo;
		}
	}
	else
	{
		return true; // kinda arbitrary but this will make pickups more obviously broken
	}
}

void AUTCharacter::AllAmmo()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
		if (Weap)
		{
			Weap->AddAmmo(Weap->MaxAmmo);
		}
	}
#endif
}

void AUTCharacter::UnlimitedAmmo()
{
	bUnlimitedAmmo = !bUnlimitedAmmo;
}

AUTInventory* AUTCharacter::K2_FindInventoryType(TSubclassOf<AUTInventory> Type, bool bExactClass)
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (bExactClass ? (Inv->GetClass() == Type) : Inv->IsA(Type))
		{
			return Inv;
		}
	}
	return NULL;
}

void AUTCharacter::AddInventory(AUTInventory* InvToAdd, bool bAutoActivate)
{
	if (InvToAdd != NULL)
	{
		if (InventoryList == NULL)
		{
			InventoryList = InvToAdd;
		}
		else if (InventoryList == InvToAdd)
		{
			UE_LOG(UT, Warning, TEXT("AddInventory: %s already in %s's inventory!"), *InvToAdd->GetName(), *GetName());
		}
		else
		{
			AUTInventory* Last = InventoryList;
			while (Last->NextInventory != NULL)
			{
				// avoid recursion
				if (Last->NextInventory == InvToAdd)
				{
					UE_LOG(UT, Warning, TEXT("AddInventory: %s already in %s's inventory!"), *InvToAdd->GetName(), *GetName());
					return;
				}
				Last = Last->NextInventory;
			}
			Last->NextInventory = InvToAdd;
		}
		InvToAdd->GivenTo(this, bAutoActivate);
	}
}

void AUTCharacter::RemoveInventory(AUTInventory* InvToRemove)
{
	if (InvToRemove != NULL && InventoryList != NULL)
	{
		if (InvToRemove == InventoryList)
		{
			InventoryList = InventoryList->NextInventory;
		}
		else
		{
			for (AUTInventory* TestInv = InventoryList; TestInv->NextInventory != NULL; TestInv = TestInv->NextInventory)
			{
				if (TestInv->NextInventory == InvToRemove)
				{
					TestInv->NextInventory = InvToRemove->NextInventory;
					break;
				}
			}
		}
		if (InvToRemove == PendingWeapon)
		{
			PendingWeapon = NULL;
		}
		else if (InvToRemove == Weapon)
		{
			Weapon = NULL;
			if (PendingWeapon != NULL)
			{
				WeaponChanged();
			}
			else
			{
				WeaponClass = NULL;
				UpdateWeaponAttachment();
			}
			if (!bTearOff)
			{
				if (IsLocallyControlled())
				{
					SwitchToBestWeapon();
				}
				else
				{
					ClientWeaponLost((AUTWeapon*)InvToRemove);
				}
			}
		}
		InvToRemove->Removed();
	}
}

bool AUTCharacter::IsInInventory(const AUTInventory* TestInv) const
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv == TestInv)
		{
			return true;
		}
	}

	return false;
}

void AUTCharacter::TossInventory(AUTInventory* InvToToss, FVector ExtraVelocity)
{
	if (InvToToss == NULL)
	{
		UE_LOG(UT, Warning, TEXT("TossInventory(): InvToToss == NULL"));
	}
	else if (!IsInInventory(InvToToss))
	{
		UE_LOG(UT, Warning, TEXT("Attempt to remove %s which is not in %s's inventory!"), *InvToToss->GetName(), *GetName());
	}
	else
	{
		InvToToss->DropFrom(GetActorLocation() + GetActorRotation().Vector() * GetSimpleCollisionCylinderExtent().X, GetVelocity() + GetActorRotation().RotateVector(ExtraVelocity + FVector(300.0f, 0.0f, 150.0f)));
	}
}

void AUTCharacter::DiscardAllInventory()
{
	AUTInventory* Inv = InventoryList;
	while (Inv != NULL)
	{
		AUTInventory* NextInv = Inv->GetNext();
		Inv->Destroy();
		Inv = NextInv;
	}
}

void AUTCharacter::InventoryEvent(FName EventName)
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv->bCallOwnerEvent)
		{
			Inv->OwnerEvent(EventName);
		}
	}
}

void AUTCharacter::SwitchWeapon(AUTWeapon* NewWeapon)
{
	if (NewWeapon != NULL && !IsDead())
	{
		if (Role <= ROLE_SimulatedProxy)
		{
			UE_LOG(UT, Warning, TEXT("Illegal SwitchWeapon() call on remote client"));
		}
		else if (NewWeapon->Instigator != this || !IsInInventory(NewWeapon))
		{
			UE_LOG(UT, Warning, TEXT("Weapon %s is not owned by self"), *GetNameSafe(NewWeapon));
		}
		else if (Role == ROLE_Authority)
		{
			ClientSwitchWeapon(NewWeapon);
			// NOTE: we don't call LocalSwitchWeapon() here; we ping back from the client so all weapon switches are lead by the client
			//		otherwise, we get inconsistencies if both sides trigger weapon changes
		}
		else
		{
			LocalSwitchWeapon(NewWeapon);
			ServerSwitchWeapon(NewWeapon);
		}
	}
}

void AUTCharacter::LocalSwitchWeapon(AUTWeapon* NewWeapon)
{
	// make sure clients don't try to switch to weapons that haven't been fully replicated/initialized
	if (NewWeapon != NULL && NewWeapon->GetUTOwner() == NULL)
	{
		ClientSwitchWeapon(Weapon);
	}
	else
	{
		if (Weapon == NULL)
		{
			if (NewWeapon != NULL)
			{
				// initial equip
				PendingWeapon = NewWeapon;
				WeaponChanged();
			}
		}
		else if (NewWeapon != NULL)
		{
			if (NewWeapon != Weapon)
			{
				if (Weapon->PutDown())
				{
					// standard weapon switch to some other weapon
					PendingWeapon = NewWeapon;
				}
			}
			else if (PendingWeapon != NULL)
			{
				// switching back to weapon that was on its way down
				PendingWeapon = NULL;
				Weapon->BringUp();
			}
		}
		else if (Weapon != NULL && PendingWeapon != NULL && PendingWeapon->PutDown())
		{
			// stopping weapon switch in progress by passing NULL
			PendingWeapon = NULL;
			Weapon->BringUp();
		}
	}
}

void AUTCharacter::ClientSwitchWeapon_Implementation(AUTWeapon* NewWeapon)
{
	LocalSwitchWeapon(NewWeapon);
	if (Role < ROLE_Authority)
	{
		ServerSwitchWeapon(NewWeapon);
	}
}

void AUTCharacter::ServerSwitchWeapon_Implementation(AUTWeapon* NewWeapon)
{
	if (NewWeapon != NULL)
	{
		LocalSwitchWeapon(NewWeapon);
	}
}
bool AUTCharacter::ServerSwitchWeapon_Validate(AUTWeapon* NewWeapon)
{
	return true;
}

void AUTCharacter::WeaponChanged()
{
	if (PendingWeapon != NULL)
	{
		checkSlow(IsInInventory(PendingWeapon));
		Weapon = PendingWeapon;
		WeaponClass = Weapon->GetClass();
		UpdateWeaponAttachment();
		PendingWeapon = NULL;
		Weapon->BringUp();
	}
	else if (Weapon != NULL)
	{
		// restore current weapon since pending became invalid
		Weapon->BringUp();
	}
}

void AUTCharacter::ClientWeaponLost_Implementation(AUTWeapon* LostWeapon)
{
	if (IsLocallyControlled())
	{
		if (Weapon == LostWeapon)
		{
			Weapon = NULL;
			if (!IsDead())
			{
				if (PendingWeapon == NULL)
				{
					SwitchToBestWeapon();
				}
				if (PendingWeapon != NULL)
				{
					WeaponChanged();
				}
			}
		}
		else if (PendingWeapon == LostWeapon)
		{
			PendingWeapon = NULL;
			WeaponChanged();
		}
	}
}

void AUTCharacter::SwitchToBestWeapon()
{
	if (IsLocallyControlled())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			PC->SwitchToBestWeapon();
		}
		// TODO:  bots
	}
}

void AUTCharacter::UpdateWeaponAttachment()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		TSubclassOf<AUTWeaponAttachment> NewAttachmentClass = (WeaponClass != NULL) ? WeaponClass.GetDefaultObject()->AttachmentType : NULL;
		if (WeaponAttachment != NULL && (NewAttachmentClass == NULL || (WeaponAttachment != NULL && WeaponAttachment->GetClass() != NewAttachmentClass)))
		{
			WeaponAttachment->Destroy();
			WeaponAttachment = NULL;
		}
		if (WeaponAttachment == NULL && NewAttachmentClass != NULL)
		{
			FActorSpawnParameters Params;
			Params.Instigator = this;
			Params.Owner = this;
			WeaponAttachment = GetWorld()->SpawnActor<AUTWeaponAttachment>(NewAttachmentClass, Params);
		}
	}
}

float AUTCharacter::GetFireRateMultiplier()
{
	return FMath::Max<float>(FireRateMultiplier, 0.1f);
}
void AUTCharacter::SetFireRateMultiplier(float InMult)
{
	FireRateMultiplier = InMult;
	FireRateChanged();
}
void AUTCharacter::FireRateChanged()
{
	if (Weapon != NULL)
	{
		Weapon->UpdateTiming();
	}
}

void AUTCharacter::OnRep_ReplicatedMovement()
{
	if (bTearOff && (RootComponent == NULL || !RootComponent->IsSimulatingPhysics()))
	{
		bDeferredReplicatedMovement = true;
	}
	else
	{
		Super::OnRep_ReplicatedMovement();
	}
}

void AUTCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, LastTakeHitInfo, GetWorld()->TimeSeconds - LastTakeHitTime < 1.0f);
}

void AUTCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTCharacter, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, InventoryList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, LastTakeHitInfo, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, DamageScaling, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireRateMultiplier, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, AmbientSound, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CharOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ReplicatedBodyMaterial, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadScale, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bUnlimitedAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, bFeigningDeath, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bRepDodgeRolling, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bSpawnProtectionEligible, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, EmoteReplicationInfo, COND_None);
}

void AUTCharacter::AddDefaultInventory(TArray<TSubclassOf<AUTInventory>> DefaultInventoryToAdd)
{
	// Add the default character inventory
	for (int i=0;i<DefaultCharacterInventory.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultCharacterInventory[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}

	// Add the default inventory passed in from the game
	for (int i=0;i<DefaultInventoryToAdd.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultInventoryToAdd[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}
}

bool AUTCharacter::CanDodge() const
{
	return CanDodgeInternal();
}

bool AUTCharacter::CanDodgeInternal_Implementation() const
{
	return !bIsCrouched && UTCharacterMovement && UTCharacterMovement->CanDodge() && (UTCharacterMovement->Velocity.Z > -1.f * MaxSafeFallSpeed);
}

bool AUTCharacter::Dodge(FVector DodgeDir, FVector DodgeCross)
{
	if (CanDodge())
	{
		if ( DodgeOverride(DodgeDir, DodgeCross) )
		{
			// blueprint handled dodge attempt
			return true;
		}
		if (UTCharacterMovement && UTCharacterMovement->PerformDodge(DodgeDir, DodgeCross))
		{
			bCanPlayWallHitSound = true;
			OnDodge(DodgeDir);
			return true;
		}
	}
	return false;
}

bool AUTCharacter::CanJumpInternal_Implementation() const
{
	return !bIsCrouched && UTCharacterMovement && UTCharacterMovement->CanJump();
}

void AUTCharacter::CheckJumpInput(float DeltaTime)
{
	if (CharacterMovement && ((CharacterMovement->MovementMode == MOVE_Flying) || (CharacterMovement->MovementMode == MOVE_Swimming)))
	{
		// No jump when swimming or flying
	}
	else if (bPressedJump)
	{
		DoJump(bClientUpdating);
	}
	else
	{
		if (UTCharacterMovement->bPressedDodgeForward || UTCharacterMovement->bPressedDodgeBack || UTCharacterMovement->bPressedDodgeLeft || UTCharacterMovement->bPressedDodgeRight)
		{
			float DodgeDirX = UTCharacterMovement->bPressedDodgeForward ? 1.f : (UTCharacterMovement->bPressedDodgeBack ? -1.f : 0.f);
			float DodgeDirY = UTCharacterMovement->bPressedDodgeLeft ? -1.f : (UTCharacterMovement->bPressedDodgeRight ? 1.f : 0.f);
			float DodgeCrossX = (UTCharacterMovement->bPressedDodgeLeft || UTCharacterMovement->bPressedDodgeRight) ? 1.f : 0.f;
			float DodgeCrossY = (UTCharacterMovement->bPressedDodgeForward || UTCharacterMovement->bPressedDodgeBack) ? 1.f : 0.f;
			FRotator TurnRot(0.f, GetActorRotation().Yaw, 0.f);
			FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
			FVector X = TurnRotMatrix.GetScaledAxis(EAxis::X);
			FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);
			Dodge(DodgeDirX*X + DodgeDirY*Y, DodgeCrossX*X + DodgeCrossY*Y);
		}
	}
}

void AUTCharacter::ClearJumpInput()
{
	Super::ClearJumpInput();
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ClearJumpInput();
	}
}

void AUTCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = GetControlRotation();
		// @TODO FIXMESTEVE need CharacterMovement method that retursns if forward is full freedom or horizontal only
		FRotator YawRotation = (CharacterMovement && ((CharacterMovement->MovementMode == MOVE_Flying) || (CharacterMovement->MovementMode == MOVE_Swimming))) ? Rotation : FRotator(0, Rotation.Yaw, 0);

		// add movement in forward direction
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AUTCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is right
		const FRotator Rotation = GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// add movement in right direction
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}

void AUTCharacter::MoveUp(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in up direction
		AddMovementInput(FVector(0.f,0.f,1.f), Value);
	}
}

void AUTCharacter::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	if (UTCharacterMovement)
	{
		int32 DodgeFlags = (Flags >> 2) & 7;
		UTCharacterMovement->bPressedDodgeForward = (DodgeFlags == 1);
		UTCharacterMovement->bPressedDodgeBack = (DodgeFlags == 2);
		UTCharacterMovement->bPressedDodgeLeft = (DodgeFlags == 3);
		UTCharacterMovement->bPressedDodgeRight = (DodgeFlags == 4);
		UTCharacterMovement->bIsSprinting = (DodgeFlags == 5);
		UTCharacterMovement->bIsDodgeRolling = (DodgeFlags == 6);
		bool bOldWillDodgeRoll = UTCharacterMovement->bWillDodgeRoll;
		UTCharacterMovement->bWillDodgeRoll = (DodgeFlags == 7);
		if (!bOldWillDodgeRoll && UTCharacterMovement->bWillDodgeRoll)
		{
			UTCharacterMovement->DodgeRollTapTime = UTCharacterMovement->GetCurrentMovementTime();
		}
		bRepDodgeRolling = UTCharacterMovement->bIsDodgeRolling;
	}
}

APlayerCameraManager* AUTCharacter::GetPlayerCameraManager()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
	return PC != NULL ? PC->PlayerCameraManager : NULL;
}

void AUTCharacter::PlayFootstep(uint8 FootNum)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), FootstepSound, this, SRT_IfSourceNotReplicated);
	LastFoot = FootNum;
	LastFootstepTime = GetWorld()->TimeSeconds;
}

FVector AUTCharacter::GetPawnViewLocation() const
{
	float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
	return GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + EyeOffsetGlobalScaling*EyeOffset;
}

void AUTCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bFindCameraComponentWhenViewTarget && CharacterCameraComponent && CharacterCameraComponent->bIsActive)
	{
		CharacterCameraComponent->GetCameraView(DeltaTime, OutResult);
		float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
		OutResult.Location = OutResult.Location + CrouchEyeOffset + EyeOffsetGlobalScaling*EyeOffset;
		return;
	}

	GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
}

void AUTCharacter::PlayJump()
{
	DesiredJumpBob = WeaponJumpBob;
	TargetEyeOffset.Z = EyeOffsetJumpBob;
	UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, this, SRT_AllButOwner);
}

void AUTCharacter::OnDodge_Implementation(const FVector &DodgeDir)
{
	FRotator TurnRot(0.f, GetActorRotation().Yaw, 0.f);
	FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
	FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);

	DesiredJumpBob = WeaponDodgeBob;
	if ((Y | DodgeDir) > 0.6f)
	{
		DesiredJumpBob.Y *= -1.f;
	}
	else if ((Y | DodgeDir) > -0.6f)
	{
		DesiredJumpBob.Y = 0.f;
	}
	UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeSound, this, SRT_AllButOwner);
}

void AUTCharacter::Landed(const FHitResult& Hit)
{
	// cause crushing damage if we fell on another character
	if (Cast<AUTCharacter>(Hit.Actor.Get()) != NULL)
	{
		float Damage = CrushingDamageFactor * CharacterMovement->Velocity.Z / -100.0f;
		if (Damage >= 1.0f)
		{
			FUTPointDamageEvent DamageEvent(Damage, Hit, -CharacterMovement->Velocity.SafeNormal(), UUTDmgType_FallingCrush::StaticClass());
			Hit.Actor->TakeDamage(Damage, DamageEvent, Controller, this);
		}
	}

	bCanPlayWallHitSound = true;
	if (Role == ROLE_Authority)
	{
		MakeNoise(FMath::Clamp<float>(CharacterMovement->Velocity.Z / (MaxSafeFallSpeed * -0.5f), 0.0f, 1.0f));
	}
	DesiredJumpBob = FVector(0.f);

	// bob weapon and viewpoint on landing
	if (CharacterMovement->Velocity.Z < WeaponLandBobThreshold)
	{
		DesiredJumpBob = WeaponLandBob* FMath::Min(1.f, (-1.f*CharacterMovement->Velocity.Z - WeaponLandBobThreshold) / FullWeaponLandBobVelZ);
	}
	if (CharacterMovement->Velocity.Z <= EyeOffsetLandBobThreshold)
	{
		TargetEyeOffset.Z = EyeOffsetLandBob * FMath::Min(1.f, (-1.f*CharacterMovement->Velocity.Z - (0.8f*EyeOffsetLandBobThreshold)) / FullEyeOffsetLandBobVelZ);
	}
	UTCharacterMovement->OldZ = GetActorLocation().Z;

	TakeFallingDamage(Hit);

	Super::Landed(Hit);

	static FName NAME_Landed(TEXT("Landed"));
	InventoryEvent(NAME_Landed);

	LastHitBy = NULL;

	if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeRollSound, this, SRT_None);
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), LandingSound, this, SRT_None);
	}
}

void AUTCharacter::MoveBlockedBy(const FHitResult& Impact) 
{
	if (CharacterMovement && (CharacterMovement->MovementMode == MOVE_Falling) && (GetWorld()->GetTimeSeconds() - LastWallHitNotifyTime > 0.5f))
	{
		if (Impact.ImpactNormal.Z > 0.4f)
		{
			TakeFallingDamage(Impact);
		}
		if ( bCanPlayWallHitSound && CharacterMovement->Velocity.SizeSquared() > FMath::Square(0.9f*CharacterMovement->MaxWalkSpeed))
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), WallHitSound, this, SRT_None);
		}
		LastWallHitNotifyTime = GetWorld()->GetTimeSeconds();
	}
}

void AUTCharacter::TakeFallingDamage(const FHitResult& Hit)
{
	if (Role == ROLE_Authority)
	{
		float FallingSpeed = CharacterMovement->Velocity.Z;
		if (FallingSpeed < -1.f * MaxSafeFallSpeed && !HandleFallingDamage(FallingSpeed, Hit))
		{
			/* TODO: water
			if (IsTouchingWaterVolume())
			{
				FallingSpeed += 100.f;
			}*/
			if (FallingSpeed < -1.f * MaxSafeFallSpeed)
			{
				float FallingDamage = -100.f * (FallingSpeed + MaxSafeFallSpeed) / MaxSafeFallSpeed;
				if (UTCharacterMovement)
				{
					FallingDamage -= UTCharacterMovement->FallingDamageReduction(FallingDamage, Hit);
				}
				if (FallingDamage >= 1.0f)
				{
					FUTPointDamageEvent DamageEvent(FallingDamage, Hit, CharacterMovement->Velocity.SafeNormal(), UUTDmgType_Fell::StaticClass());
					TakeDamage(DamageEvent.Damage, DamageEvent, Controller, this);
				}
			}
		}
	}
}

void AUTCharacter::SetCharacterOverlay(UMaterialInterface* NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayMaterial(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay material %s was not registered"), *NewOverlay->GetFullName());
			}
			else
			{
				checkSlow(Index < sizeof(CharOverlayFlags * 8));
				if (bEnabled)
				{
					CharOverlayFlags |= (1 << Index);
				}
				else
				{
					CharOverlayFlags &= ~(1 << Index);
				}
				if (GetNetMode() != NM_DedicatedServer)
				{
					UpdateCharOverlays();
				}
			}
		}
	}
}
void AUTCharacter::SetWeaponOverlay(UMaterialInterface* NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayMaterial(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay material %s was not registered"), *NewOverlay->GetFullName());
			}
			else
			{
				checkSlow(Index < sizeof(WeaponOverlayFlags * 8));
				if (bEnabled)
				{
					WeaponOverlayFlags |= (1 << Index);
				}
				else
				{
					WeaponOverlayFlags &= ~(1 << Index);
				}
				if (GetNetMode() != NM_DedicatedServer)
				{
					UpdateWeaponOverlays();
				}
			}
		}
	}
}

void AUTCharacter::UpdateCharOverlays()
{
	if (CharOverlayFlags == 0)
	{
		if (OverlayMesh != NULL && OverlayMesh->IsRegistered())
		{
			OverlayMesh->DetachFromParent();
			OverlayMesh->UnregisterComponent();
		}
	}
	else
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			if (OverlayMesh == NULL)
			{
				OverlayMesh = ConstructObject<USkeletalMeshComponent>(Mesh->GetClass(), this, NAME_None, RF_NoFlags, Mesh, true);
				OverlayMesh->AttachParent = NULL; // this gets copied but we don't want it to be
				OverlayMesh->SetMasterPoseComponent(Mesh);
			}
			if (!OverlayMesh->IsRegistered())
			{
				OverlayMesh->RegisterComponent();
				OverlayMesh->AttachTo(Mesh, NAME_None, EAttachLocation::SnapToTarget);
				OverlayMesh->SetRelativeScale3D(FVector(1.015f, 1.015f, 1.015f));
			}
			UMaterialInterface* FirstOverlay = GS->GetFirstOverlay(CharOverlayFlags);
			// apply team color, if applicable
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FirstOverlay, OverlayMesh);
				static FName NAME_TeamColor(TEXT("TeamColor"));
				MID->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
				FirstOverlay = MID;
			}
			for (int32 i = 0; i < OverlayMesh->GetNumMaterials(); i++)
			{
				OverlayMesh->SetMaterial(i, FirstOverlay);
			}
		}
	}
}
void AUTCharacter::UpdateWeaponOverlays()
{
	if (Weapon != NULL)
	{
		Weapon->UpdateOverlays();
	}
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->UpdateOverlays();
	}
}

void AUTCharacter::SetSkin(UMaterialInterface* NewSkin)
{
	ReplicatedBodyMaterial = NewSkin;
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateSkin();
	}
}
void AUTCharacter::UpdateSkin()
{
	if (ReplicatedBodyMaterial != NULL)
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, ReplicatedBodyMaterial);
		}
	}
	else
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->GetMaterial(i));
		}
	}
	if (Weapon != NULL)
	{
		Weapon->SetSkin(ReplicatedBodyMaterial);
	}
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->SetSkin(ReplicatedBodyMaterial);
	}
}

void AUTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Mesh->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered && !Mesh->bRecentlyRendered && !IsLocallyControlled())
	{
		// TODO: currently using an arbitrary made up interval and scale factor
		// TODO: for local player in first person, sync to view bob, etc
		float Speed = CharacterMovement->Velocity.Size();
		if (CharacterMovement->MovementMode == MOVE_Walking && Speed > 0.0f && GetWorld()->TimeSeconds - LastFootstepTime > 0.35f * CharacterMovement->MaxWalkSpeed / Speed)
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}

	if (BodyMI != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			static FName NAME_SpawnProtectionPct(TEXT("SpawnProtectionPct"));
			BodyMI->SetScalarParameterValue(NAME_SpawnProtectionPct, (bSpawnProtectionEligible && GS->SpawnProtectionTime > 0.0f) ? FMath::Max(0.0f, 1.0f - (GetWorld()->TimeSeconds - CreationTime) / GS->SpawnProtectionTime) : 0.0f);
		}
	}

	// tick ragdoll recovery
	if (!bFeigningDeath && bInRagdollRecovery)
	{
		// TODO: anim check?
		if (Mesh->Bodies.Num() == 0)
		{
			bInRagdollRecovery = false;
		}
		else
		{
			Mesh->SetAllBodiesPhysicsBlendWeight(FMath::Max(0.0f, Mesh->Bodies[0]->PhysicsBlendWeight - (1.0f / FMath::Max(0.01f, RagdollBlendOutTime)) * DeltaTime));
			if (Mesh->Bodies[0]->PhysicsBlendWeight == 0.0f)
			{
				bInRagdollRecovery = false;
			}
		}
		if (!bInRagdollRecovery)
		{
			// disable physics and make sure mesh is in the proper position
			Mesh->SetSimulatePhysics(false);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->AttachTo(CapsuleComponent, NAME_None, EAttachLocation::SnapToTarget);
			Mesh->SetRelativeLocation(GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->RelativeLocation);
			Mesh->SetRelativeRotation(GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->RelativeRotation);
			Mesh->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->RelativeScale3D);

			bDisallowWeaponFiring = GetClass()->GetDefaultObject<AUTCharacter>()->bDisallowWeaponFiring;

			// switch back to first person view
			// TODO: remember prior setting?
			AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
			if (PC != NULL)
			{
				PC->BehindView(false);
			}
		}
	}

	// update eyeoffset 
	if (CharacterMovement->MovementMode == MOVE_Walking && !MovementBaseUtility::UseRelativePosition(RelativeMovement.MovementBase))
	{
		// smooth up/down stairs
		EyeOffset.Z += (UTCharacterMovement->OldZ - GetActorLocation().Z);

		// avoid clipping
		if (CrouchEyeOffset.Z + EyeOffset.Z > CapsuleComponent->GetUnscaledCapsuleHalfHeight() - BaseEyeHeight - 12.f)
		{
			if (!IsLocallyControlled())
			{
				CrouchEyeOffset.Z = 0.f;
				EyeOffset.Z = FMath::Min(EyeOffset.Z, CapsuleComponent->GetUnscaledCapsuleHalfHeight() - BaseEyeHeight); // @TODO FIXMESTEVE CONSIDER CLIP PLANE -12.f);
			}
			else
			{
				// trace and see if camera will clip
				static FName CameraClipTrace = FName(TEXT("CameraClipTrace"));
				FCollisionQueryParams Params(CameraClipTrace, false, this);
				FHitResult Hit;
				if (GetWorld()->SweepSingle(Hit, GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight), GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + EyeOffset, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
				{
					EyeOffset.Z = Hit.Location.Z;
				}
			}
		}
	}

	// decay offset
	float InterpTime = FMath::Min(1.f, EyeOffsetInterpRate*DeltaTime);
	EyeOffset = (1.f - InterpTime)*EyeOffset + InterpTime*TargetEyeOffset;
	float CrouchInterpTime = FMath::Min(1.f, CrouchEyeOffsetInterpRate*DeltaTime);
	CrouchEyeOffset = (1.f - CrouchInterpTime)*CrouchEyeOffset;
	if (EyeOffset.Z > 0.f)
	{
		// faster decay if positive
		EyeOffset.Z = (1.f - InterpTime)*EyeOffset.Z + InterpTime*TargetEyeOffset.Z;
	}
	TargetEyeOffset *= FMath::Max(0.f, 1.f - EyeOffsetDecayRate*DeltaTime);

	if (IsLocallyControlled() && CharacterMovement) // @TODO FIXME ALSO FOR SPECTATORS
	{
		// @TODO FIXMESTEVE this should all be event driven
		if (CharacterMovement->IsFalling() && (CharacterMovement->Velocity.Z < FallingAmbientStartSpeed))
		{
			SetLocalAmbientSound(FallingAmbientSound, false);
		}
		else
		{
			SetLocalAmbientSound(FallingAmbientSound, true);
			if (CharacterMovement->IsMovingOnGround())
			{
				if (CharacterMovement->Velocity.Size2D() > SprintAmbientStartSpeed)
				{
					SetLocalAmbientSound(SprintAmbientSound, false);
				}
				else if (CharacterMovement->Velocity.Size2D() < SprintAmbientStopSpeed)
				{
					SetLocalAmbientSound(SprintAmbientSound, true);
				}
			}
			else
			{
				SetLocalAmbientSound(SprintAmbientSound, true);
			}
		}
	}
	/*
	if (CharacterMovement && ((CharacterMovement->GetCurrentAcceleration() | CharacterMovement->Velocity) < 0.f))
	{
		UE_LOG(UT, Warning, TEXT("Position %f %f time %f"),GetActorLocation().X, GetActorLocation().Y, GetWorld()->GetTimeSeconds());
	}*/
}

float SprintAmbientStartSpeed;

/** Running speed to stop sprint sound */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
float SprintAmbientStopSpeed;


void AUTCharacter::BecomeViewTarget(class APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	if (PC != NULL)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
		if (UTPC != NULL && UTPC->IsLocalController() && UTPC->GetUTCharacter() == this)
		{
			SetMeshVisibility(UTPC->IsBehindView());
			return;
		}
	}

	SetMeshVisibility(true);
}

void AUTCharacter::EndViewTarget( class APlayerController* PC )
{
	Super::EndViewTarget(PC);
	if (PC != NULL && (PC->Player == NULL || PC->IsLocalController()))
	{
		SetMeshVisibility(true);
	}
}

uint8 AUTCharacter::GetTeamNum() const
{
	const IUTTeamInterface* TeamInterface = InterfaceCast<IUTTeamInterface>(Controller);
	if (TeamInterface != NULL)
	{
		return TeamInterface->GetTeamNum();
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
	}
}

FLinearColor AUTCharacter::GetTeamColor() const
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->Team != NULL)
	{
		return PS->Team->TeamColor;
	}
	return FLinearColor::White;
}

void AUTCharacter::PossessedBy(AController* NewController)
{
	// TODO: shouldn't base class do this? APawn::Unpossessed() still does SetOwner(NULL)...
	SetOwner(NewController);

	Super::PossessedBy(NewController);
	NotifyTeamChanged();
}

void AUTCharacter::UnPossessed()
{
	ClearPendingFire();

	Super::UnPossessed();
}

void AUTCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (PlayerState != NULL)
	{
		NotifyTeamChanged();
	}
}

void AUTCharacter::NotifyTeamChanged()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && BodyMI != NULL)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		if (PS->Team != NULL)
		{
			BodyMI->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
		}
		else
		{
			// in FFA games, let the local player decide the team coloring
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
				if (PC != NULL && PC->FFAPlayerColor.A > 0.0f)
				{
					BodyMI->SetVectorParameterValue(NAME_TeamColor, PC->FFAPlayerColor);
					// NOTE: no splitscreen support, first player wins
					break;
				}
			}
		}
	}
}

void AUTCharacter::PlayerChangedTeam()
{
	PlayerSuicide();
}

void AUTCharacter::PlayerSuicide()
{
	if (Role == ROLE_Authority)
	{
		FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
		FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), UUTDmgType_Suicide::StaticClass());
		Died(NULL, FakeDamageEvent);
	}
}

bool AUTCharacter::CanPickupObject(AUTCarriedObject* PendingObject)
{
	return GetCarriedObject() == NULL && Controller != NULL && !bTearOff && !IsDead();
}

AUTCarriedObject* AUTCharacter::GetCarriedObject()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->CarriedObject != NULL)
	{
		return PS->CarriedObject;
	}
	return NULL;
}


/** This is only here for legacy. */
void AUTCharacter::DropFlag()
{
	DropCarriedObject();
}

void AUTCharacter::DropCarriedObject()
{
	ServerDropCarriedObject();
}


void AUTCharacter::ServerDropCarriedObject_Implementation()
{
	AUTCarriedObject* Obj = GetCarriedObject();
	if (Obj != NULL)
	{
		Obj->Drop(NULL);
	}
}

bool AUTCharacter::ServerDropCarriedObject_Validate()
{
	return true;
}

void AUTCharacter::UseCarriedObject()
{
	ServerUseCarriedObject();
}


void AUTCharacter::ServerUseCarriedObject_Implementation()
{
	AUTCarriedObject* Obj = GetCarriedObject();
	if (Obj != NULL)
	{
		Obj->Use();
	}
}

bool AUTCharacter::ServerUseCarriedObject_Validate()
{
	return true;
}

void AUTCharacter::ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
{
	UE_LOG(UT, Warning, TEXT("Use TakeDamage() instead"));
	checkSlow(false);
}

void AUTCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (IsDead())
	{
		Super::FellOutOfWorld(DmgType);
	}
	else
	{
		Died(NULL, FUTPointDamageEvent(1000.0f, FHitResult(this, CapsuleComponent, GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), DmgType.GetClass()));
	}
}

bool AUTCharacter::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	// during teleportation, we need to change our collision to overlap potential telefrag targets instead of block
	// however, EncroachingBlockingGeometry() doesn't handle reflexivity correctly so we can't get anywhere changing our collision responses
	// instead, we must change our object type to adjust the query

	ECollisionChannel SavedObjectType = CapsuleComponent->GetCollisionObjectType();
	CapsuleComponent->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
	bool bResult = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	CapsuleComponent->SetCollisionObjectType(SavedObjectType);
	CapsuleComponent->UpdateOverlaps(); // make sure collision object type changes didn't mess with our overlaps
	// TODO: set bJustTeleported here?
	return bResult;
}
void AUTCharacter::OnOverlapBegin(AActor* OtherActor)
{
	if (Role == ROLE_Authority && OtherActor != this && CapsuleComponent->GetCollisionObjectType() == COLLISION_TELEPORTING_OBJECT) // need to make sure this ISN'T reflexive, only teleporting Pawn should be checking for telefrags
	{
		AUTCharacter* OtherC = Cast<AUTCharacter>(OtherActor);
		if (OtherC != NULL)
		{
			AUTTeamGameMode* TeamGame = GetWorld()->GetAuthGameMode<AUTTeamGameMode>();
			if (TeamGame == NULL || TeamGame->TeamDamagePct > 0.0f || !GetWorld()->GetGameState<AUTGameState>()->OnSameTeam(OtherC, this))
			{
				FUTPointDamageEvent DamageEvent(100000.0f, FHitResult(this, CapsuleComponent, GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Telefragged::StaticClass());
				OtherC->Died(Controller, DamageEvent);
			}
		}
		// TODO: if OtherActor is a vehicle, then we should be killed instead
	}
}

void AUTCharacter::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if ( UTPS != NULL && UTPS->Team != NULL && PC != NULL && PC->GetPawn() != NULL && PC->GetViewTarget() != this && GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f &&
		FVector::DotProduct(CameraDir, (GetActorLocation() - CameraPosition)) > 0.0f && GS != NULL && GS->OnSameTeam(PC->GetPawn(), this) )
	{
		float Dist = (CameraPosition - GetActorLocation()).Size();
		if (Dist <= PlayerIndicatorMaxDistance)
		{
			float XL,YL;

			float Scale = Canvas->ClipX / 1920;

			UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;
			Canvas->TextSize(TinyFont, PlayerState->PlayerName, XL, YL,Scale,Scale);

			FVector ScreenPosition = Canvas->Project(GetActorLocation() + (CapsuleComponent->GetUnscaledCapsuleHalfHeight() * 1.25f) * FVector(0,0,1));

			// Make the team backgrounds darker
			FLinearColor TeamColor = UTPS->Team->TeamColor;
			TeamColor.R *= 0.24;
			TeamColor.G *= 0.24;
			TeamColor.B *= 0.24;

			Canvas->SetLinearDrawColor(TeamColor);
			Canvas->DrawTile(Canvas->DefaultTexture, ScreenPosition.X - (XL * 0.5) - 1, ScreenPosition.Y - YL - 2, XL + 2, YL - (6 * Scale), 0,0, 1,1);
			Canvas->SetLinearDrawColor(FLinearColor::White);
			FFontRenderInfo FRI = Canvas->CreateFontRenderInfo(true, false);
			Canvas->DrawText(TinyFont, PlayerState->PlayerName, ScreenPosition.X - (XL * 0.5), ScreenPosition.Y - YL, Scale, Scale, FRI);
		}
	}
}

void AUTCharacter::OnRepEmote()
{
	PlayEmote(EmoteReplicationInfo.EmoteIndex);
}

void AUTCharacter::PlayEmote(int32 EmoteIndex)
{
	EmoteReplicationInfo.EmoteIndex = EmoteIndex;
	EmoteReplicationInfo.bNewData = !EmoteReplicationInfo.bNewData;

	if (EmoteAnimations.IsValidIndex(EmoteIndex) && EmoteAnimations[EmoteIndex] != nullptr && !bFeigningDeath)
	{
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(EmoteAnimations[EmoteIndex], 1.0f);
		}
	}
}


EAllowedSpecialMoveAnims AUTCharacter::AllowedSpecialMoveAnims()
{
	if (CharacterMovement != NULL && (!CharacterMovement->IsMovingOnGround() || !CharacterMovement->GetCurrentAcceleration().IsNearlyZero()))
	{
		return EASM_UpperBodyOnly;
	}

	return EASM_Any;
}